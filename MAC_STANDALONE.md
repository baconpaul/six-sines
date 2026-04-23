# macOS Standalone: Code Review & Bug Analysis

**Reviewed:** `libs/clap-libs/clap-wrapper/src/detail/standalone/macos/`  
**Files:** `AppDelegate.h`, `AppDelegate.mm`, `StandaloneFunctions.mm`, `Info.plist.in`, `MainMenu.xib`, plus shared `standalone_host_audio.cpp`, `standalone_host_midi.cpp`, `entry.cpp`  
**Date:** 2026-04-03  
**Context:** Critical code review of the macOS standalone host for CLAP plugins.

---

## Architecture Summary

The macOS standalone uses a conventional Cocoa application structure:

1. **Entry point** — `wrapasstandalone.mm` calls `NSApplicationMain()`
2. **XIB** — `MainMenu.xib` loads the window and wires `ClapWrapperAppDelegate` as the app delegate
3. **App delegate** — `AppDelegate.mm` drives all plugin, GUI, and audio setup from `applicationDidFinishLaunching:` via a 0.001 s one-shot timer
4. **Plugin/audio** — `entry.cpp` and shared `standalone_host_*.cpp` own the plugin instance and RtAudio/RtMidi streams

---

## Critical Bugs

### ~~Bug 1 — Retina Scale Factor Hardcoded to 1.0~~ (RETRACTED)

**File:** `AppDelegate.mm` (inside `doSetup`)

```objc
ui->set_scale(p, 1);   // harmless; ignored per CLAP spec for Cocoa
```

This was originally flagged as critical, but the CLAP GUI spec explicitly states for `CLAP_WINDOW_API_COCOA`:

> *uses logical size, don't call clap_plugin_gui->set_scale()*

Cocoa plugins work in logical pixels and handle their own Retina scaling internally (via NSView backing scale APIs). The `set_scale()` call is specified to be ignorable — a conforming plugin returns `false` and does nothing. The `1.0` passed here is a harmless defensive call in case a non-conforming plugin needs a nudge, but it is not the cause of any rendering problem.

**Retina rendering quality is entirely the plugin's responsibility under Cocoa.** The host's only obligation is to add `NSHighResolutionCapable` to the Info.plist (Bug 2), which opts the process into high-resolution mode so the window system does not pre-scale the backing store away from the plugin.

---

### Bug 2 — `NSHighResolutionCapable` Missing from Info.plist (CRITICAL — compounds Bug 1)

**File:** `Info.plist.in`

Without `NSHighResolutionCapable = true`, macOS opts the app into legacy low-resolution mode and will not pass a 2× backing scale through the window system at all. The plugin GUI renders in a blurry, scaled-up bitmap regardless of what `set_scale()` is told.

Required addition to `Info.plist.in`:

```xml
<key>NSHighResolutionCapable</key>
<true/>
```

---

### Bug 3 — Spinlock in the Audio Callback (HIGH — causes audio dropouts)

**File:** `standalone_host.cpp`

```cpp
ClapWrapper::detail::shared::SpinLockGuard processLockGuard(processLock);
```

The RtAudio callback acquires a spinlock on every audio block. If the main thread holds this lock while doing any GUI work, the audio thread busy-waits — consuming a full CPU core and causing glitches, dropouts, and system unresponsiveness. This is a well-known anti-pattern in real-time audio code. Spinlocks are never safe in an audio callback path. The correct approach is to use lock-free atomic flags or message queues to communicate state changes without blocking the audio thread.

---

### Bug 4 — No Error Checking After `ui->create()` (HIGH — silent invisible window)

**File:** `AppDelegate.mm` (`doSetup`)

```objc
if (!ui->is_api_supported(p, CLAP_WINDOW_API_COCOA, false))
    LOGINFO("[WARNING] GUI API not supported");

ui->create(p, CLAP_WINDOW_API_COCOA, false);   // return value ignored
ui->set_scale(p, 1);                            // return value ignored
ui->get_size(p, &w, &h);                        // if this returns 0,0 → invisible window
ui->set_parent(p, &win);                        // return value ignored
ui->show(p);                                    // return value ignored
```

If `create()` fails, every subsequent call operates on undefined state. If `get_size()` returns `(0, 0)`, `setContentSize:` produces a zero-size window — visible in the Dock but showing nothing. None of these return values are checked, and no error is surfaced to the user.

---

### Bug 5 — MIDI Queue Missing Memory Fence (HIGH — data races on Apple Silicon)

**File:** `standalone_host_midi.cpp` / shared `fixedqueue`

The lock-free queue used to pass MIDI events from the RtMidi callback thread to the audio thread uses an atomic index but no explicit memory fence between the element write and the index update:

```cpp
_elements[_head] = *val;          // array write
_head = (_head + 1) & _wrapMask;  // atomic index update — but no fence before this
```

On Apple Silicon (ARM), the store-release guarantee on `_head` does not order the preceding array write without an explicit `std::atomic_thread_fence(std::memory_order_release)` between them. The audio thread can read a partially-written MIDI event. This can manifest as wrong note values, stuck notes, or silent corruption.

---

### Bug 6 — Memory Leak in AudioSettingsWindow (MEDIUM)

**File:** `AppDelegate.mm` (Audio Settings handler)

```objc
auto *window = [[AudioSettingsWindow alloc]
    initWithContentRect:windowRect
    styleMask:...
    backing:NSBackingStoreBuffered
    defer:NO];
```

If ARC is not enabled (the Objective-C++ mix in this project makes it easy to inadvertently compile without ARC), this `alloc/init` window is never released. Each time the user opens the Audio Settings panel, a new `AudioSettingsWindow` is allocated and leaked. The same pattern is used for the `NSAlert` instances at multiple sites (plugin load failure, audio error dialogs).

---

## Setup Timing Issues

### The 0.001 s Deferred Setup

**File:** `AppDelegate.mm` (`applicationDidFinishLaunching:`)

```objc
[NSTimer scheduledTimerWithTimeInterval:0.001
                                target:self
                              selector:@selector(doSetup)
                              userInfo:nil
                               repeats:NO];
```

All plugin creation, GUI embedding, and audio initialization happen inside this 0.001 s timer callback. The intent is to let the run loop complete its first spin before doing work. In practice, 1 ms is not a meaningful guarantee — on a loaded system this fires at an arbitrary time. More significantly:

- The window is shown (`orderFrontRegardless`) **before** the plugin GUI is embedded. If setup is slow the user briefly sees an empty window.
- If setup fails partway through, the window remains visible with no content and no error.

### Run Loop Mode for the Callback Timer

**File:** `AppDelegate.mm`

```objc
[runLoop addTimer:self.requestCallbackTimer forMode:NSDefaultRunLoopMode];
```

The timer that handles `on_main_thread` plugin callbacks is added to `NSDefaultRunLoopMode` only. During live window resizing or menu tracking, macOS switches the run loop to `NSEventTrackingRunLoopMode`. The callback timer does not fire in that mode, so plugin `on_main_thread` callbacks are silently delayed until the user stops interacting with the UI.

Fix: use `NSRunLoopCommonModes` instead.

---

## Window Sizing

### Variable Shadowing in Resize Logic

**File:** `AppDelegate.mm` (`windowWillResize:toSize:`)

```objc
NSWindow *w = sender;       // ← outer 'w'
// ...
uint32_t w, h;              // ← inner 'w' shadows NSWindow *w
plugin->_ext._gui->get_size(plugin->_plugin, &w, &h);
```

The `uint32_t w` shadows the `NSWindow *w` declared a few lines above. This does not cause a crash in the current code path because the outer `w` is not used after the shadow is introduced, but it is a maintenance hazard and suggests the resize logic was written hastily.

### No Bounds Checking on Plugin-Reported Size

`get_size()` is called without validating that the returned width and height are sensible (non-zero, not absurdly large). A misbehaving plugin could return `(0, 0)` or `(65535, 65535)`, with no guard.

---

## Error Handling

Multiple critical operations have no user-visible error path:

| Site | Failure Mode | Current Handling |
|------|--------------|-----------------|
| Plugin entry not found | No plugin loaded | `LOGINFO` only, window shown empty |
| `plugin->create()` fails | Null plugin pointer used | `LOGINFO` only |
| `ui->create()` fails | GUI in undefined state | Not checked at all |
| `ui->get_size()` returns (0,0) | Zero-size window | Not checked |
| `mainStartAudio()` fails | No audio | Not checked; user hears nothing |
| RtAudio stream open fails | Logged, stream closed | No dialog; audio silently absent |

---

## MIDI Notes

- **`startMIDIThread()`** is a misnomer — it runs on the main thread. RtMidi callbacks fire on an internal RtMidi thread, not a thread this code controls.
- **MIDI output** is explicitly noted as unimplemented: `LOGINFO("[WARNING] Midi Output not supported yet")`
- **SysEx** is limited to 3 bytes across all platforms (shared code).
- **No port selection**: all available MIDI input ports are opened unconditionally.

---

## Info.plist Issues

| Key | Status | Impact |
|-----|--------|--------|
| `NSMicrophoneUsageDescription` | Present | Correct — needed for audio input permission |
| `NSHighResolutionCapable` | **Missing** | Retina rendering broken (see Bug 2) |
| `NSSupportsAutomaticGraphicsSwitching` | Missing | App may hold discrete GPU when not needed |

---

## Incomplete / TODO Items in Shared Code

| Location | Note |
|----------|------|
| `standalone_host.cpp:160` | `// TODO cleaner` around main bus index logic |
| `standalone_host.cpp:179` | Same TODO, duplicated |
| `standalone_host_audio.cpp:244` | `"[WARNING] Hardcoding frame size to 256 samples for now"` — buffer size is not configurable |
| `standalone_host_midi.cpp:103` | `"[WARNING] Midi Output not supported yet"` |
| `AppDelegate.h:7` | Commented-out `AudioSettingsWindowDelegate` — unclear if still planned |

---

## Summary Table

| # | Bug | Severity | Impact |
|---|-----|----------|--------|
| 1 | `set_scale()` call with Cocoa | ~~Critical~~ N/A | Retracted — Cocoa spec says don't call `set_scale()`; plugin ignores it per spec |
| 2 | `NSHighResolutionCapable` missing from plist | Critical | macOS forces low-res mode; plugin has no chance to render at Retina resolution |
| 3 | Spinlock in audio callback | High | Audio dropouts during any GUI interaction |
| 4 | No error checking after `ui->create()` | High | Silent invisible window on any GUI failure — *(Done)* |
| 5 | MIDI queue missing memory fence | High | Data races / corruption on Apple Silicon |
| 6 | `AudioSettingsWindow` leaked each open | Medium | Growing memory use over session |
| 7 | Callback timer on `NSDefaultRunLoopMode` only | Medium | `on_main_thread` callbacks drop during resize/menus — *(Done)* |
| 8 | Zero-size window if `get_size()` returns (0,0) | Medium | Window exists but shows nothing — *(Done)* |
| 9 | No user-visible audio init error | Medium | Silent audio failure |
| 10 | Variable shadowing in resize handler | Low | Maintenance hazard, not a current crash |
| 11 | 0.001 s deferred setup is fragile | Low | Window briefly blank; bad failure UX |
| 12 | `NSSupportsAutomaticGraphicsSwitching` missing | Low | May keep discrete GPU alive |

---

## Recommended Fixes (Priority Order)

1. **Add `NSHighResolutionCapable = true`** to `Info.plist.in` immediately. *(Done)*
2. ~~**Fix `set_scale()`**~~ — retracted; `set_scale()` must not be called for `CLAP_WINDOW_API_COCOA` per the CLAP spec. The existing call at `1.0` is harmless but should not be changed to read `backingScaleFactor`.
3. **Remove the spinlock from the audio callback** — use atomic flags or a lock-free ring buffer for plugin ↔ audio-thread state.
4. **Add error checking** after `ui->create()`, `ui->set_parent()`, and `mainStartAudio()`, with user-facing `NSAlert` dialogs. *(Done)*
5. **Add a memory fence** in the MIDI queue between the element write and the index update.
6. **Switch the callback timer** to `NSRunLoopCommonModes`. *(Done)*
7. **Audit ARC coverage** — ensure `NSAlert` and `AudioSettingsWindow` instances are properly released.
8. **Validate sizes** returned by `ui->get_size()` before calling `setContentSize:`. *(Done)*
9. **Rename `startMIDIThread()`** and document which thread MIDI callbacks fire on.