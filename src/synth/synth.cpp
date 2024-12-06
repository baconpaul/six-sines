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
    // sample rate conversion has to go here
    memset(output, 0, sizeof(output));
    for (int i = 0; i < VMConfig::maxVoiceCount; ++i)
    {
        if (voices[i].used)
        {
            voices[i].src.setFrequency(440.0 * pow(2.0, (voices[i].key - 69) / 12.0));
            voices[i].src.renderBlock();
            voices[i].out.renderBlock(voices[i].gated);
            if (voices[i].out.env.stage >
                sst::basic_blocks::modulators::DAHDSREnvelope<SRProvider, blockSize>::s_release)
            {
                FMLOG("Ending voice at " << voices[i].key);
                responder.doVoiceEndCallback(&voices[i]);
                voices[i].used = false;
            }
            for (int s = 0; s < blockSize; ++s)
            {
                output[0][s] += voices[i].out.output[0][s];
                output[1][s] += voices[i].out.output[1][s];
            }
        }
    }
}
} // namespace baconpaul::fm