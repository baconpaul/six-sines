TODO BEFORE 1JAN2025


ANDREYA BUG LIST
- What Andreya really wants is a full feldge screen for main pan and tune. Fine.


GENERAL AND CLEANUPS
- Temposync Labels screwed
- Windows HPDI
- Screen Reader Check
  - Toggles and Multiswitches dont have accesible set actions
  - Multi-switch doesn't have RMB
  - Ruled Labels are on
- Ctrl/Knob quantizes on ratio to PO2
- Write some semblance of a manual

THINGS I DIDNT GET TO FOR 1.0
- Triangle Waveform (you can do it as a sin-topped tri like we do a sin-topped saw and square)
- Temposync Envelopes
- An output per OP wher each output is just the solo OP * Main ADSR (and zero if the OP is off)
- 90%-100% of internal nyquist mute fades (maybe ship without this tho)
- Main DAHDSR at DAHD=0 SR=1 means voice lifetime is longest active op mixer DAHDSR and main DAHDSR bypassed
- Note Expressions
- CLAP PolyMod
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