TODO BEFORE 1JAN2025

SOURCE:
- Keytrack on/off, and if off ratio -> offset from 440hz
- 90%-100% of internal nyquist mute fades (maybe ship without this tho)

MIXER:
- LFO on Mixer Node to level or pan

MODULATION:
- Each item gets a modulation list from source set
- Monophonic Midi source
- Macro source
- Polyphonic Voice Source
- MPE source? (later)
- Note expressions? (later)

PRESETS:
- preset factory set with cmakerc
- preset scan user directory to make tree

PLAY MODE:
- Portamento maybe
- An envelope retrigger legato more where envelopes are opt-in to retrigger even on gated voices
- Voice Unison

CLAP
- An output per OP wher each output is just the solo OP * Main ADSR (and zero if the OP is off)

GENERAL AND CLEANUPS
- Ctrl/Knob quantizes on ratio to PO2
- Show ratios as 1/2.5 not .46 and allow typein in same format
- AM is somehow not quiet right. Signal to zero seems 'no modulation' not 'no output'
- Don't send VU etc when editor not attached
- LFO in Pulse and S&H need a tiny little LPF to avoid super-clicka on modulation nodes (but not ratio nodes)
- Main DAHDSR at DAHD=0 SR=1 means voice lifetime is longest active op mixer DAHDSR and main DAHDSR bypassed


