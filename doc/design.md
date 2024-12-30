TODO BEFORE 1JAN2025

MODULATION:
- Put modulation in
  - matrix
  - mixer
  - main
- Unison Sources

PATCH SELECTOR:
- Jog Buttons for next/prev

MAIN/PLAY MODE:
- Global Transpose and Global in-semitone-tuning with LFO routable to tuning

CLAP
- An output per OP wher each output is just the solo OP * Main ADSR (and zero if the OP is off)

GENERAL AND CLEANUPS
- Screen Reader Check
  - Toggles and Multiswitches dont have accesible set actions
  - Show menu doesnt work for set value
- Ctrl/Knob quantizes on ratio to PO2
- AM is somehow not quiet right. Signal to zero seems 'no modulation' not 'no output'
- Don't send VU etc when editor not attached
- Write a better README
- Write some semblance of documentation

THINGS I DIDNT DO
- 90%-100% of internal nyquist mute fades (maybe ship without this tho)
- Main DAHDSR at DAHD=0 SR=1 means voice lifetime is longest active op mixer DAHDSR and main DAHDSR bypassed
- Note Expressions
- CLAP PolyMod