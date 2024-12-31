TODO BEFORE 1JAN2025


ANDREYA BUG LIST
- What Andreya really wants is a full feldge screen for main pan and tune. Fine.
- Envelope combination is a mess
------
  so then it is

Env Plus/Mul (envIsMultiplactive)
LFO is enveloped or not  (lfoIsEnveloped)

Env Depth (gray out if env is Mul)
LFO Depth

are the 5 controls we need for each 'nubbin'

With that in mind review still
- Self
- FromTo
- Main
-----


GENERAL AND CLEANUPS
- Temposync Labels screwed
- Triangle Waveform
- Per operator octave
- Windows HPDI
- Screen Reader Check
  - Toggles and Multiswitches dont have accesible set actions
  - Multi-switch doesn't have RMB
  - Ruled Labels are on
- Ctrl/Knob quantizes on ratio to PO2
- Write some semblance of a manual

THINGS I DIDNT GET TO FOR 1.0
- Temposync Envelopes
- An output per OP wher each output is just the solo OP * Main ADSR (and zero if the OP is off)
- AM is somehow not quiet intuitive. Modulator signal to zero results in 'no output' but it sort of intuitively should me
  'no modulation'
- 90%-100% of internal nyquist mute fades (maybe ship without this tho)
- Main DAHDSR at DAHD=0 SR=1 means voice lifetime is longest active op mixer DAHDSR and main DAHDSR bypassed
- Note Expressions
- CLAP PolyMod
- Global tuning other than octave transpose
- A main LFO
- Negative Feedback
- Solo Oscillators in the mixer


JACKY IDEAS
---
This is so much better now with MPE and the LS. Thanks for it.

Reflecting back on earlier discussions, I'm wondering if it would be within the realm of possibilities, to be able to quantize Op Ratios to integer harmonics...

With Pressure routed to this, one could then actually break-out precise harmonics above the fundamental frequencies.

The way it works now is one very useful pole, but quantizing ratios to integers during modulation would be an amazing next-level feature.
In an ideal scenario, I might set the depth such that it only modulates up to harmonic 4.
Thinking exponential, rather than linear, so there's a wider area around the 1/1...
Rambling and dreaming. I know the clock is running out, but just tossing this out there, as it does seem interesting and akin to the behavior of acoustic instruments somehow.
---