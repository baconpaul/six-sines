TODO BEFORE 1JAN2025

SOURCE:
- Keytrack on/off, and if off ratio -> offset from 440hz
- TX waveshapes
- Ratio uses 2^x table rather than pow
- Ratio lerps across block

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
- Voice limit count
- Pitch Bend support
- Portamento maybe

GENERAL AND CLEANUPS
- AM is somehow not quiet right. Signal to zero seems 'no modulation' not 'no output'
- Temposync the LFO
- Don't send VU etc when editor not attached
- Edit area says "click a knbo to edit on startup"
- SRProvider uses table, not pow

INFRASTRUCTURE:
- mac signing
- win/lin zip only
- pipelines on all three platforms


