/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#include "synth/synth.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/PanLaws.h"

namespace baconpaul::fm
{

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

Synth::Synth() : responder(*this)
{
    voiceManager = std::make_unique<voiceManager_t>(responder, monoResponder);
    lagHandler.setRate(60, blockSize, gSampleRate);
}
void Synth::process()
{
    assert(resampler);

    processUIQueue();

    while (resampler->inputsRequiredToGenerateOutputs(blockSize) > 0)
    {
        lagHandler.process();

        float lOutput alignas(16)[2][blockSize];
        memset(lOutput, 0, sizeof(output));
        // this can be way more efficient
        auto cvoice = head;

        while (cvoice)
        {
            assert(cvoice->used);
            cvoice->renderBlock();

            mech::accumulate_from_to<blockSize>(cvoice->output[0], lOutput[0]);
            mech::accumulate_from_to<blockSize>(cvoice->output[1], lOutput[1]);

            if (cvoice->out.env.stage >
                sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize>::s_release)
            {
                responder.doVoiceEndCallback(cvoice);
                cvoice = removeFromVoiceList(cvoice);
            }
            else
            {
                cvoice = cvoice->next;
            }
        }

        // Apply main output
        auto lv = patch.mainOutput.level.value;
        lv = lv * lv * lv;
        mech::scale_by<blockSize>(lv, lOutput[0], lOutput[1]);

        auto pn = patch.mainOutput.pan.value;
        if (pn != 0.f)
        {
            pn = (pn + 1) * 0.5;
            sdsp::pan_laws::panmatrix_t pmat;
            sdsp::pan_laws::stereoEqualPower(pn, pmat);

            for (int i = 0; i < blockSize; ++i)
            {
                auto il = lOutput[0][i];
                auto ir = lOutput[1][i];
                lOutput[0][i] = pmat[0] * il + pmat[2] * ir;
                lOutput[1][i] = pmat[1] * ir + pmat[3] * il;
            }
        }

        for (int i = 0; i < blockSize; ++i)
        {
            resampler->push(lOutput[0][i], lOutput[1][i]);
        }
    }

    resampler->populateNextBlockSize(output[0], output[1]);
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
}

Voice *Synth::removeFromVoiceList(Voice *cvoice)
{
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
    cvoice->used = false;
    cvoice->next = nullptr;
    cvoice->prior = nullptr;
    return nv;
}

void Synth::dumpVoiceList()
{
    FMLOG("DUMP VOICE LIST : head=" << std::hex << head << std::dec);
    auto c = head;
    while (c)
    {
        FMLOG("   c=" << std::hex << c << std::dec << " key=" << c->key << " u=" << c->used);
        c = c->next;
    }
}

void Synth::processUIQueue()
{
    bool didRefresh{false};
    if (doFullRefresh)
    {
        pushFullUIRefresh();
        doFullRefresh = false;
        didRefresh = true;
    }
    auto uiM = uiToAudio.pop();
    while (uiM.has_value())
    {
        switch (uiM->action)
        {
        case UIToAudioMsg::REQUEST_REFRESH:
        {
            if (!didRefresh)
            {
                // don't do it twice in one process obvs
                pushFullUIRefresh();
            }
        }
        break;
        case UIToAudioMsg::SET_PARAM:
        {
            auto dest = &(patch.paramMap.at(uiM->paramId)->value);
            lagHandler.setNewDestination(dest, uiM->value);
        }
        break;
        }
        uiM = uiToAudio.pop();
    }
}

void Synth::pushFullUIRefresh()
{
    for (const auto *p : patch.params)
    {
        AudioToUIMsg au = {AudioToUIMsg::UPDATE_PARAM, p->meta.id, p->value};
        audioToUi.push(au);
    }
}

} // namespace baconpaul::fm