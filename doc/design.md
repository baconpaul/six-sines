Individual Oscillator

- Keytrack
  - if on: ratio
  - if off: absolute frequency
- Waveshape (quadrant picker)
- lfo with depth to ratio
+ dahdsr with depth to ratio
- ratio uses 2^x table

FM Matrix Node
+ DAHDSR to scale float depth
- LFO to add to float depth

Self matrix node
+ DAHDSR to scale fb depth
+ LFO to add to fb depth

Mixer
+ Each node gets an envelope 
- Each node an LFO to level or a pan

Macros and MIDI
- A macros section - 6 macros each with a target list of 4 params at depth?
- and maybe do midi routing the same way? (put a midi button above the macro vertical)
- velocity to amplitude in main
- Other (monophonic) MIDI?

Other ToDos
- Envelope Rate Linear to 2x provider
- General UI

Installer

Probably ship at this point but then

Non-monophonic voice level modulation
- CLAP advanced features like polymod
- Note expressions?
- MPE?


Saturday
+ typeins
- preset save/restore
- sort params more natrually
- envelope power for subs
- temposync lfo
- restore rm with each matrix node having a to-rm-or-to-fm option
- auv2
- vst3
- a build
- deal with ui attached or not for messages
