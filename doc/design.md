Individual Oscillator

+ stereo phase and rm input
+ Stereo out
- power
- Keytrack
  - if on: ratio
  - if off: absolute frequency
- Waveshape (quadrant picker)
- feedback
- will need gated somewhere
- lfo with power and depth to frequency
- dahdsr with power and depth to frequency

FM Matrix Node
+ Has a reference to an input and output 
+ float depth
- pan (for application)
- RM or FM
- DAHDSR to scale float depth
- LFO to scale float depth
- (its a product if both are on)

Self matrix node
- basically just an FM level with an envelope and LFO

Evaluator

Mixer
- Each node gets an envelope an LFO and a pan and level control

Final output lanczos downsampler (and gSR to 120khz or some such)

MIDI mappings what to do
- velocity to amplitude everywhere of course

Other ToDos
- Envelope Rate Linear to 2x provider