# SixSines ChangeLog

# Changelog

## 1.2.0 (v1.1.0 → current)

These are the changes in the 1.2.0 release candidate (as of 2b0c0193a9)

### Sources

There are multiple changes to source nodes. They now contain Phase Distortion (CZ-like) and Noise
sources, support audio input, and more

- Extended mode: phase remap allows distortion of the phase
- Extended mode: resonant sweep allows underlying pitch sweeps with windows
- Extended mode: Noise provides pink, white, tilt, and LFSR/Chip-style noise in a node
- Operator 1 can set its source to audio sidechain input
- Each node has an absolute hz offset

### Modulation and SuperMacros

- All envelopes support temposync
- All nodes with LFOs get a step sequencer as well as curve-style LFOs
- LFOs on amplitude-like nodes get an add, scale and attenuate mode similar to the envelope
- Add a song-position run mode for LFOs — lock phase to the host timeline instead of note attack 
- SuperMacro: macros become per-voice mod sources with their own DAHDSR + LFO + 3-slot mod matrix
- Add envelope rate as a modulation target (exponential, ±8x)
- Add MPE Timbre (bipolar) as a modulation source
- Make AShape, RShape, and DShape modulatable in the matrix

### The Output Stage

- Add a variety of filter saturation and re-sampling options in the output stage to tame aliasing under high saturation / low-rate ZOH
- These allow sound changes which give you a more 'beat up' or 'old' sound in some cases, but can produce
  delightful or unexpected artifacts in others. Use your ears!


### Other DSP

- Engine-wide MIDI/MPE/note-expression (25 ms) and param-automation (2 ms) smoothing times 
- Smooth per-voice MPE and CLAP note-expression values with a 5 ms one-pole lag 
- Move MPE active/bend range out of the patch onto the engine so preset changes no longer wipe MPE config (#374)
- Track unison spread and pan continuously while a voice is playing
- Implement MTS-ESP-aware MPE pitch bends

### UI

- The enture UI is turned 'sideways' giving loads more room, with many re-layouts and changes
- The UI has a color theme editor so you can re-color the UI to your hearts content
- The synth has an analyzer showing playing spectrum and waveform. Press the button next to the VU meter
- The ratio editor has a new segmented widget in addition to a knob. Press the '...' in the source area to swap
- Add a patch AUTHOR field plus the workflow to set/default it
- Add a per-op VU meter
- Add design mode helpers for patch editing (run all nodes independent of power; all-sounds-off on power toggle)

### Infrastructure

- Change user-dir vendor resolution order so a 1.1 scan doesn't disrupt 1.2
- Save MPE bend range and smoothing times as user defaults that seed the engine on startup
- Add an error reporting path
- Update the param rescan mechanism to use onMainThread and be more parsimonious (#350)
- Allow AUv2 versioned params; add tests
- Various workflow & installer fixes
- Build on Linux ARM 
- Implement preset discovery (tested against REAPER dev)
- Add a Windows installer and better CMake targets
- Move to C++20 and latest libraries 

### Patches

- New patches from djTubig

## v1.1.0

v1.1.0 was developed as part of the collaboration with the [One Synth Challenge](https://www.kvraudio.com/forum/viewtopic.php?t=618178) community
and other early users. It contains some substantial feature upgrades and some bug fixes not in 1.0.5

- Synth Features
  - Add a 'Six Sines, Seven Outs' plugin variant which has an individual vst out per operator
    - Unison voices can choose a strategy for which bus to particpate in, allowing center-only-to-main
      and other strategies
  - Add a very-low-frequency option for non-keytracked operators, allowing operation from 0-10hz
  - Add a unison width control
  - The mod matrix nodes (op3->op5, etc...) have a 10x depth control
  - Allow the last point of the enelope of lfo to participate in the node mod matrix
  - Add an 'Envelope Retrigger from Zero' mode
  - Ratio (Fine) as well as Ration available as target in operator matrix
  - Smooth the velocity source in the mod matrix to avoid jumps when using velocity
    as a modulator in legato modes
  - Add 'Linear' and 'ZOH' resampler options
  - Fix the TX tables to match the TX81Z more accurately; rename the prior incorrect tables
    to 'spiky' variants
  - Add a 'coarse' tune knob in addition to a 'fine' tune
  - Portamento Continuation allows porta to restart on release, on new voice, and so on
  - Fix several temposync bugs in the LFO
  - Add TZLinear FM and Exponential FM as modulation modes for the mod matrix, in addition
    to PM and RM; add scaling (abs, unipolar, normal) modes to the RM
  - Add 'Solo' feature to the mixer
  - Update the voice manager, allowing full note id support in VST3 and CLAP in legato modes
  - Add a collection of 'Window' style waveforms, useful for AM/RM modelling
  - Add an LFO Start Phase control
  - Fix a problem where the center voice was mis-identified in unison in some cases

- User Interface
  - Add a suite of accesibility fixes to various controls
  - Add UI scaling from 75-150%
  - Add a light-mode skin
  - Allow fractional typeins on ratios
  - You can copy and paste nodes or node regions between nodes.
  - Consistent dirty flag displays in UI when patch is modified and unsaved to disk or DAW
  - Temposync string values reflect properly in clap parameter displays
  - Temposync controlls allow typein like '1/4' or '1/16.' or '1/8T'
  - Ratio buttons have a jog control
  - Add an option to reposition sources and matrices
  - Fix a problem with mouse wheel on macos when using an actual mouse (as opposed to trackpad)
  - Fix a problem with tooltip hover sticking
  - Fix a problem where the jog buttons on patch selector would mis-jump after saving a patch
  - Obey upper bound correctly on DAHDR typeins

- Plugin related improvements
  - Add parameter smoothing to clap params
  - Add support for CLAP and VST3 pan, tune, and volume note expressions
  - Wrap all parameter changes in begin/end consistently
  - Call `gui::set_size` consistently on scale changes, fixing a windows reaper sizing issue
    
- Code Improvements
  - Remove some troublesome uses of thread_local storage in envelopes and elsewhere
  - Place visual indication of a debug build in the UI
  - Move to a list/grid based layout; port the code back to sst-jucegui and adjust screens
  - Consolidate target locations when building both in pipeline and locally
  - Move juce LookandFeel management to the shared sst-jucegui pacakge
  - Substantial improvements to the performance of the LFO and Envelope classes under
    constant rate.
  - Clean up the patch load / sync mechanism to be more thread-aware
  - Set modsource param max to allow all mod sources. (the 'UNK2048' problem)
  - Implement (but leave off) the clap preset-factory mechanism.

- Infrastructure
  - Move the macOS minimum to 10.14
  - Build with docker ubuntu 20 image for linux
  - Add an option to disable the juce Software renderer on Windows
  - Fix a problem with UTF16 paths and patch loading on Windows.
  - Fix a problem with keyboard input in the VST3 in Studio1 and Live on Windows.

- Documentation and Content
  - Upgrade the manual to include the wonderful videos from Taron
  - New patches from Videco
  - New patches from SiL3NC3
  - Fix a few patches which were tuned off by 7 semitones

## v1.0.5, .4, .3, .2, and .1

v1.0.0-v1.0.5 contains a set of changes we found in the days after 1.0.0

- Make the minimum macos version 10.14
- Build the linux distribution with a ubuntu 20 docker image
- Adjust the clap wrapper to work correctly in Logic/AUv2 at non 44.1 sample rates
- Inform the clap host of param changes on reload

## v1.0.0

v1.0.0 is the first release of the synth, with core features intact