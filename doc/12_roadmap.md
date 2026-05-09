# Six Sines 1.2 Roadmap

I have a lot of ideas for a '1.2' version of six sines. Not 2.0. Still compatible. But expanded. Here's my rough list

## THings which came in after mostly done

- Reset Controls when automated for clap edge for things like sample rate

## Modulation
- Clamp targets
- LFO -> M etc... depth as an additive and attenuation target

## Noise Upgrades
- Pink, White, Tilt (with N for tilt) **DONE**
- Old BaconPaul Chip LFSR 'semi-tuned' (with N for 'sequence')

## Smaller Things from the crew
- Can we smooth MPE pitch bend with a lagger? (Or do we)
- Unison retuning slider could cubic rescale
- Mod target sliders scale matches display rescaling based on target (could be tricky)
- Now we have DES move MPE to the instance
   - if you load a stream which has it from the clap edge set the instance to that
   - if you load a patch from an sxsnp ignore the patch setting
   - move the mpe enabled param to be instance based (leave the param in to not break stuff but basically dont use it to drive mpe)

## Infrastructure

- Clap Wrapper Standalone upgrades
  - windows ui open isn't right
  - jack on linux

## MPE Pitch Bend **DONE**

- basically in mpe mode interpolate between keys for mts tuning in voice.cpp


## CZ-style PD features **done**

- Implement phase remap **done**
- Implement phase formant sweep + window style **done**

## Pink Noise as mode **DONE**

## UI Ratio editor Upgrade **DONE**

The ratio editor is a knob. That's clumsy. segmented control will be better.
Want modes - like segmented float, 

## Visualization **DONE**

- Add a simple built in spectrum and scope

## Upgrades to the Resampler, Final path, and Reconstruction **DONE**

- Make the upsample / downsample path have more control and better viz
- Specifically have the pipeline be
  - Run at internal sample rate to make high bit rate clean signal
  - Soft drive saturator (use the cubic clamp form) with optional small drive
  - Add a cytomic SVF at 16khz / root 2 resonance
  - ZOH decimate the signal to 32khz at upper rate
  - Do a bitheight redicution (round(f * (1<<b)) / (1<<b))
  - Add an optional highpass at 5-15hz to kill DC
  - Downsample with strategy
- Each of those steps except the first and last is optional.
- basically all options to make it less 'clean' which all toggle at internal block

## Super-Macros **DONE**

- Names
- LFOs and Envelopes per voice with power siwtch
- In-use display

## UI Color Editor **DONE**

- Make sure everything is themed **DONE**
- Write a jucegui theme editor which allows constaints to used subset I guess **DONE**

## Audio Input **DONE**

- Add an audio input and allow operator 1 to take the audio input
- Tricky part: have to upsample from audio input rate to six sines rate, so there will be some input latency.
- Mostly useful for really some FSU FM work.

## Patch Upgradeds **DONE**

- Add an author field for goodness sake and show it in the UI somwher


# 1.3 Roadmap (pushed from 1.2)

## Granular FM

- that crazy idea kisney and i chatted about
- more t/k

## User Wavetables (May not do)

- Allow a user to load a wav and take L channel (if stereo) or mono as a single cycle wavetable.
- Probably allow CSV of floats also
- Gotta upsample to 4096 and take derivs to make it work like tables
- Gotta stream into patch
- Since we want sharing these are probably 'patch level' slots which the node adddresses
- Really worried about aliasing here.

## Clap Polymod and Note Expressions 

- Note Expressions?
- Clap Polymod?
- Definite maybe at best for these tho

## AllPassNetwork as modulation style

- Remember that super cool paper on all pass networks as phase modulation?
- Well make that a mode if I can.
- Not sure if this fits in the architecture properly - might have to do all-pass mods at the end after the signal isotherwise made etc
