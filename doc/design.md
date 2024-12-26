TODO BEFORE 1JAN2025

SOURCE:
- Keytrack on/off, and if off ratio -> offset from 440hz
- 90%-100% of internal nyquist mute fades (maybe ship without this tho)

MAIN:
- Voice limit in Main Area hooked up
- Voice limit font and split font with menu button from top

MIXER:
- LFO on Mixer Node to level or pan

MODULATION:
- Each item gets a modulation list from source set
- Monophonic Midi source
- Macro source
- Polyphonic Voice Source
- MPE source
- Note expressions?

PRESETS:
- preset factory set with cmakerc
- preset scan user directory to make tree

PLAY MODE:
- Portamento maybe
- An envelope retrigger legato more where envelopes are opt-in to retrigger even on gated voices

GENERAL AND CLEANUPS
- Ctrl/Knob quantizes on ratio to PO2
- Show ratios as 1/2.5 not .46 and allow typein in same format
- AM is somehow not quiet right. Signal to zero seems 'no modulation' not 'no output'
- Don't send VU etc when editor not attached
- LFO in Pulse and S&H need a tiny little LPF to avoid super-clicka on modulation nodes (but not ratio nodes)



