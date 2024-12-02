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

Evaluator

Mixer

Final output lanczos downsampler (and gSR to 120khz or some such)