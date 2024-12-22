Individual Oscillator

- Keytrack
  - if on: ratio
  - if off: absolute frequency
- Waveshape (quadrant picker)
- lfo with depth to ratio
- ratio uses 2^x table

FM Matrix Node
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
- Voice Manager has Mono and Mono Legato mode - turn them on?
- Interpolate ratio across the block in source

Installer
- mac signing
- win/lin zip only
- pipelines on all three platforms

Probably ship at this point but then

Non-monophonic voice level modulation
- CLAP advanced features like polymod
- Note expressions?
- MPE?


Sunday
- LFO on Mixer Node to level or pan
- LFO mul or plus on consistently on all three nodes
- LFO Level Built In
- AM is somehow not quiet right. Signal to zero seems 'no modulation' not 'no output' 
- preset factory set with cmakerc
- preset scan user directory to make tree
- param split for voices and base/top
- temposync lfo
- deal with ui attached or not for messages
- edit area says "click a knbo to edit on startup"