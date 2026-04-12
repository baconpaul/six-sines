# Audio In Feature — Prompts and Plan

**Date:** 2026-04-12

---

## User Prompts (verbatim)

### Prompt 1

OK so today we are adding audio in to six sines. I want to start with a plan-only exercise for you to present me how you would solve it, and then if i like the plan we can generate code. Go laod up the six sines skills and memories you have first.

Audio in works as follows

1. The clap gets changed so that the audioPorts api responds with 1 input which is not main and is stereo. Fine.
2. The processor should have a lanczos interpolation operator from basic-blocks which interpolates from clap sample rate up to internal sample rate. If the internal sample rate changes, clear the buffer. On construction, clear the buffer. Make the Lanczos nice and big. Give it like 1024 * 10 samples worth of storage or something. On each sample the audio in wrotes to the lanczos buffer.
3. At the top of each internal block the synth engine reads the upsampled version of that lanczos buffer into an internal block siuzed resampled buffer.
4. Operator 1 - and only operator 1 - can have audio in as a wavetable. It uses the upsampled buffer from step 3 when selected. Add the new type "AUDIO_IN" as a wavetable.
5. Once operator 1 is set to audio in the rest of the engine will work fine with a couple of constraints. Op1 feedback wont work, and op1 ratio does nothing. So AUDIO_IN is special in that we dont have to run the op1 lfo, envelope, etc... on the node or the feedback node just the mix node.
6. The UI side operator 1 audio in appears last in the list after a spearator. When it appears the ratio knob deactivates (but still allows skip to select) as do all the modulation and envelope parameters and the feedback knob. 

So that's basically the design idea. Can you give me a plan which restates this design and how you intend to do it.

### Prompt 2

OK so a few modifications to the plan, which I'd like you to make and then restate the plan. Mostly AUDIO_IN being 21 as special. In this iteration of six sines I'm going to add several new waveforms. AUDIO_IN is this but later on I'll add a bunch of phase distortion ones. So I dont want AUDIO_IN being last to be special (but your analysis that it is streaming compatible to add is fine). So what I want to do is add a ui-side feature to sort and segment the waveform menu. We can cluster with separators and reorder. So basically a separator before TX2, before SPIKY TX and before the windows, then a separator and audio in last. We can just have a little data structure for separators and order and check that all values are in the menu. Then condition for operator 1.

As to the nodes, if a node is set to AUDIO_IN other than 1, lets for now silence it but have that code there in case later we decide to let audio in be in other nodes.

So restate the plan with those changes for my review please.

### Prompt 3

Great. I'd like to save my prompts and your plans. I'm going to do this often. So please save exactly verbatim the prompts I entered here along with the final version of your plan along with the date to @doc/12_prompts/audio_in_prompt.md. Like I said verbatim what I prompted in the top of the document (including this) then the output of the last version of the plan.

---

## Final Plan (Revised)

### Overview

Stereo audio input is added to the plugin. Op1 (and only Op1) can be set to `AUDIO_IN` waveform mode, which replaces its synthesized waveform with the live upsampled audio input. The audio arrives at host rate, is upsampled to engine rate via a Lanczos resampler in Synth, and the upsampled block is written into `MonoValues` where `OpSource` can read it. If AUDIO_IN is selected on ops 2–5, the op silences — the code path is there for potential future expansion.

`AUDIO_IN` is assigned value 21 (the current `NUM_WAVEFORMS`). Future phase-distortion waveforms will be added as 22, 23, etc. A new UI data structure governs menu display order and separators independently of the integer enum order, so `AUDIO_IN` always appears last in the menu regardless of what integer values future waveforms get.

---

### Step 1: CLAP Audio Ports (`src/clap/six-sines-clap.cpp`)

**`audioPortsCount`**: Change `isInput ? 0` → `isInput ? 1`. Both single and multi-out variants report one input.

**`audioPortsInfo`**: Remove the `assert(!isInput)`. Add a branch for `isInput && index == 0`:
- Unique `id` (e.g. `82649`)
- `name = "Audio In"`
- `flags = 0` — explicitly **not** `CLAP_AUDIO_PORT_IS_MAIN`
- `channel_count = 2`, `port_type = CLAP_PORT_STEREO`
- `in_place_pair = CLAP_INVALID_ID`

**`process()`**: Before the per-sample loop, capture input data pointers:
```cpp
float *audioInL{nullptr}, *audioInR{nullptr};
if (process->audio_inputs_count > 0 && process->audio_inputs[0].data32) {
    audioInL = process->audio_inputs[0].data32[0];
    audioInR = process->audio_inputs[0].data32[1];
}
```
Inside the per-sample loop at every `s`, push one host-rate sample into the engine's resampler:
```cpp
engine->pushAudioIn(audioInL ? audioInL[s] : 0.f,
                    audioInR ? audioInR[s] : 0.f);
```
This is unconditional on `blockPos` — every host-rate input sample is pushed as it arrives.

---

### Step 2: Engine Audio-In Resampler (`src/synth/synth.h` / `synth.cpp`)

**`synth.h`** — Add members:
```cpp
using audioInResampler_t = sst::basic_blocks::dsp::LanczosResampler<blockSize>;
std::unique_ptr<audioInResampler_t> audioInResampler;
void pushAudioIn(float L, float R);
```

`LanczosResampler` has `BUFFER_SZ = 4096` host-rate samples of internal ring storage, which is far more than the 2–8 samples of lookahead the Lanczos kernel requires. This is the "nice and big" buffer; no changes to the SST library are needed.

**`synth.cpp`** — In `setSampleRate()`, after computing `hostSampleRate` and `engineSampleRate`, (re)construct the resampler — this clears it implicitly since the constructor `memset`s its buffers:
```cpp
audioInResampler = std::make_unique<audioInResampler_t>(
    (float)hostSampleRate, (float)engineSampleRate);
```

**`pushAudioIn()`**: Delegates to `audioInResampler->push(L, R)`.

---

### Step 3: MonoValues Audio-In Block (`src/synth/mono_values.h`)

Add to `MonoValues`:
```cpp
float audioInBlock alignas(16)[blockSize]{};  // engine-rate audio in, mono mix
```

This shared buffer is updated once per engine block by `processInternal`, before any voice rendering runs.

---

### Step 4: `processInternal` — Populate Audio-In Block (`src/synth/synth.cpp`)

At the top of the inner `while (generated < blockSize)` loop, before rendering voices, pull one engine-rate block:
```cpp
{
    float aiL[blockSize], aiR[blockSize];
    auto got = (int)audioInResampler->populateNext(aiL, aiR, blockSize);
    for (int i = 0; i < blockSize; ++i)
        monoValues.audioInBlock[i] = (i < got) ? (aiL[i] + aiR[i]) * 0.5f : 0.f;
}
```
Mono-mixes L+R since Op1 is a mono signal source in the FM graph. Zero-fills if the resampler doesn't have enough input yet (startup transient).

---

### Step 5: Waveform Enum Extension (`src/dsp/sintable.h`)

Add `AUDIO_IN` immediately before `NUM_WAVEFORMS`:
```cpp
TUKEY_WINDOW = 20,
AUDIO_IN = 21,      // no table entry; handled as special case in OpSource
NUM_WAVEFORMS = 22
```

`AUDIO_IN = 21` is a stable streaming value. Future phase-distortion waveforms will be inserted as 22, 23, … in the enum. The UI menu (Step 7) places `AUDIO_IN` last regardless of integer order. The `SinTable::at()` function is **never called** for `AUDIO_IN` — `OpSource` early-exits before reaching table lookup.

---

### Step 6: Patch Parameter Range Update (`src/synth/patch.h`)

Extend the `waveForm` parameter range by one (to include index 21). All 6 ops share the same parameter template so technically all ops can hold value 21 in state, but ops 2–5 will silence if that happens (Step 8). No `patchVersion` bump needed — this is a range expansion with no backwards-incompatible changes.

The display-name for value 21 in `ParamMetaData` should be `"Audio In"`.

---

### Step 7: New `src/ui/waveform-display.h`

This is a new header in `src/ui/` containing a data structure that governs the waveform menu's display order and grouping. It is the authoritative ordering source — completely independent of enum integer values.

```cpp
struct WaveformMenuEntry {
    bool isSeparator{false};
    const char *label{nullptr};   // group name for separators, display name for waveforms
    int waveformValue{-1};        // -1 for separators
};

// Base menu — all operators
inline constexpr WaveformMenuEntry waveformMenuBase[] = {
    {false, "Sin",              SinTable::SIN},
    {false, "Sin Fifth",        SinTable::SIN_FIFTH},
    {false, "Squarish",         SinTable::SQUARISH},
    {false, "Sawish",           SinTable::SAWISH},
    {false, "Triangle",         SinTable::TRIANGLE},
    {false, "Sin of Cubed",     SinTable::SIN_OF_CUBED},
    {true,  "TX"},
    {false, "TX 2",             SinTable::TX2},
    {false, "TX 3",             SinTable::TX3},
    {false, "TX 4",             SinTable::TX4},
    {false, "TX 5",             SinTable::TX5},
    {false, "TX 6",             SinTable::TX6},
    {false, "TX 7",             SinTable::TX7},
    {false, "TX 8",             SinTable::TX8},
    {true,  "Spiky TX"},
    {false, "Spiky TX 2",       SinTable::SPIKY_TX2},
    {false, "Spiky TX 4",       SinTable::SPIKY_TX4},
    {false, "Spiky TX 6",       SinTable::SPIKY_TX6},
    {false, "Spiky TX 8",       SinTable::SPIKY_TX8},
    {true,  "Windows"},
    {false, "Hann",             SinTable::HANN_WINDOW},
    {false, "Blackman-Harris",  SinTable::BLACKMAN_HARRIS_WINDOW},
    {false, "Half BH",          SinTable::HALF_BLACKMAN_HARRIS_WINDOW},
    {false, "Tukey",            SinTable::TUKEY_WINDOW},
    // Future phase-distortion waveforms (22+) inserted here, before Audio In
};

// Op1-only extension — appended to base
inline constexpr WaveformMenuEntry waveformMenuAudioIn[] = {
    {true,  ""},                              // separator before Audio In
    {false, "Audio In", SinTable::AUDIO_IN},
};
```

**Helper functions** (also in this header or an associated `.cpp`):
- `waveformDisplayName(int value) -> const char*` — looks up label from base + audioIn tables
- `nextWaveformValue(int current, bool includeAudioIn) -> int` — finds next non-separator entry in menu order
- `prevWaveformValue(int current, bool includeAudioIn) -> int` — same, backwards

**Validation**: A `static_assert` or debug-build `assert` that iterates all `SinTable::WaveForm` values 0..20 and verifies each appears exactly once in `waveformMenuBase`. Similarly verify `AUDIO_IN` appears exactly once in `waveformMenuAudioIn`. This catches any future waveform being added to the enum without being added to the menu.

---

### Step 8: `OpSource` — `opIndex` Member and AUDIO_IN Mode (`src/dsp/op_source.h`)

Add `int opIndex{0}` as a member of `OpSource`. It is set from `Voice` after array construction (see Step 9).

At the top of `renderBlock()`, after the `!active` early-out, add:

```cpp
bool isAudioIn = ((int)std::round(waveForm) == SinTable::AUDIO_IN);
if (isAudioIn) {
    if (opIndex == 0) {
        for (int i = 0; i < blockSize; ++i)
            output[i] = monoValues.audioInBlock[i] * rmLevel[i];
    } else {
        // AUDIO_IN only wired for Op1 currently; silence other ops
        memset(output, 0, sizeof(output));
    }
    fbVal[0] = fbVal[1] = 0.f;
    return;
}
```

Ring-mod (`rmLevel`) is still applied for Op1 — matrix connections to Op1 can still ring-modulate the audio input, which is a useful FM technique. Feedback state is zeroed to prevent stale values if the mode is toggled. All ratio/phase/envelope/LFO code is bypassed.

---

### Step 9: Voice — Set `opIndex` and Skip Feedback for Op0 Audio-In (`src/synth/voice.cpp`)

In `Voice::Voice()`, after constructing `src`, set indices:
```cpp
for (int i = 0; i < numOps; ++i)
    src[i].opIndex = i;
```

In `Voice::renderBlock()`, for operator index 0, skip `selfNode[0]` when in AUDIO_IN mode:
```cpp
bool op0IsAudioIn = ((int)std::round(src[0].waveForm) == SinTable::AUDIO_IN);
if (!op0IsAudioIn) {
    selfNode[0].renderBlock(src[0].feedbackLevel, src[0].output);
}
src[0].renderBlock();
```

`matrixNode` connections FROM op0 TO ops 1–5 still run unmodified — the audio signal in `src[0].output` can modulate higher operators. `mixerNode[0]` still runs — output level/envelope/LFO for Op1 continue to work.

---

### Step 10: WavPainter Fix (`src/ui/source-sub-panel.cpp`)

`WavPainter::paint()` currently calls `st.at(phase)` without checking for `AUDIO_IN`. When `waveForm == AUDIO_IN`, skip the waveform drawing and instead render a representative placeholder (e.g., a flat line or a noise-like squiggle label "Audio In"). The simplest approach: check at the top of `paint()` and early-return with a text label if `AUDIO_IN`.

---

### Step 11: UI — Waveform Selector for Op1 (`src/ui/source-sub-panel.cpp`)

**Custom JogUpDownButton navigation**: After `createComponent(...)` creates `wavButton`/`wavButtonD`, override the data binding's navigation. The JogUpDownButton calls `setValueFromGUI(getValue() ± 1)` for up/down. To intercept this, either:
- Set custom up/down callbacks on `wavButton` that call `nextWaveformValue`/`prevWaveformValue` from `waveform-display.h` and then call `wavButtonD->setValueFromGUI(newValue)`
- Or subclass `PatchDiscrete` into a `PatchDiscreteWaveform` that overrides `setValueFromGUI` to remap through menu order

Either way, the navigation skips separator entries cleanly. The display string comes from `waveformDisplayName()`.

**Op1 gate for AUDIO_IN**: The `includeAudioIn` flag passed to `nextWaveformValue`/`prevWaveformValue` is `(index == 0)`. For ops 2–5, navigation never reaches value 21.

**`setEnabledState()`**: Extend to check for AUDIO_IN on Op1. When `index == 0 && waveForm == AUDIO_IN`, disable:
- `envToRatio`, `envToRatioFine`
- `lfoToRatio`, `lfoToRatioFine`
- All DAHDSR sliders and shape knobs
- All LFO controls
- All modulation slots
- `startingPhase` (no phase in audio-in mode)
- `keyTrack`, `keyTrackValue`, `keyTrackLow`, `keyTrackLowValue` (irrelevant)
- `tsposeButton` (ratio is meaningless)

`wavButtonD->onGuiSetValue` already calls `wavPainter->repaint()` and `wavButton->repaint()` — extend it to also call `setEnabledState()` so the greying updates immediately when AUDIO_IN is selected.

Also add a notification back to `SourcePanel` so the ratio knob in the top panel greys out. The `onGuiSetValue` callback can call into the editor or panel to trigger an `updateEnabledState()` on `SourcePanel`'s knobs[0].

---

### Step 12: Self Sub Panel — Feedback Deactivation (`src/ui/self-sub-panel.cpp`)

In `SelfSubPanel::populate()` (or equivalent), add a check: when the selected operator index is 0 and `waveForm == AUDIO_IN`, call `setEnabled(false)` on the feedback slider and its label. Feedback mode is a no-op for AUDIO_IN since `selfNode[0]` is skipped entirely.

---

### Summary of Files Changed

| File | Change |
|------|--------|
| `src/clap/six-sines-clap.cpp` | Add input port; push audio samples to engine per host sample |
| `src/synth/synth.h` | Add `audioInResampler`, `pushAudioIn()` |
| `src/synth/synth.cpp` | Construct resampler in `setSampleRate()`; populate `monoValues.audioInBlock` in `processInternal()` |
| `src/synth/mono_values.h` | Add `audioInBlock[blockSize]` |
| `src/dsp/sintable.h` | Add `AUDIO_IN = 21` before `NUM_WAVEFORMS` |
| `src/synth/patch.h` | Extend waveform param range to include 21; add display name |
| `src/dsp/op_source.h` | Add `opIndex`; AUDIO_IN early-exit in `renderBlock()` |
| `src/synth/voice.cpp` | Set `src[i].opIndex = i`; skip `selfNode[0]` for AUDIO_IN |
| `src/ui/waveform-display.h` | **New file**: menu entries, separators, nav helpers, completeness validation |
| `src/ui/source-sub-panel.cpp` | Custom wavButton navigation via menu order; extend `setEnabledState()` for AUDIO_IN; fix `WavPainter` for AUDIO_IN |
| `src/ui/self-sub-panel.cpp` | Disable feedback when Op1 is AUDIO_IN |
| `src/ui/source-panel.h/cpp` | Add `updateOpEnabledState(int idx)` to grey out ratio knob + up/down buttons |
| `src/ui/matrix-panel.h/cpp` | Add `updateSelfKnobState(int idx)` to grey out feedback level knob + power toggle |

---

## Follow-up Prompt 4

OK I reviewed it and made two small changes. There's a couple of things I want to add also

1. There's no reason to do the lanczoes resample of audio in *unless* op 1 is set to AUDIO_IN so can we check that before buildign the audio in modval and just zero it otherwise
2. If op1 is set to AUDIO_IN the op1 feedback node knob in the UI can also be disabled.

So please make these changes (and add this ot the explicit prompts in the doc).

Then once that is done please spin up a fresh agent, ask it to review the diffs here, and provide feedback vs plan

### Changes made for Follow-up 4

1. **`src/synth/synth.cpp` `processInternal()`**: Guard the Lanczos resampler population behind `op1IsAudioIn = ((int)std::round(patch.sourceNodes[0].waveForm.value) == SinTable::AUDIO_IN)`. When not in AUDIO_IN mode, `monoValues.audioInBlock` is zeroed instead (no resampler work done).

2. **`src/ui/matrix-panel.h/cpp`**: Added `updateSelfKnobState(int idx)` which disables `Sknobs[idx]` (the `fbLevel` knob) and `Spower[idx]` (the active toggle) when that op is AUDIO_IN. Called from `SourceSubPanel::setEnabledState()` alongside `updateOpEnabledState()`.

---

## Follow-up Prompt 5

yes please address all three of those

*(Context: responding to a fresh review agent's report that flagged three gaps: DAHDSR/LFO/modulation slots not disabled in SourceSubPanel, missing menu completeness static_assert in waveform-display.h, and modulation slots not greyed.)*

### Changes made for Follow-up 5

1. **`src/ui/source-sub-panel.cpp` `setEnabledState()`**: Added `setEnabled(!isAudioIn)` for all 6 DAHDSR sliders, 3 shape knobs, trigger button, all 7 LFO controls (rate, deform, phase, shape, tempoSync, bipolar, isEnv), and all 3 modulation slots (sourceMenu, targetMenu, depthSlider).

2. **`src/ui/waveform-display.h`**: Added two compile-time `static_assert`s — one verifying every waveform value 0..`TUKEY_WINDOW` appears exactly once in `waveformMenuBase`, one verifying `AUDIO_IN` appears exactly once in `waveformMenuAudioIn`.

---

## Follow-up Prompt 6

Alright close. That disable of the knobs doesn't repaint the parent so add a call to editor->repaint. Also when the op1 feedback is knocked out we also want to knock out the envelope and envelope for that node.

### Changes made for Follow-up 6

1. **`src/ui/source-sub-panel.cpp` `setEnabledState()`**: Added `editor.repaint()` after the existing `repaint()` so the operator grid and matrix panel redraw when controls are greyed.

2. **`src/ui/self-sub-panel.cpp` `setEnabledState()`**: Extended to disable all DAHDSR sliders, shape knobs, trigger button, and all LFO controls when op1 is AUDIO_IN. Also added `editor.repaint()`.

---

## Follow-up Prompt 7

Great. Please add my other verbatim prompts to @doc/12_prompts/audio_in_prompt.md and also spin up another fresh review agent and let me know what it thinks

---

## Follow-up Prompt 8

yeah fix the clap audio push bug and then let me look by hand again. And add this to the audio_in_prompt.md also

### Changes made for Follow-up 8

1. **`src/clap/six-sines-clap.cpp` `process()`**: Changed audio input push from gated `if (audioInL && audioInR)` to unconditional `engine->pushAudioIn(audioInL ? audioInL[s] : 0.f, audioInR ? audioInR[s] : 0.f)`. This ensures the resampler always receives one sample per host frame, preventing startup transients or dropouts if a host provides null channel pointers.

---

## Follow-up Prompt 9

OK couple of things on hand review

1. When you set op1 to the source why do you multiply by rm[i]? We know that will be 1 so we can skip it
2. The static assert in waveform_display hard codes TUKEY_WINDOW. Instead what I'd like to do is make sure the result covers all of them up to NUM_WAVEFORMS - 1 *except* for AUDIO_IN, again because I plan to add more waveforms which will come after AUDIO_IN in the future.

### Changes made for Follow-up 9

1. **`src/dsp/op_source.h`**: Removed `* rmLevel[i]` from the AUDIO_IN copy loop — `output[i] = monoValues.audioInBlock[i]`. Ring-mod level is always 1.0 for op1 in AUDIO_IN mode since no matrix node assigns RM to it.

2. **`src/ui/waveform-display.h`**: Changed the base-menu `static_assert` to iterate `0..NUM_WAVEFORMS-1`, skip `AUDIO_IN`, and require every other value appears exactly once. This is forward-compatible: new waveforms added at 22, 23, etc. will automatically cause a build failure until added to the menu.

---

## Follow-up Prompt 10

OK great and one last review, and update the doc with these prompts

---

## Follow-up Prompt 11

Yeah lets fix 1 and 2. And with 2 lets add a comment in sin table that things will be easier if we keep audio in last and just update streaming version when adding table types so when I do add them you know to up streaming to n+1 and do a handler to push audio in always to the end. Make that change, add this prompt to doc, spin up a final review agent (including checking comments) and report back

### Changes made for Follow-up 11

1. **`src/ui/source-sub-panel.cpp` `setEnabledState()`**: Added `editor.selfSubPanel->setEnabledState()` call when `index == 0` and SelfSubPanel is visible, so feedback controls grey immediately if the user changes Op1's waveform while the feedback panel is open. Added `#include "self-sub-panel.h"` for the complete type.

2. **`src/ui/waveform-display.h` `WaveformPatchDiscrete::getMax()`**: For ops 2–5 (non-AUDIO_IN), now returns `NUM_WAVEFORMS - 2` instead of hardcoded `TUKEY_WINDOW`. This automatically includes any future synthesized waveforms added before AUDIO_IN without any change here.

3. **`src/dsp/sintable.h`**: Added comment block before `AUDIO_IN` explaining the streaming convention: new waveforms must be inserted before AUDIO_IN, AUDIO_IN's integer value will shift, so when adding them bump the SourceNode streaming version in `patch.h` and write a migration handler to remap the old AUDIO_IN value to the new one.
