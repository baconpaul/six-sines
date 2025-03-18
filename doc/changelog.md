# SixSines ChangeLog

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