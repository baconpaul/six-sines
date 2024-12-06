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

#ifndef BACONPAUL_FMTHING_SYNTH_SYNTH_H
#define BACONPAUL_FMTHING_SYNTH_SYNTH_H

#include <memory>
#include "sst/voicemanager/voicemanager.h"

namespace baconpaul::fm
{
struct Voice;

struct Synth
{
    struct VMConfig
    {
        static constexpr size_t maxVoiceCount{128};
        using voice_t = Voice;
    };
    struct VMResponder
    {
        void setVoiceEndCallback(std::function<void(Voice *)>) {}
        void retriggerVoiceWithNewNoteID(Voice *, int32_t, float) {}
        void moveVoice(Voice *, uint16_t, uint16_t, uint16_t, float) {}
        void moveAndRetriggerVoice(Voice *, uint16_t, uint16_t, uint16_t, float) {}

        int32_t beginVoiceCreationTransaction(
            typename sst::voicemanager::VoiceBeginBufferEntry<VMConfig>::buffer_t &, uint16_t,
            uint16_t, uint16_t, int32_t, float)
        {
            return 1;
        };
        void endVoiceCreationTransaction(uint16_t, uint16_t, uint16_t, int32_t, float) {}
        void terminateVoice(Voice *) {}
        int32_t initializeMultipleVoices(
            int32_t,
            const typename sst::voicemanager::VoiceInitInstructionsEntry<VMConfig>::buffer_t &,
            typename sst::voicemanager::VoiceInitBufferEntry<VMConfig>::buffer_t &, uint16_t,
            uint16_t, uint16_t, int32_t, float, float)
        {
            return 1;
        }
        void releaseVoice(Voice *, float) {}
        void setNoteExpression(Voice *, int32_t, double) {}
        void setVoicePolyphonicParameterModulation(Voice *, uint32_t, double) {}
        void setPolyphonicAftertouch(Voice *, int8_t) {}

        void setVoiceMIDIMPEChannelPitchBend(Voice *, uint16_t) {}
        void setVoiceMIDIMPEChannelPressure(Voice *, int8_t) {}
        void setVoiceMIDIMPETimbre(Voice *, int8_t) {}
    };
    struct VMMonoResponder
    {
        void setMIDIPitchBend(int16_t, int16_t) {}
        void setMIDI1CC(int16_t, int16_t, int8_t) {}
        void setMIDIChannelPressure(int16_t, int16_t) {}
    };
    using voiceManager_t = sst::voicemanager::VoiceManager<VMConfig, VMResponder, VMMonoResponder>;

    static_assert(sst::voicemanager::constraints::ConstraintsChecker<VMConfig, VMResponder,
                                                                     VMMonoResponder>::satisfies());
};
} // namespace baconpaul::fm
#endif // SYNTH_H
