# Linux Standalone: Code Review & Bug Analysis

**Reviewed:** `libs/clap-libs/clap-wrapper/src/detail/standalone/linux/`  
**Files:** `x11_gui.h`, `x11_gui.cpp`, plus shared `standalone_host_audio.cpp`, `standalone_host_midi.cpp`, `entry.cpp`  
**Last updated:** 2026-04-04  
**Context:** Ongoing work on the Linux standalone host for CLAP plugins. GTK path has been removed; X11 is the only GUI path.

---

## What Has Been Fixed

| # | Fix |
|---|-----|
| 1 | `XOpenDisplay()` return value checked; exits with a message on failure |
| 2 | `XInitThreads()` called before any Xlib call |
| 3 | `MapNotify` handler guards `plugin` and `_ext._gui` before dereferencing |
| 4 | GTK path removed entirely; X11 is unconditional on Linux |
| 5 | MIDI port open failure now caught per-port; loop continues on error |
| 6 | `adjust_size()` adjusted values are used for window creation |
| 7 | `epoll_fd` set to -1 after close in `shutdown()` so the guard in `unregister_timer` fires correctly |
| 8 | `gui->destroy()` called in `X11Gui::shutdown()` before epoll/display teardown, fixing timer unregistration crash on exit |

---

## X11 GUI: Remaining Issues

### Window Position Hard-Coded

**File:** `x11_gui.cpp` — `XCreateSimpleWindow` call in `setPlugin()`

Always opens at `(10, 10)`. No centering, no saved position restore.

---

### Window Size Locked via Size Hints

**File:** `x11_gui.cpp` — `resetSizeTo()`

```cpp
hints->min_width = hints->max_width = w;
hints->min_height = hints->max_height = h;
```

Min and max hints are set to the same value on every resize. This prevents the window manager from allowing user-initiated resizing even for plugins that support `can_resize()`. To allow resizing, only `PSize` / `PAspect` hints should be set; min/max should be omitted or set to sensible bounds.

---

### `set_parent()` Only Called on `MapNotify`

**File:** `x11_gui.cpp` — `runloop()`

The plugin's `set_parent()` and `show()` are deferred until the window receives a `MapNotify` event. This is fine for most plugins but means the plugin has no parent window during the window-creation phase. Plugins that use `set_parent` to initialize their renderer may produce a blank frame on first display.

---

### No Wayland Support

Neither XWayland-fallback nor native Wayland. On a Wayland-only session `XOpenDisplay` will fail (caught and exited cleanly now), but the user gets no hint about setting `DISPLAY` or `GDK_BACKEND=x11`. A more helpful exit message could suggest `DISPLAY=:0` or the XWayland socket.

---

### Epoll Error Handling: Failures Logged but Not Propagated

`epoll_ctl` failures in `register_fd`, `register_timer`, `setPlugin` are logged and return false, but callers generally ignore the return value. A failed epoll registration means the corresponding timer or fd will never fire, silently breaking plugin functionality.

---

### Magic Numbers

`nextTimerId` starts at `2112`, epoll max events is `256`, epoll timeout is `50ms`. None are named constants.

---

### No RAII for X11 Resources

`XCloseDisplay`, `XFree`, timerfd closes, and epoll fd close are all manual. An exception or early return will leak resources. The timer fds in `timerIdToFd` are not closed in `shutdown()` if any are still registered.

---

## Audio Backend: ALSA / JACK / PipeWire

### The Core Problem

**File:** `standalone_host_audio.cpp:60`

```cpp
setAudioApi(RtAudio::Api::UNSPECIFIED);
```

`RtAudio::Api::UNSPECIFIED` lets RtAudio auto-select from whichever backends were compiled in. The selection priority in RtAudio is approximately JACK → ALSA → PulseAudio, but in practice **ALSA is almost always chosen** even when a JACK daemon is running, because RtAudio's auto-select logic does not reliably detect a running JACK server. This is the confirmed cause of the user-reported "only binds to ALSA, doesn't show JACK" symptom.

The compiled-in backends are controlled by CMake options in `wrap_standalone.cmake` (inherited from RtAudio's own CMakeLists):

```cmake
option(RTAUDIO_API_ALSA   "Build ALSA API"                ${LINUX})
option(RTAUDIO_API_PULSE  "Build PulseAudio API"           ${pulse_FOUND})
option(RTAUDIO_API_JACK   "Build JACK audio server API"    ${HAVE_JACK})
```

JACK support is only compiled in when `HAVE_JACK` is true at configure time (i.e., JACK headers are present on the build machine). On a build machine without JACK headers the JACK backend is silently absent from the binary.

### PipeWire

PipeWire is **not explicitly configured** — there is no `RTAUDIO_API_PIPEWIRE` option. PipeWire provides:

- An **ALSA compatibility layer** (`pipewire-alsa`) — when active, ALSA device enumeration goes through PipeWire transparently. This usually works without any code changes.
- A **JACK compatibility layer** (`pipewire-jack`) — replaces `libjack` with a PipeWire shim. If the build machine has `pipewire-jack` installed in place of real JACK, the JACK backend will talk to PipeWire's JACK server. This also generally works.
- A **native PipeWire RtAudio backend** exists in newer RtAudio versions (`RtAudio::Api::LINUX_PIPEWIRE`). This is not enabled in the current build and would require adding `RTAUDIO_API_PIPEWIRE` to the CMake options.

In short: on a system running PipeWire with the ALSA and JACK compatibility layers active, the existing code works but the user gets ALSA latency characteristics rather than PipeWire's low-latency graph scheduling. For most users this is acceptable.

### How to Address the JACK/API Selection Problem

The `StandaloneHost` already has all the infrastructure needed:

- `getCompiledApi()` — returns `std::vector<RtAudio::Api>` of built-in backends
- `setAudioApi(RtAudio::Api)` — switches the active backend
- `audioApi`, `audioApiName`, `audioApiDisplayName` — current state

What is missing is a way to surface this to the user. The two practical options are:

**Option A — Command-line flag (minimal, recommended first step)**

Parse a `--audio-api <name>` argument in `wrapasstandalone.cpp` before calling `mainStartAudio()`. Map the name to an `RtAudio::Api` value and call `getStandaloneHost()->setAudioApi(...)`. Example mapping:

| Flag value | `RtAudio::Api` |
|------------|---------------|
| `alsa`     | `LINUX_ALSA`  |
| `jack`     | `UNIX_JACK`   |
| `pulse`    | `LINUX_PULSE` |
| `pipewire` | `LINUX_PIPEWIRE` (if compiled) |

Add `--list-audio-apis` to print the compiled-in options and exit. Use `getCompiledApi()` for the list.

**Option B — Prefer JACK automatically when the server is reachable**

Before defaulting to `UNSPECIFIED`, check whether JACK is compiled in and whether a JACK server is running:

```cpp
// Pseudo-code
auto apis = getCompiledApi();
if (std::find(apis.begin(), apis.end(), RtAudio::Api::UNIX_JACK) != apis.end())
{
    // Try JACK first; fall back to UNSPECIFIED if stream open fails
    setAudioApi(RtAudio::Api::UNIX_JACK);
}
```

RtAudio will return an error when opening the stream if no JACK server is running, so the fallback path is safe.

### Device Enumeration and Selection

- **No device selection UI or flag.** `getDefaultInputDevice()` / `getDefaultOutputDevice()` are used unconditionally. On systems where the default device is a USB headset or HDMI output this produces confusing behaviour. A `--audio-device <name>` flag (matched against `getDeviceNames()`) would address this.
- **Default device IDs not validated.** If the default device ID returned is 0 and no device 0 exists for the selected API, `openStream` will fail with a potentially cryptic error.
- **No input device selection.** `startAudioThread()` always passes the default input device. Plugins with audio inputs have no way to select their source.

### Audio Backend: Remaining Code Issues

---

### Channel Count Hard-Coded to 2 (Stereo Only)

**File:** `standalone_host_audio.cpp:96, 101`

```cpp
startAudioThreadOn(in, 2, ..., out, 2, ...);
```

Input and output are always opened as stereo. Plugins with mono or multi-channel buses will get the wrong channel count. `clapProcess` also asserts `inputChannelByBus[inp] == 2` and `outputChannelByBus[oup] == 2`.

---

### Buffer Size Defaults to 256, Not Device-Queried

**File:** `standalone_host_audio.cpp:246-248`

```cpp
if (currentBufferSize == 0)
{
    currentBufferSize = 256;
}
```

`currentBufferSize` can be set externally before `startAudioThread()` is called, but there is no UI or command-line path to do so. The default of 256 may be wrong for the selected device or API (JACK has its own period size; ALSA may not support 256 frames on all hardware).

---

### Xrun Detection Callback Is Empty

**File:** `standalone_host_audio.cpp:24-26`

```cpp
if (status)
{
}
```

RtAudio passes a non-zero `status` on xrun (underflow/overflow). The callback body is empty — xruns are silently dropped. `rtaErrorCallback` does catch `RTAUDIO_OUTPUT_UNDERFLOW` and `RTAUDIO_INPUT_OVERFLOW` separately, but only logs the first occurrence.

---

### `stopAudioThread()` Busy-Waits Up to 10 Seconds

**File:** `standalone_host_audio.cpp:306-310`

```cpp
for (auto i = 0; i < 10000 && !finishedRunning; ++i)
{
    std::this_thread::sleep_for(1ms);
}
```

Waits for the audio callback to set `finishedRunning`. If the stream stalls or the callback never fires again after `running = false`, this silently times out after 10 seconds and then calls `stopStream()` anyway. No log or error is emitted on timeout.

---

### `getSampleRates()` Has a Dead-Code Bug

**File:** `standalone_host_audio.cpp:150-157`

```cpp
auto samplesRates{rtaDac->getDeviceInfo(audioInputDeviceID).sampleRates};  // unused

for (auto &sampleRate : rtaDac->getDeviceInfo(audioInputDeviceID).sampleRates)  // queried again
```

The device info is fetched twice; the first result (`samplesRates`) is never read.

---

### `getBufferSizes()` Returns a Hard-Coded List

**File:** `standalone_host_audio.cpp:164-166`

```cpp
std::vector<uint32_t> res{16, 32, 48, 64, 96, 128, 144, 160, 192, 224, 256, 480, 512, 1024, 2048, 4096};
```

Not queried from the device. RtAudio doesn't expose valid buffer sizes for a device, so this is largely unavoidable — but the list includes non-power-of-two sizes (48, 96, 144, 160, 192, 224, 480) that many ALSA and JACK drivers will reject.

---

## MIDI: Remaining Issues

### Fatal Exit on MIDI Init Failure

**File:** `standalone_host_midi.cpp:25-29`

```cpp
catch (RtMidiError &error)
{
    error.printMessage();
    exit(EXIT_FAILURE);
}
```

If creating the initial `RtMidiIn` (used only to count available ports) fails, the process exits hard. This kills the standalone even when MIDI is not needed. Should log a warning and continue with `numMidiPorts = 0`.

---

### `RtMidiIn` Created Twice Per Session

**File:** `standalone_host_midi.cpp:22-23, 36`

A temporary `RtMidiIn` is created just to call `getPortCount()`, then discarded. A separate instance is created for each port in the loop. The first instance is wasted. This can cause issues on some ALSA configurations where creating an `RtMidiIn` registers a client with the sequencer.

---

### All Ports Opened Unconditionally

There is no mechanism to select which MIDI inputs to use. Every available port is opened. On systems with many virtual/loopback ALSA ports this can produce unwanted MIDI loops.

---

### 3-Byte Message Cap; No SysEx

**File:** `standalone_host_midi.cpp:53`

```cpp
if (nBytes <= 3)
```

Messages longer than 3 bytes are silently discarded. SysEx (which can be arbitrarily long) is never forwarded to the plugin.

---

### No Timestamp Preservation

MIDI timestamps from the RtMidi callback (`deltatime`) are ignored. All events are queued with `header.time = 0`, meaning they are all presented to the plugin at the start of each audio block regardless of when they arrived.

---

## Summary Table

| # | Item | Severity | Status |
|---|------|----------|--------|
| 1 | `XOpenDisplay()` not checked | Critical | Fixed |
| 2 | No `XInitThreads()` | Critical | Fixed |
| 3 | `MapNotify` null dereference | Critical | Fixed |
| 4 | GTK `std::terminate()` on Wayland | High | Fixed (GTK removed) |
| 5 | MIDI port open aborts loop | High | Fixed |
| 6 | `adjust_size()` result discarded | Medium | Fixed |
| 7 | `epoll_fd` not cleared to -1 after close | Medium | Fixed |
| 8 | Timer unregistration crash on exit | Critical | Fixed |
| — | `RtAudio::UNSPECIFIED` misses JACK; no API/device selection | High | Open |
| — | Channel count hard-coded to stereo | High | Open |
| — | Fatal exit if MIDI init fails | Medium | Open |
| — | Buffer size defaults to 256, not configurable | Medium | Open |
| — | Window size locked via min=max hints | Medium | Open |
| — | `RtMidiIn` created twice | Low | Open |
| — | All MIDI ports opened unconditionally | Low | Open |
| — | No SysEx; 3-byte message cap | Low | Open |
| — | No MIDI timestamp preservation | Low | Open |
| — | Xrun callback body empty | Low | Open |
| — | `stopAudioThread` 10s busy-wait, no timeout log | Low | Open |
| — | `getSampleRates()` double-fetches device info | Low | Open |
| — | Window position hard-coded to (10, 10) | Low | Open |
| — | Leaks timer fds on shutdown if not unregistered | Low | Open |
| — | No Wayland support or helpful error hint | Low | Open |
