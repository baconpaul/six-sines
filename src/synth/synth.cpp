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
void Synth::process()
{
    assert(resampler);

    while (resampler->inputsRequiredToGenerateOutputs(blockSize) > 0)
    {
        float lOutput alignas(16)[2][blockSize];
        memset(lOutput, 0, sizeof(output));
        // this can be way more efficient
        auto cvoice = head;

        while (cvoice)
        {
            if (!cvoice->used)
            {
                FMLOG("Unused cvoice at " << cvoice->key);
            }
            assert(cvoice->used);
            cvoice->src.setFrequency(440.0 * pow(2.0, (cvoice->key - 69) / 12.0));
            cvoice->src.renderBlock();
            cvoice->out.renderBlock(cvoice->gated);
            for (int s = 0; s < blockSize; ++s)
            {
                lOutput[0][s] += cvoice->out.output[0][s] * 0.5;
                lOutput[1][s] += cvoice->out.output[1][s] * 0.5;
            }

            if (cvoice->out.env.stage >
                sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize>::s_release)
            {
                responder.doVoiceEndCallback(cvoice);

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
                cvoice->used = false;
                auto nv = cvoice->next;
                cvoice->next = nullptr;
                cvoice->prior = nullptr;
                cvoice = nv;

                // dumpList();
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

void Synth::dumpList()
{
    FMLOG("DUMP LIST : head=" << std::hex << head << std::dec);
    auto c = head;
    while (c)
    {
        FMLOG("   c=" << std::hex << c << std::dec << " key=" << c->key << " u=" << c->used);
        c = c->next;
    }
}

} // namespace baconpaul::fm