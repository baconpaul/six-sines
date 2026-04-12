# Six Sines 1.2 Roadmap

I have a lot of ideas for a '1.2' version of six sines. Not 2.0. Still compatible. But expanded. Here's my rough list

## Audio Input

- Add an audio input and allow operator 1 to take the audio input
- Tricky part: have to upsample from audio input rate to six sines rate, so there will be some input latency. 
- Mostly useful for really some FSU FM work.

## User Wavetables

- Allow a user to load a wav and take L channel (if stereo) or mono as a single cycle wavetable.
- Probably allow CSV of floats also
- Gotta upsample to 4096 and take derivs to make it work like tables
- Gotta stream into patch
- One per operator? Or one per patch?

## CZ-style PD features

- We already have PD so this really amounts to diferent operator styles
- The CZ basically has enveloped tuned waveforms it seems which are strictly positive as PD sources
- Do some research to se if we can scope it that way and get some interesting results.

## AllPassNetwork as modulation style

- Remember that super cool paper on all pass networks as phase modulation?
- Well make that a mode if I can. 
- Not sure if this fits in the architecture properly - might have to do all-pass mods at the end after the signal isotherwise made etc

## Super-Macros

- Names
- LFOs and Envelopes per voice with power siwtch
- In-use display

## UI Color Editor

- Make sure everything is themed
- Write a jucegui theme editor which allows constaints to used subset I guess

## UI Ratio editor Upgrade

The ratio editor is a knob. That's clumsy. segmented control will be better. 
Want modes - like segmented float, ratio as pair of ints, etc...
Some of those modes like ratio as pair of ints requires extra streaming to store and retain
Probably want a legacy mode of knob

## Plugin Edges

- Note Expressions?
- Clap Polymod?
- Definite maybe at best for these tho

## Infrastructure

- Clap Wrapper Standalone upgrades
  - windows ui open isn't right
  - jack on linux

## Visualization

- That fun explicig DFT as spectrum
- Show the wavetable somehow?
- or maybe do none of this

## Pattch Upgradeds

- Add an author field for goodness sake and show it in the UI somwher
