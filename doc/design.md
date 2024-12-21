Individual Oscillator

- Keytrack
  - if on: ratio
  - if off: absolute frequency
- Waveshape (quadrant picker)
- lfo with power and depth to ratio
- dahdsr with power and depth to ratio
- ratio uses 2^x table

FM Matrix Node
- DAHDSR to scale float depth
- LFO to scale float depth

Self matrix node
- basically just an FM level with an envelope and LFO

Mixer
- Each node gets an envelope an LFO and a pan - do stereo here not in the output node

MIDI mappings what to do
- velocity to amplitude in main
- Note expressions?
- Other MIDI?

Other ToDos
- Envelope Rate Linear to 2x provider
- General UI

Installer

Friday
- envelope power for subs
- simple lfo in feedback
- typeins
- lfo support
- restore rm with each matrix node having a to-rm-or-to-fm option
- auv2
- vst3
- a build
- deal with ui attached or not for messages
