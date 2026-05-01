# Six Sines 1.2 Roadmap

I have a lot of ideas for a '1.2' version of six sines. Not 2.0. Still compatible. But expanded. Here's my rough list

## CZ-style PD features **UNDERWAY**

- Implement phase remap **done**
- Implement phase formant sweep + window style

## Upgrades to the Resampler, Final path, and Reconstruction

- Make the upsample / downsample path have more control and better viz
- Specifically have the pipeline be
  - Run at internal sample rate to make high bit rate clean signal
  - Soft drive saturator (use the cubic clamp form) with optional small drive
  - Add a cytomic SVF at 16khz / root 2 resonance
  - ZOH decimate the signal to 32khz at upper rate
  - Do a bitheight redicution (round(f * (1<<b)) / (1<<b))
  - Downsample with strategy
- Each of those steps except the first and last is optional.
- basically all options to make it less 'clean' which all toggle at internal block


## Granular FM

- that crazy idea kisney and i chatted about
- more t/k

## UI Ratio editor Upgrade

The ratio editor is a knob. That's clumsy. segmented control will be better. 
Want modes - like segmented float, ratio as pair of ints, etc...
Some of those modes like ratio as pair of ints requires extra streaming to store and retain
Probably want a legacy mode of knob

## Infrastructure

- Clap Wrapper Standalone upgrades
  - windows ui open isn't right
  - jack on linux

## Visualization

- That fun explicig DFT as spectrum
- Show the wavetable somehow?
- or maybe do none of this

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
