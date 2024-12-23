TODO BEFORE 1JAN2025

SOURCE:
- Keytrack on/off, and if off ratio -> offset from 440hz
- waveshapes:
  - TX waveshapes
  - maybe a smoothed-saw and smoothed-square if i can figure out an analytic form i like
- 90%-100% of internal nyquist mute fades

MAIN:
- Rotate VU meter to use space
- Put voice count below vU meter along with voice limit editor

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
- Voice limit count and Voice Count display
- Pitch Bend support
- Portamento maybe

GENERAL AND CLEANUPS
- Temposync the LFO
- AM is somehow not quiet right. Signal to zero seems 'no modulation' not 'no output'
- Don't send VU etc when editor not attached
- Edit area says "click a knbo to edit on startup"
- LFO in Pulse and S&H need a tiny little LPF to avoid super-clicka

INFRASTRUCTURE:
- mac signing


