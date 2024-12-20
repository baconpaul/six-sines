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
- velocity to amplitude everywhere of course
- Note expressions?

Other ToDos
- Envelope Rate Linear to 2x provider
- MTS/ESP support

Installer

Friday
- subeditors
- envelope power for subs
- simple lfo in feedback
- lfo support
- step support
- typein
