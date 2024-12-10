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

namespace baconpaul::fm
{
Synth::Synth() : responder(*this)
{
    voiceManager = std::make_unique<voiceManager_t>(responder, monoResponder);
}
void Synth::process()
{
    assert(resampler);

    processUIQueue();

    while (resampler->inputsRequiredToGenerateOutputs(blockSize) > 0)
    {
        float lOutput alignas(16)[2][blockSize];
        memset(lOutput, 0, sizeof(output));
        // this can be way more efficient
        auto cvoice = head;

        while (cvoice)
        {
            assert(cvoice->used);
            cvoice->renderBlock();

            for (int s = 0; s < blockSize; ++s)
            {
                lOutput[0][s] += cvoice->output[0][s] * 0.5;
                lOutput[1][s] += cvoice->output[1][s] * 0.5;
            }

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
        FMLOG("UI Message " << uiM->action);
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