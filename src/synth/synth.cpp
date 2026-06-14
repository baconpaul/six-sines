/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024-2025, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#include "synth/synth.h"
#include "sst/cpputils/constructors.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/PanLaws.h"

#include "tinyxml/tinyxml.h"
#include "sst/plugininfra/paths.h"

#include "libMTSClient.h"

namespace baconpaul::six_sines
{

int debugLevel{0};

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

Synth::Synth(bool mo)
    : isMultiOut(mo), responder(*this), monoResponder(*this),
      voices(sst::cpputils::make_array<Voice, VMConfig::maxVoiceCount>(patch, monoValues))
{
    voiceManager = std::make_unique<voiceManager_t>(responder, monoResponder);
    monoValues.mtsClient = MTS_RegisterClient();

    for (int i = 0; i < numMacros; ++i)
    {
        monoValues.macroPtr[i] = &patch.macroNodes[i].level.value;
    }

    patch.dawExtraStateTo = [this](TiXmlElement &e) { toDawExtraState(e); };
    patch.dawExtraStateFrom = [this](TiXmlElement &e) { fromDawExtraState(e); };

    std::fill(lState.begin(), lState.end(), nullptr);
    std::fill(rState.begin(), rState.end(), nullptr);

    // User-defaults reader. Uses the same path/product name as the editor so both share the
    // one preferences file. Construction only reads it; nothing is written here.
    auto docPath = userDocumentsPath();
    SXSNLOG("Six Sines user documents path: '" << docPath.u8string() << "'");
    defaultsProvider = std::make_unique<ui::defaultsProvder_t>(
        docPath, "SixSinesUI", ui::defaultName,
        [](auto e, auto b) { SXSNLOG("[ERROR]" << e << " " << b); });

    // Seed the session (DawExtraState) and live mono values from saved defaults, falling back
    // to the struct defaults when a preference is absent. A fresh patch then starts from the
    // user's preferred MPE bend range and smoothing times.
    dawExtraState.mpeBendRange =
        defaultsProvider->getUserDefaultValue(ui::defaultMPEBend, dawExtraState.mpeBendRange);
    {
        auto ms = defaultsProvider->getUserDefaultValue(ui::defaultMIDISmoothing, std::string{});
        if (!ms.empty())
            dawExtraState.midiCCSmoothingTimeMs = std::stof(ms);
        auto ps = defaultsProvider->getUserDefaultValue(ui::defaultParamSmoothing, std::string{});
        if (!ps.empty())
            dawExtraState.paramAutomationSmoothingTimeMs = std::stof(ps);
    }
    monoValues.mpeBendRange = dawExtraState.mpeBendRange;
    monoValues.midiCCSmoothingTimeMs = dawExtraState.midiCCSmoothingTimeMs;
    monoValues.paramAutomationSmoothingTimeMs = dawExtraState.paramAutomationSmoothingTimeMs;

    reapplyControlSettings();
    resetSoloState();

    /*
     * Internal consistency checks
     */
    bool anyWarn{false};
    for (auto &s : monoValues.modMatrixConfig.sources)
    {
        if (s.id >= patch.output.modsource[0].meta.maxVal ||
            s.id < patch.output.modsource[0].meta.minVal)
        {
            SXSNLOG("WARNING: Source " << s.group << "/" << s.name << " with id " << s.id
                                       << " is outside of param range "
                                       << patch.output.modsource[0].meta.minVal << " to "
                                       << patch.output.modsource[0].meta.maxVal);
            anyWarn = true;
        }
    }
    if (anyWarn)
    {
        SXSNLOG("Six Sines will termiante");
        std::terminate();
    }
}

Synth::~Synth()
{
    if (monoValues.mtsClient)
    {
        MTS_DeregisterClient(monoValues.mtsClient);
    }

    for (auto ls : lState)
        if (ls)
            src_delete(ls);
    for (auto rs : rState)
        if (rs)
            src_delete(rs);
}

fs::path Synth::userDocumentsPath()
{
    try
    {
        // Prefer the legacy ~/Documents/SixSines folder if a prior install created it, so we
        // don't strand existing presets/themes/defaults.
        auto legacy = sst::plugininfra::paths::bestDocumentsFolderPathFor("SixSines");
        if (fs::exists(legacy))
            return legacy;

        // Otherwise use the vendored BaconPaul/SixSines location, creating it if needed.
        auto vendor =
            sst::plugininfra::paths::bestDocumentsVendorFolderPathFor("BaconPaul", "SixSines");
        if (!fs::exists(vendor))
            fs::create_directories(vendor);
        return vendor;
    }
    catch (const fs::filesystem_error &e)
    {
        SXSNLOG("Unable to resolve user documents path: " << e.what());
    }
    return {};
}

void Synth::toDawExtraState(TiXmlElement &e) const
{
    TiXmlElement cm("colorMap");
    if (!dawExtraState.colorMapXml.empty())
    {
        TiXmlText t(dawExtraState.colorMapXml);
        t.SetCDATA(true);
        cm.InsertEndChild(t);
    }
    e.InsertEndChild(cm);

    // Always emit current engine-instance MPE state; this is what flips 1.1→1.2 sessions
    // to "the new way" once a re-save has happened.
    TiXmlElement mpe("mpe");
    mpe.SetAttribute("active", monoValues.mpeActive ? 1 : 0);
    mpe.SetAttribute("bendRange", monoValues.mpeBendRange);
    e.InsertEndChild(mpe);

    // Engine-wide smoothing times (ms). Absent in pre-1.x saves; defaults in DawExtraState apply.
    TiXmlElement sm("smoothing");
    sm.SetDoubleAttribute("midiCCMs", dawExtraState.midiCCSmoothingTimeMs);
    sm.SetDoubleAttribute("paramAutomationMs", dawExtraState.paramAutomationSmoothingTimeMs);
    e.InsertEndChild(sm);
}

void Synth::fromDawExtraState(TiXmlElement &e, DawExtraState &out)
{
    out = DawExtraState{};
    auto *cm = e.FirstChildElement("colorMap");
    if (cm)
    {
        auto *n = cm->FirstChild();
        if (n)
        {
            auto *txt = n->ToText();
            if (txt && txt->Value())
                out.colorMapXml = txt->Value();
        }
    }

    // The mpe element is only present in 1.2+ saves. Flag drives the 1.1 fallback.
    auto *mpe = e.FirstChildElement("mpe");
    if (mpe)
    {
        out.mpeFromExtraState = true;
        int active{0};
        if (mpe->QueryIntAttribute("active", &active) == TIXML_SUCCESS)
            out.mpeActive = (active != 0);
        int bendRange{24};
        if (mpe->QueryIntAttribute("bendRange", &bendRange) == TIXML_SUCCESS)
            out.mpeBendRange = bendRange;
    }

    // Smoothing element absent in older saves; struct defaults stand.
    auto *sm = e.FirstChildElement("smoothing");
    if (sm)
    {
        double v{0.0};
        if (sm->QueryDoubleAttribute("midiCCMs", &v) == TIXML_SUCCESS)
            out.midiCCSmoothingTimeMs = (float)v;
        if (sm->QueryDoubleAttribute("paramAutomationMs", &v) == TIXML_SUCCESS)
            out.paramAutomationSmoothingTimeMs = (float)v;
    }
}

void Synth::setSampleRate(double sampleRate)
{
    auto ohsr = hostSampleRate;
    auto oesr = engineSampleRate;

    hostSampleRate = sampleRate;
    // Look for 44 variants
    bool is441{false};
    auto hsrBy441 = hostSampleRate / (44100 / 2);
    if (std::fabs(hsrBy441 - std::round(hsrBy441)) < 1e-3)
    {
        is441 = true;
    }

    double internalRate{0.f};
    double mul{0.f};
    switch (sampleRateStrategy)
    {
    case SR_110120:
    {
        mul = 2.5;
    }
    break;
    case SR_132144:
    {
        mul = 3.0;
    }
    break;
    case SR_176192:
    {
        mul = 4;
    }
    break;
    case SR_220240:
    {
        mul = 5;
    }
    break;
    }

    if (is441)
        internalRate = 44100 * mul;
    else
        internalRate = 48000 * mul;

    engineSampleRate = internalRate;

    monoValues.sr.setSampleRate(internalRate);

    lagHandler.setRate(60, blockSize, monoValues.sr.sampleRate);
    vuPeak.setSampleRate(monoValues.sr.sampleRate, 20);
    for (int i = 0; i < numOps; ++i)
    {
        opVuPeak[i].setSampleRate(monoValues.sr.sampleRate, 20);
    }
    sampleRateRatio = hostSampleRate / engineSampleRate;

    if (usesLanczos())
    {
        for (int i = 0; i < (isMultiOut ? (1 + numOps) : 1); ++i)
            resampler[i] = std::make_unique<resampler_t>((float)monoValues.sr.sampleRate,
                                                         (float)hostSampleRate);
    }
    else
    {
        auto mode = SRC_SINC_FASTEST;
        if (resamplerEngine == SRC_MEDIUM)
        {
            mode = SRC_SINC_MEDIUM_QUALITY;
        }
        else if (resamplerEngine == SRC_BEST)
        {
            mode = SRC_SINC_BEST_QUALITY;
        }

        for (int i = 0; i < (isMultiOut ? (1 + numOps) : 1); ++i)
        {
            if (lState[i])
            {
                src_delete(lState[i]);
            }
            if (rState[i])
            {
                src_delete(rState[i]);
            }
            int ec;

            lState[i] = src_new(mode, 1, &ec);
            rState[i] = src_new(mode, 1, &ec);
            src_set_ratio(lState[i], sampleRateRatio);
            src_set_ratio(rState[i], sampleRateRatio);
        }
    }

    // (Re)construct the audio-in resampler whenever the sample rate changes.
    // LanczosResampler constructor zeroes its internal ring buffers, so this
    // also acts as the "clear on rate change" requirement.
    audioInResampler =
        std::make_unique<audioInResampler_t>((float)hostSampleRate, (float)engineSampleRate);

    for (auto &[i, p] : patch.paramMap)
    {
        // paramAutomationSmoothingTimeMs is in milliseconds.
        p->lag.setRateInMilliseconds(monoValues.paramAutomationSmoothingTimeMs, engineSampleRate,
                                     1.0 / blockSize);
        p->lag.snapTo(p->value);
    }
    paramLagSet.removeAll();

    // midiCCSmoothingTimeMs is in milliseconds; also applies to MPE / note-expression lags
    // configured per-voice on attack.
    midiCCLagCollection.setRateInMilliseconds(monoValues.midiCCSmoothingTimeMs, engineSampleRate,
                                              1.0 / blockSize);
    midiCCLagCollection.snapAllActiveToTarget();

    audioToUi.push(
        {AudioToUIMsg::SEND_SAMPLE_RATE, 0, (float)hostSampleRate, (float)engineSampleRate});

    // Refresh anything keyed off engineSampleRate (filter coefs, ZOH ratio).
    // Safe vs. recursion: reapplyControlSettings only re-enters setSampleRate
    // when sampleRateStrategy diverges from the patch, which it doesn't here.
    reapplyControlSettings();
}

template <bool multiOut> void Synth::processInternal(const clap_output_events_t *outq)
{
    auto start = std::chrono::high_resolution_clock::now();
    if (!SinTable::staticsInitialized)
        SinTable::initializeStatics();

    processUIQueue(outq);

    if (!audioRunning)
    {
        memset(output, 0, sizeof(output));
        return;
    }

    for (auto it = paramLagSet.begin(); it != paramLagSet.end();)
    {
        it->lag.process();
        it->value = it->lag.v;
        if (!it->lag.isActive())
        {
            it = paramLagSet.erase(it);
        }
        else
        {
            ++it;
        }
    }

    midiCCLagCollection.processAll();

    monoValues.attackFloorOnRetrig = patch.output.attackFloorOnRetrig > 0.5;

    // Advance the song-position clock once per host block. On the first block of a host
    // buffer, re-anchor to the host position (when playing with a seconds timeline); every
    // other block — and the whole free-run case — advances by the real elapsed block time.
    // This keeps us sample-locked to the host even across wide (e.g. 4096) buffers.
    if (monoValues.isPlayingAndHasSecondsTimeline && monoValues.songPosNeedsResync)
        monoValues.songPosSeconds = monoValues.hostSongPosSeconds;
    else
        monoValues.songPosSeconds += (double)blockSize / hostSampleRate;
    monoValues.songPosNeedsResync = false;

    int loops{0};

    SRC_DATA d;

    int generated{0};

    if (usesLanczos())
        generated = (resampler[0]->inputsRequiredToGenerateOutputs(blockSize) > 0 ? 0 : blockSize);

    std::array<bool, numOps> mixerActive;
    if constexpr (multiOut)
    {
        std::fill(mixerActive.begin(), mixerActive.end(), false);
    }

    while (generated < blockSize)
    {
        loops++;
        lagHandler.process();

        // Hoist mono unison params so per-voice renderBlock derives uniRatioMul / uniPanShift
        // from the smoothed scalars without each voice repeating the twoToTheX lookup.
        monoValues.unisonSpreadFactorMinus1 =
            monoValues.twoToTheX.twoToThe(patch.output.unisonSpread.value) - 1.f;
        monoValues.unisonPanScalar = patch.output.unisonPan.value;

        auto op1IsAudioIn =
            ((int)std::round(patch.sourceNodes[0].waveForm.value) == SinTable::AUDIO_IN);
        if (audioInResampler && op1IsAudioIn)
        {
            float aiL[blockSize]{}, aiR[blockSize]{};
            auto got = (int)audioInResampler->populateNext(aiL, aiR, blockSize);
            for (int i = 0; i < blockSize; ++i)
                monoValues.audioInBlock[i] = (i < got) ? (aiL[i] + aiR[i]) * 0.5f : 0.f;
        }
        else
        {
            memset(monoValues.audioInBlock, 0, sizeof(monoValues.audioInBlock));
        }

        if (portaContinuation.updateEveryBlock && portaContinuation.active)
        {
            portaContinuation.portaFrac += portaContinuation.dPortaFrac;
            portaContinuation.sourceKey += portaContinuation.dKey;
            if (portaContinuation.portaFrac >= 1.0)
            {
                portaContinuation.active = false;
                portaContinuation.updateEveryBlock = false;
            }
        }

        float lOutput alignas(16)[2 * (1 + (multiOut ? numOps : 0))][blockSize];
        memset(lOutput, 0, sizeof(lOutput));

        auto cvoice = head;
        Voice *removeVoice{nullptr};

        while (cvoice)
        {
            assert(cvoice->used);
            cvoice->renderBlock();

            mech::accumulate_from_to<blockSize>(cvoice->output[0], lOutput[0]);
            mech::accumulate_from_to<blockSize>(cvoice->output[1], lOutput[1]);

            if constexpr (multiOut)
            {
                // TODO if voice active check
                float stp[2][blockSize];
                for (int i = 0; i < numOps; ++i)
                {
                    if (!cvoice->mixerNode[i].active)
                    {
                        continue;
                    }
                    if (!cvoice->mixerNode[i].from.operatorOutputsToOp)
                    {
                        continue;
                    }
                    mixerActive[i] = true;
                    mech::mul_block<blockSize>(cvoice->out.finalEnvLevel,
                                               cvoice->mixerNode[i].output[0], stp[0]);
                    mech::mul_block<blockSize>(cvoice->out.finalEnvLevel,
                                               cvoice->mixerNode[i].output[1], stp[1]);
                    mech::accumulate_from_to<blockSize>(stp[0], lOutput[2 + 2 * i]);
                    mech::accumulate_from_to<blockSize>(stp[1], lOutput[2 + 2 * i + 1]);
                }
            }

            if (cvoice->out.env.stage > OutputNode::env_t::s_release || cvoice->fadeBlocks == 0)
            {
                auto rvoice = cvoice;
                cvoice = removeFromVoiceList(cvoice);
                rvoice->next = removeVoice;
                removeVoice = rvoice;
            }
            else
            {
                cvoice = cvoice->next;
            }
        }

        while (removeVoice)
        {
            responder.doVoiceEndCallback(removeVoice);
            auto v = removeVoice;
            removeVoice = removeVoice->next;
            v->next = nullptr;
            assert(!v->next && !v->prior);
        }

        // End-of-chain stages run on the main engine-rate stereo bus,
        // before downsampling. Per-op buses in multiOut are not processed yet.
        processEndOfBlock(lOutput[0], lOutput[1]);

        if (usesLanczos())
        {
            if constexpr (multiOut)
            {
                for (int rsi = 0; rsi < numOps + 1; ++rsi)
                {
                    for (int i = 0; i < blockSize; ++i)
                    {
                        resampler[rsi]->push(lOutput[rsi * 2][i], lOutput[rsi * 2 + 1][i]);
                    }
                }
            }
            else
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    resampler[0]->push(lOutput[0][i], lOutput[1][i]);
                }
            }
            generated =
                (resampler[0]->inputsRequiredToGenerateOutputs(blockSize) > 0 ? 0 : blockSize);
        }
        else
        {
            int gen0{0};
            for (int rsi = 0; rsi < (multiOut ? (numOps + 1) : 1); ++rsi)
            {
                d.data_in = lOutput[2 * rsi];
                d.data_out = output[2 * rsi] + generated;
                d.input_frames = blockSize;
                d.output_frames = blockSize - generated;
                d.end_of_input = 0;
                d.src_ratio = sampleRateRatio;

                src_process(lState[rsi], &d);
                auto lgen = d.output_frames_gen;

                d.data_in = lOutput[2 * rsi + 1];
                d.data_out = output[2 * rsi + 1] + generated;
                d.input_frames = blockSize;
                d.output_frames = blockSize - generated;
                d.end_of_input = 0;
                d.src_ratio = sampleRateRatio;

                src_process(rState[rsi], &d);
                auto rgen = d.output_frames_gen;
                assert(lgen == rgen);
                if (rsi == 0)
                {
                    gen0 = lgen;
                }
            }
            generated += gen0;
        }

        if (isEditorAttached)
        {
            float stp[numOps][2][blockSize];
            memset(stp, 0, sizeof(stp));
            auto cvoice = head;
            while (cvoice)
            {
                for (int i = 0; i < numOps; ++i)
                {
                    if (!cvoice->mixerNode[i].active)
                    {
                        continue;
                    }
                    mech::mul_block<blockSize>(cvoice->out.finalEnvLevel,
                                               cvoice->mixerNode[i].output[0], stp[i][0]);
                    mech::mul_block<blockSize>(cvoice->out.finalEnvLevel,
                                               cvoice->mixerNode[i].output[1], stp[i][1]);
                }
                cvoice = cvoice->next;
            }
            for (int i = 0; i < blockSize; ++i)
            {
                vuPeak.process(lOutput[0][i], lOutput[1][i]);
                for (int j = 0; j < numOps; ++j)
                {
                    opVuPeak[j].process(stp[j][0][i], stp[j][1][i]);
                }
            }

            if (lastVuUpdate >= updateVuEvery)
            {
                AudioToUIMsg msg{AudioToUIMsg::UPDATE_VU, 0, vuPeak.vu_peak[0], vuPeak.vu_peak[1]};
                audioToUi.push(msg);
                for (uint32_t j = 0; j < numOps; ++j)
                {
                    AudioToUIMsg smsg{AudioToUIMsg::UPDATE_VU, j + 1, opVuPeak[j].vu_peak[0],
                                      opVuPeak[j].vu_peak[1]};
                    audioToUi.push(smsg);
                }

                AudioToUIMsg msg2{AudioToUIMsg::UPDATE_VOICE_COUNT, (uint32_t)voiceCount};
                audioToUi.push(msg2);

                AudioToUIMsg msg3{AudioToUIMsg::UPDATE_CPU_USAGE, 0, (float)(cpuUsage * 100)};
                audioToUi.push(msg3);

                AudioToUIMsg msg4{AudioToUIMsg::MTS_POINTER, 0, 0, 0, nullptr,
                                  monoValues.mtsClient};
                audioToUi.push(msg4);

                lastVuUpdate = 0;
            }
            else
            {
                lastVuUpdate++;
            }
        }
    }

    if (resamplerEngine == LANCZOS)
    {
        if constexpr (multiOut)
        {
            for (int rsi = 0; rsi < numOps + 1; ++rsi)
            {
                resampler[rsi]->populateNextBlockSize(output[rsi * 2], output[rsi * 2 + 1]);
                resampler[rsi]->renormalizePhases();
            }
        }
        else
        {
            resampler[0]->populateNextBlockSize(output[0], output[1]);
            resampler[0]->renormalizePhases();
        }
    }
    if (resamplerEngine == ZOH)
    {
        if constexpr (multiOut)
        {
            for (int rsi = 0; rsi < numOps + 1; ++rsi)
            {
                resampler[rsi]->populateNextBlockSizeZOH(output[rsi * 2], output[rsi * 2 + 1]);
                resampler[rsi]->renormalizePhases();
            }
        }
        else
        {
            resampler[0]->populateNextBlockSizeZOH(output[0], output[1]);
            resampler[0]->renormalizePhases();
        }
    }
    if (resamplerEngine == LINTERP)
    {
        if constexpr (multiOut)
        {
            for (int rsi = 0; rsi < numOps + 1; ++rsi)
            {
                resampler[rsi]->populateNextBlockSizeLin(output[rsi * 2], output[rsi * 2 + 1]);
                resampler[rsi]->renormalizePhases();
            }
        }
        else
        {
            resampler[0]->populateNextBlockSizeLin(output[0], output[1]);
            resampler[0]->renormalizePhases();
        }
    }

    if (isEditorAttached)
    {
        // Tap host-SR main bus for visualizers (only when someone is listening).
        if (audioOutputRing.subscribed())
        {
            audioOutputRing.push(output[0], output[1], blockSize);
        }

        // Finish CPU calculation
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        auto micros = duration.count();
        auto availmicrosinv = hostSampleRate * 1e-9 / blockSize;
        auto pct = micros * availmicrosinv;
        auto cpuFac = 0.995;
        cpuUsage = cpuUsage * cpuFac + pct * (1 - cpuFac);
    }
}

void Synth::process(const clap_output_events_t *o)
{
    if (isMultiOut)
        processInternal<true>(o);
    else
        processInternal<false>(o);
}

void Synth::addToVoiceList(Voice *v)
{
    v->prior = nullptr;
    v->next = head;
    if (v->next)
    {
        v->next->prior = v;
    }
    head = v;
    voiceCount++;
}

Voice *Synth::removeFromVoiceList(Voice *cvoice)
{
    if (patch.output.portaContMode.value > 0.5 && voiceCount == 1)
    {
        portaContinuation.sourceKey =
            cvoice->voiceValues.key + cvoice->voiceValues.portaDiff * cvoice->voiceValues.portaSign;
        portaContinuation.portaFrac = cvoice->voiceValues.portaFrac;
        portaContinuation.dPortaFrac = cvoice->voiceValues.dPortaFrac;
        portaContinuation.dKey = -cvoice->voiceValues.dPorta * cvoice->voiceValues.portaSign;

        portaContinuation.active = true;
        portaContinuation.updateEveryBlock = (int)std::round(patch.output.portaContMode.value) == 2;
    }
    else
    {
        portaContinuation.active = false;
    }
    if (cvoice->prior)
    {
        cvoice->prior->next = cvoice->next;
    }
    if (cvoice->next)
    {
        cvoice->next->prior = cvoice->prior;
    }
    if (cvoice == head)
    {
        head = cvoice->next;
    }
    auto nv = cvoice->next;
    cvoice->cleanup();
    cvoice->next = nullptr;
    cvoice->prior = nullptr;
    cvoice->fadeBlocks = -1;
    voiceCount--;

    return nv;
}

void Synth::dumpVoiceList()
{
    SXSNLOG("DUMP VOICE LIST : head=" << std::hex << head << std::dec);
    auto c = head;
    while (c)
    {
        SXSNLOG("   c=" << std::hex << c << std::dec << " key=" << c->voiceValues.key
                        << " u=" << c->used);
        c = c->next;
    }
}

void Synth::processUIQueue(const clap_output_events_t *outq)
{
    bool didRefresh{false};
    if (doFullRefresh)
    {
        pushFullUIRefresh();
        doFullRefresh = false;
        didRefresh = true;
    }
    // Accumulate rescan flags across the whole drain so we issue one
    // requestParamRescan / request_callback at the end, regardless of how many
    // messages in this batch wanted a rescan.
    uint32_t pendingRescan{0};
    auto uiM = mainToAudio.pop();
    while (uiM.has_value())
    {
        switch (uiM->action)
        {
        case MainToAudioMsg::REQUEST_REFRESH:
        {
            if (!didRefresh)
            {
                // don't do it twice in one process obvs
                pushFullUIRefresh();
            }
        }
        break;
        case MainToAudioMsg::SET_PARAM_WITHOUT_NOTIFYING:
        case MainToAudioMsg::SET_PARAM:
        {
            bool notify = uiM->action == MainToAudioMsg::SET_PARAM;

            auto dest = patch.paramMap.at(uiM->paramId);
            if (notify)
            {
                if (beginEndParamGestureCount == 0)
                {
                    SXSNLOG("Non-begin/end bound param edit for '" << dest->meta.name << "'");
                }
                if (dest->meta.type == md_t::FLOAT)
                    lagHandler.setNewDestination(&(dest->value), uiM->value);
                else
                    dest->value = uiM->value;

                clap_event_param_value_t p;
                p.header.size = sizeof(clap_event_param_value_t);
                p.header.time = 0;
                p.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                p.header.type = CLAP_EVENT_PARAM_VALUE;
                p.header.flags = 0;
                p.param_id = uiM->paramId;
                p.cookie = dest;

                p.note_id = -1;
                p.port_index = -1;
                p.channel = -1;
                p.key = -1;

                p.value = uiM->value;

                outq->try_push(outq, &p.header);
            }
            else
            {
                dest->value = uiM->value;
            }

            handleAudioThreadParamSideEffects(dest);

            auto d = patch.dirty;
            if (!d)
            {
                patch.dirty = true;
                audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
            }
        }
        break;
        case MainToAudioMsg::BEGIN_EDIT:
        case MainToAudioMsg::END_EDIT:
        {
            if (uiM->action == MainToAudioMsg::BEGIN_EDIT)
            {
                beginEndParamGestureCount++;
            }
            else
            {
                beginEndParamGestureCount--;
            }
            clap_event_param_gesture_t p;
            p.header.size = sizeof(clap_event_param_gesture_t);
            p.header.time = 0;
            p.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            p.header.type = uiM->action == MainToAudioMsg::BEGIN_EDIT
                                ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                : CLAP_EVENT_PARAM_GESTURE_END;
            p.header.flags = 0;
            p.param_id = uiM->paramId;

            outq->try_push(outq, &p.header);
        }
        break;
        case MainToAudioMsg::STOP_AUDIO:
        {
            if (lagHandler.active)
                lagHandler.instantlySnap();
            voiceManager->allSoundsOff();
            audioRunning = false;
        }
        break;
        case MainToAudioMsg::START_AUDIO:
        {
            audioRunning = true;
        }
        break;
        case MainToAudioMsg::SEND_PATCH_NAME:
        {
            memset(patch.name, 0, sizeof(patch.name));
            strncpy(patch.name, uiM->uiManagedPointer, 255);
            audioToUi.push({AudioToUIMsg::SET_PATCH_NAME, 0, 0, 0, patch.name});
        }
        break;
        case MainToAudioMsg::SEND_PATCH_AUTHOR:
        {
            patch.setAuthor(uiM->uiManagedPointer ? uiM->uiManagedPointer : "");
        }
        break;
        case MainToAudioMsg::SEND_MACRO_NAME:
        {
            auto idx = uiM->paramId;
            if (idx < numMacros && uiM->uiManagedPointer)
            {
                auto &buf = patch.macroNames[idx];
                // Only act on actual changes — avoids gratuitous INFO rescans on
                // preset loads where macro names match what the host already knows.
                if (std::strncmp(buf.data(), uiM->uiManagedPointer, buf.size()) != 0)
                {
                    std::fill(buf.begin(), buf.end(), 0);
                    strncpy(buf.data(), uiM->uiManagedPointer, buf.size() - 1);
                    AudioToUIMsg out{AudioToUIMsg::SET_MACRO_NAME};
                    out.paramId = idx;
                    out.patchNamePointer = buf.data();
                    audioToUi.push(out);
                    pendingRescan |= RescanRequest::INFO;
                }
            }
        }
        break;
        case MainToAudioMsg::SEND_PATCH_IS_CLEAN:
        {
            patch.dirty = false;
            audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
        }
        break;
        case MainToAudioMsg::SEND_POST_LOAD:
        {
            postLoad();
            // Preset load just rewrote every param value — let the host re-read.
            pendingRescan |= RescanRequest::VALUES;
        }
        break;
        case MainToAudioMsg::SEND_PREP_FOR_STREAM:
        {
            prepForStream();
        }
        break;
        case MainToAudioMsg::EDITOR_ATTACH_DETATCH:
        {
            isEditorAttached = uiM->paramId;
        }
        break;
        case MainToAudioMsg::PANIC_STOP_VOICES:
        {
            voiceManager->allSoundsOff();
        }
        break;
        case MainToAudioMsg::SET_DESIGN_MODE_RUN_ALL:
        {
            // Latched into each voice's active flag at attack(); in-flight voices keep
            // their state, the next attack picks up the new mode.
            monoValues.designModeRunAll = uiM->value > 0.5;
        }
        break;
        case MainToAudioMsg::SET_DAW_EXTRA_STATE:
        {
            auto *p = static_cast<const DawExtraState *>(uiM->dawExtraStatePointer);
            if (p)
            {
                dawExtraState = *p;
                // If the incoming DES had no <mpe> element (1.1-era session), fall back
                // to the legacy patch slots — patch swap messages are queued before this
                // one in CLAP stateLoad, so legacy values are already in place. Resolved
                // state is echoed back so future host saves are written in the new form.
                if (!dawExtraState.mpeFromExtraState)
                {
                    dawExtraState.mpeActive = patch.output.legacyMpeActive.value > 0.5f;
                    dawExtraState.mpeBendRange =
                        (int)std::round(patch.output.legacyMpeBendRange.value);
                }
                monoValues.mpeActive = dawExtraState.mpeActive;
                monoValues.mpeBendRange = dawExtraState.mpeBendRange;
                monoValues.midiCCSmoothingTimeMs = dawExtraState.midiCCSmoothingTimeMs;
                monoValues.paramAutomationSmoothingTimeMs =
                    dawExtraState.paramAutomationSmoothingTimeMs;
                applyMpeState();
                applySmoothingTimes();

                AudioToUIMsg au{AudioToUIMsg::SET_DAW_EXTRA_STATE};
                au.dawExtraStatePointer = &dawExtraState;
                audioToUi.push(au);
            }
        }
        break;
        case MainToAudioMsg::SET_MPE_ACTIVE:
        {
            monoValues.mpeActive = uiM->value > 0.5f;
            dawExtraState.mpeActive = monoValues.mpeActive;
            applyMpeState();
            AudioToUIMsg au{AudioToUIMsg::SET_DAW_EXTRA_STATE};
            au.dawExtraStatePointer = &dawExtraState;
            audioToUi.push(au);
        }
        break;
        case MainToAudioMsg::SET_MPE_BEND_RANGE:
        {
            monoValues.mpeBendRange = std::clamp((int)std::round(uiM->value), 1, 96);
            dawExtraState.mpeBendRange = monoValues.mpeBendRange;
            applyMpeState();
            AudioToUIMsg au{AudioToUIMsg::SET_DAW_EXTRA_STATE};
            au.dawExtraStatePointer = &dawExtraState;
            audioToUi.push(au);
        }
        break;
        case MainToAudioMsg::SET_MIDI_CC_SMOOTHING_TIME_MS:
        {
            monoValues.midiCCSmoothingTimeMs = std::max(0.f, uiM->value);
            dawExtraState.midiCCSmoothingTimeMs = monoValues.midiCCSmoothingTimeMs;
            applySmoothingTimes();
            AudioToUIMsg au{AudioToUIMsg::SET_DAW_EXTRA_STATE};
            au.dawExtraStatePointer = &dawExtraState;
            audioToUi.push(au);
        }
        break;
        case MainToAudioMsg::SET_PARAM_AUTOMATION_SMOOTHING_TIME_MS:
        {
            monoValues.paramAutomationSmoothingTimeMs = std::max(0.f, uiM->value);
            dawExtraState.paramAutomationSmoothingTimeMs =
                monoValues.paramAutomationSmoothingTimeMs;
            applySmoothingTimes();
            AudioToUIMsg au{AudioToUIMsg::SET_DAW_EXTRA_STATE};
            au.dawExtraStatePointer = &dawExtraState;
            audioToUi.push(au);
        }
        break;
        }
        uiM = mainToAudio.pop();
    }

    // One coalesced rescan request per drain pass.
    if (pendingRescan)
        requestParamRescan(pendingRescan);
}

void Synth::reapplyControlSettings()
{
    if (sampleRateStrategy != (SampleRateStrategy)patch.output.sampleRateStrategy.value ||
        resamplerEngine != (ResamplerEngine)patch.output.resampleEngine.value)
    {
        sampleRateStrategy = (SampleRateStrategy)patch.output.sampleRateStrategy.value;
        resamplerEngine = (ResamplerEngine)patch.output.resampleEngine.value;

        if (hostSampleRate > 0)
        {
            voiceManager->allSoundsOff();
            setSampleRate(hostSampleRate);
        }
    }

    auto val = (int)std::round(patch.output.playMode.value);
    if (val != 0)
    {
        voiceManager->setPlaymode(0, voiceManager_t::PlayMode::MONO_NOTES,
                                  (int)voiceManager_t::MonoPlayModeFeatures::NATURAL_LEGATO);
        voiceManager->repeatedKeyMode = voiceManager_t::RepeatedKeyMode::MULTI_VOICE;
    }
    else
    {
        voiceManager->setPlaymode(0, voiceManager_t::PlayMode::POLY_VOICES);
        if (patch.output.pianoModeActive.value > 0.5)
        {
            voiceManager->repeatedKeyMode = voiceManager_t::RepeatedKeyMode::PIANO;
        }
        else
        {
            voiceManager->repeatedKeyMode = voiceManager_t::RepeatedKeyMode::MULTI_VOICE;
        }
    }

    auto lim = (int)std::round(patch.output.polyLimit.value);
    lim = std::clamp(lim, 1, (int)maxVoices);
    voiceManager->setPolyphonyGroupVoiceLimit(0, lim);

    applyMpeState();

    // End-of-chain high/low cut. The "active" flags gate per-block work.
    // Coefficient setup over-applies on every reapply for now.
    auto sr = (float)engineSampleRate;

    // Head-of-chain ultrasonic brickwall: steep Butterworth at host Nyquist,
    // run at the engine (oversample) rate ahead of every other output stage.
    ultrasonicActive = patch.output.ultrasonicFilter.value > 0.5;
    if (ultrasonicActive && sr > 0 && hostSampleRate > 0)
    {
        ultrasonicFilter.setCutoffAndSampleRate((float)hostSampleRate * 0.5f, sr);
        ultrasonicFilter.reset();
    }

    auto lpVal = (int)std::round(patch.output.lowpass.value);
    lpActive = lpVal != LP_NONE;
    if (lpActive && sr > 0)
    {
        float freq = 0;
        switch (lpVal)
        {
        case LP_7K5:
            freq = 7500.f;
            break;
        case LP_10K:
            freq = 10000.f;
            break;
        case LP_13K:
            freq = 13000.f;
            break;
        case LP_16K:
            freq = 16000.f;
            break;
        case LP_20K:
            freq = 20000.f;
            break;
        }
        lpFilter.setCutoffAndSampleRate(freq, sr);
        lpFilter.reset();
    }

    auto brVal = (int)std::round(patch.output.bitRateAdjust.value);
    bitRateActive = brVal != BR_NONE;
    bitRatePreFilterActive = false;
    if (bitRateActive && sr > 0)
    {
        bool prefilter = patch.output.zohPreFilter.value > 0.5;
        float target = 0;
        switch (brVal)
        {
        case BR_12K_ZOH:
            target = 12000.f;
            break;
        case BR_16K_ZOH:
            target = 16000.f;
            break;
        case BR_18K_ZOH:
            target = 18000.f;
            break;
        case BR_20K_ZOH:
            target = 20000.f;
            break;
        case BR_22K_ZOH:
            target = 22000.f;
            break;
        case BR_24K_ZOH:
            target = 24000.f;
            break;
        case BR_28K_ZOH:
            target = 28000.f;
            break;
        case BR_32K_ZOH:
            target = 32000.f;
            break;
        case BR_48K_ZOH:
            target = 48000.f;
            break;
        }
        bitRateZOH.setRate(target, sr);
        bitRateZOH.reset();
        // Band-limit the per-voice noise to the crusher Nyquist so the ZOH has
        // nothing above its Nyquist to fold (which would whiten the tilt).
        monoValues.noiseBandLimitHz = target * 0.5f;
        if (prefilter)
        {
            bitRatePreFilterActive = true;
            // Match the tilt-noise pre-filter: steep Butterworth at the crusher
            // Nyquist, scootched to 20k since the SRC back up still reflects.
            auto cutoff = std::min(target * 0.5f, 20000.f);
            bitRatePreFilter.setCutoffAndSampleRate(cutoff, sr);
            bitRatePreFilter.reset();
        }
    }
    else
    {
        monoValues.noiseBandLimitHz = 0.f;
    }

    auto hpVal = (int)std::round(patch.output.highpass.value);
    hpActive = hpVal != HP_NONE;
    if (hpActive && sr > 0)
    {
        float freq = 0;
        switch (hpVal)
        {
        case HP_10HZ:
            freq = 10.f;
            break;
        case HP_20HZ:
            freq = 20.f;
            break;
        case HP_50HZ:
            freq = 50.f;
            break;
        }
        hpFilter.setCutoffAndSampleRate(freq, sr);
        hpFilter.reset();
    }
}

void Synth::applyMpeState()
{
    voiceManager->dialect = monoValues.mpeActive ? voiceManager_t::MIDI1Dialect::MIDI1_MPE
                                                 : voiceManager_t::MIDI1Dialect::MIDI1;
}

void Synth::applySmoothingTimes()
{
    if (engineSampleRate <= 0)
        return;
    midiCCLagCollection.setRateInMilliseconds(monoValues.midiCCSmoothingTimeMs, engineSampleRate,
                                              1.0 / blockSize);
    for (auto &[i, p] : patch.paramMap)
    {
        p->lag.setRateInMilliseconds(monoValues.paramAutomationSmoothingTimeMs, engineSampleRate,
                                     1.0 / blockSize);
    }
}

void Synth::processEndOfBlock(float *L, float *R)
{
    if (ultrasonicActive)
        ultrasonicFilter.processBlock(L, R, blockSize);

    auto satType = (int)std::round(patch.output.saturationType.value);
    if (satType != SAT_NONE)
    {
        auto dv = patch.output.saturationDrive.value;
        auto drive = dv * dv * dv;
        if (satType == SAT_SOFT)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                L[i] = softSaturator(L[i] * drive);
                R[i] = softSaturator(R[i] * drive);
            }
        }
        else if (satType == SAT_OJD)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                L[i] = ojdSaturator(L[i] * drive);
                R[i] = ojdSaturator(R[i] * drive);
            }
        }
    }

    if (bitRatePreFilterActive)
    {
        bitRatePreFilter.processBlock(L, R, blockSize);
    }
    if (bitRateActive)
    {
        for (int i = 0; i < blockSize; ++i)
            bitRateZOH.step(L[i], R[i]);
    }

    auto bdVal = (int)std::round(patch.output.bitDepthAdjust.value);
    if (bdVal != BD_NONE)
    {
        int bits = 0;
        switch (bdVal)
        {
        case BD_8:
            bits = 8;
            break;
        case BD_12:
            bits = 12;
            break;
        case BD_16:
            bits = 16;
            break;
        }
        // bits levels span -1..1, so scale = 2^(bits-1) (e.g. 8 bit → 128 steps each side).
        auto scale = (float)(1 << (bits - 1));
        auto invScale = 1.f / scale;
        for (int i = 0; i < blockSize; ++i)
        {
            L[i] = std::round(L[i] * scale) * invScale;
            R[i] = std::round(R[i] * scale) * invScale;
        }
    }

    if (lpActive)
        lpFilter.processBlock(L, R, blockSize);
    if (hpActive)
        hpFilter.processBlock(L, R, blockSize);

    // Output gain: param value v in [0, 2], applied gain = v^3 (display in dB).
    auto v = patch.output.outputGain.value;
    auto g = v * v * v;
    if (g != 1.f)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            L[i] *= g;
            R[i] *= g;
        }
    }
}

void Synth::handleParamValue(Param *p, uint32_t pid, float value)
{
    if (!p)
    {
        p = patch.paramMap.at(pid);
    }

    // Mirror the UI path (SET_PARAM): only FLOATs lag; discrete params snap so
    // that handleAudioThreadParamSideEffects sees the new value, not the old.
    if (p->meta.type == md_t::FLOAT)
    {
        p->lag.setTarget(value);
        paramLagSet.addToActive(p);
    }
    else
    {
        p->value = value;
    }

    handleAudioThreadParamSideEffects(p);

    AudioToUIMsg au = {AudioToUIMsg::UPDATE_PARAM, pid, value};
    audioToUi.push(au);
}

void Synth::handleAudioThreadParamSideEffects(Param *dest)
{
    if (dest->meta.id == patch.output.playMode.meta.id ||
        dest->meta.id == patch.output.polyLimit.meta.id ||
        dest->meta.id == patch.output.pianoModeActive.meta.id ||
        dest->meta.id == patch.output.sampleRateStrategy.meta.id ||
        dest->meta.id == patch.output.resampleEngine.meta.id ||
        dest->meta.id == patch.output.lowpass.meta.id ||
        dest->meta.id == patch.output.highpass.meta.id ||
        dest->meta.id == patch.output.bitRateAdjust.meta.id ||
        dest->meta.id == patch.output.zohPreFilter.meta.id ||
        dest->meta.id == patch.output.ultrasonicFilter.meta.id)
    {
        reapplyControlSettings();
    }

    if (dest->adhocFeatures & Param::AdHocFeatureValues::SOLO)
    {
        resetSoloState();
    }
}

void Synth::pushFullUIRefresh()
{
    for (const auto *p : patch.params)
    {
        AudioToUIMsg au = {AudioToUIMsg::UPDATE_PARAM, p->meta.id, p->value};
        audioToUi.push(au);
    }
    audioToUi.push({AudioToUIMsg::SET_PATCH_NAME, 0, 0, 0, patch.name});
    audioToUi.push({AudioToUIMsg::SET_PATCH_DIRTY_STATE, patch.dirty});
    audioToUi.push(
        {AudioToUIMsg::SEND_SAMPLE_RATE, 0, (float)hostSampleRate, (float)engineSampleRate});

    AudioToUIMsg des{AudioToUIMsg::SET_DAW_EXTRA_STATE};
    des.dawExtraStatePointer = &dawExtraState;
    audioToUi.push(des);
}

void Synth::resetSoloState()
{
    bool anySolo = false;
    for (auto &mn : patch.mixerNodes)
    {
        anySolo = anySolo || (mn.solo.value > 0.5);
    }

    for (auto &mn : patch.mixerNodes)
    {
        mn.isMutedDueToSoloAway = anySolo && !(mn.solo.value > 0.5);
    }
}

void Synth::onMainThread()
{
    auto flags = onMainRescanFlags.exchange(0, std::memory_order_acquire);
    if (flags == 0 || !clapHost)
        return;
    auto pe =
        static_cast<const clap_host_params_t *>(clapHost->get_extension(clapHost, CLAP_EXT_PARAMS));
    if (!pe)
        return;
    // CLAP says INFO is mutually exclusive with VALUES; issue separately.
    if (flags & RescanRequest::VALUES)
        pe->rescan(clapHost, CLAP_PARAM_RESCAN_VALUES);
    if (flags & RescanRequest::INFO)
        pe->rescan(clapHost, CLAP_PARAM_RESCAN_INFO);
}

void Synth::requestParamRescan(uint32_t flags)
{
    if (flags == 0 || !clapHost)
        return;
    onMainRescanFlags.fetch_or(flags, std::memory_order_release);
    clapHost->request_callback(clapHost);
}

} // namespace baconpaul::six_sines
