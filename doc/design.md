TODO BEFORE 1JAN2025

SOURCE:
- Keytrack on/off, and if off ratio -> offset from 440hz
- waveshapes:
  - TX waveshapes
  - maybe a smoothed-saw and smoothed-square if i can figure out an analytic form i like
- 90%-100% of internal nyquist mute fades

MAIN:
- Voice limit in Main Area hooked up
- Voice limit font and split font with menu button from top

MIXER:
- LFO on Mixer Node to level or pan

MODULATION:
- Each item gets a modulation list from source set
- Monophonic Midi source
- Macro source
- Polyphonic midi source 
- MPE source
- Note expressions?

PRESETS:
- preset factory set with cmakerc
- preset scan user directory to make tree

PLAY MODE:
- Pitch Bend support
- Portamento maybe
- An envelope retrigger legato more where envelopes are opt-in to retrigger even on gated voices

GENERAL AND CLEANUPS
- AM is somehow not quiet right. Signal to zero seems 'no modulation' not 'no output'
- Don't send VU etc when editor not attached
- Edit area says "click a knbo to edit on startup"
- LFO in Pulse and S&H need a tiny little LPF to avoid super-clicka

INFRASTRUCTURE:
- mac signing


