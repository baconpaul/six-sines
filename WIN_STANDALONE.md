# Windows Standalone: Code Review & Bug Analysis

**Reviewed:** `libs/clap-libs/clap-wrapper/src/detail/standalone/windows/`  
**Files:** `windows_standalone.cpp`, `windows_standalone.h`, `windows_standalone.manifest`, `wrapasstandalone_windows.cpp`  
**Date:** 2026-04-03  
**Context:** Two user reports of the standalone not displaying its UI on Windows.

---

## Reported Symptoms

> "I start the .exe, it opens (with no visible UI) and lurks in the taskbar, even reacting to my MIDI keyboard. If I try and 'maximize it back' by clicking, after a couple of clicks, it crashes (I never see the UI anymore)."

> "I'm randomly running into problems with the windows standalone version — it's not opening the window again, unless I switch to maximised Window and back to normal window again within the link properties."

The second workaround — setting the shortcut's "Run:" property to "Maximized" and then back to "Normal" — is a significant diagnostic clue (see Hypothesis B below).

---

## Architecture Summary

The implementation uses a classical Win32 approach:

- **`Window`** — base class wrapping an `HWND` with a lambda-based message dispatch table
- **`Plugin : Window`** — extends `Window` with CLAP plugin GUI embedding, audio/MIDI setup, and the system-menu settings UI
- **`Settings : Window`** — a separate child window for audio/MIDI configuration
- The message loop (`run()`) is a plain `GetMessage` / `TranslateMessage` / `DispatchMessage` loop
- CLAP GUI embedding: `plugin.gui->set_parent()` is called with the main `HWND`; JUCE (or whatever the plugin uses internally) creates child windows inside it

---

## Confirmed Bugs

### Bug 1 — `nCmdShow` Completely Discarded (HIGH — explains Symptom 2 workaround)

**File:** `wrapasstandalone_windows.cpp:3`

```cpp
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int /* nCmdShow */)
```

`nCmdShow` is received from Windows but deliberately commented out and never used. Later, `activate()` unconditionally calls:

```cpp
// windows_standalone.cpp:341
::ShowWindow(hwnd.get(), SW_NORMAL);
```

On Windows, the shell sets `STARTUPINFO.wShowWindow` (e.g., `SW_SHOWMAXIMIZED`) when a shortcut has a "Run:" override, and the OS uses this to override the parameter on **the very first `ShowWindow` call** for that process — but only if `nCmdShow` was originally respected and passed through. Since the code always passes `SW_NORMAL`, it bypasses the OS's ability to start the window in a different state.

More importantly: the correct first-show parameter (from `nCmdShow`) should be passed so that Windows can apply any saved placement the shell has established for the application. Ignoring it can cause the window to appear at unexpected sizes or positions.

The fix for Symptom 2's workaround is explained by this: setting the shortcut to "Maximized" causes `SW_SHOWMAXIMIZED` to be passed via `STARTUPINFO`, which the OS **does** honour on the first `ShowWindow` call regardless of the parameter (this is a Windows-documented behaviour). The maximized state overrides any bad saved position, and "toggling back to Normal" updates the stored shortcut state — after which the window opens correctly because maximizing has forced a new, valid saved position.

---

### Bug 2 — `adjustSize(0, 0)` if `get_size()` Returns Zero (HIGH — explains Symptom 1)

**File:** `windows_standalone.cpp:1109–1113`

```cpp
Position pluginSize;
plugin.gui->get_size(plugin.plugin, &pluginSize.width, &pluginSize.height);
log("{}, {}", pluginSize.width, pluginSize.height);
adjustSize(pluginSize.width, pluginSize.height);
```

`get_size()` is called immediately after `create()` and `set_scale()`, before `set_parent()`. If the plugin has not yet computed its layout at this point (e.g., it requires a visible parent or defers layout until `set_parent()` is processed), it may return `(0, 0)`. `adjustSize(0, 0)` then sets the window to zero size:

```cpp
// windows_standalone.cpp:359–373
void Window::adjustSize(uint32_t width, uint32_t height) {
    ::RECT rect{ 0, 0, width, height };  // → {0, 0, 0, 0}
    // AdjustWindowRectExForDpi adds border/chrome, but with 0 inner size
    // the result is just the chrome, possibly sub-pixel or clipped to zero
    ::SetWindowPos(hwnd.get(), nullptr, 0, 0,
                   (rect.right - rect.left), (rect.bottom - rect.top),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
}
```

The window exists and has a taskbar entry, but is effectively invisible. Audio and MIDI run on their own threads and work fine. Forcing the window to maximized forces a new size to be negotiated via `onRequestResize`, which is why that workaround helps.

---

### Bug 3 — `set_parent()` Called on a Hidden Parent Window (MEDIUM — may cause rendering issues)

**File:** `windows_standalone.cpp:1115–1116`

```cpp
clap_window clapWindow{CLAP_WINDOW_API_WIN32, static_cast<void *>(hwnd.get())};
plugin.gui->set_parent(plugin.plugin, &clapWindow);
```

`set_parent()` is called before `activate()` (line 1162). The parent `HWND` has not yet been shown (`WS_VISIBLE` is not in the initial `CreateWindowExW` style). JUCE (or any Win32-based plugin) creates its child windows into a hidden parent. When the parent is later made visible, child window painting can be deferred or in a stale state depending on how the plugin handles this. Some runtimes may need `ShowWindow` + `UpdateWindow` on the child explicitly after reparenting.

---

### Bug 4 — `hwnd.reset()` in `WM_CLOSE` Causes Dangling References (HIGH — explains crash)

**File:** `windows_standalone.cpp:328–333`

```cpp
if (msg == WM_CLOSE) {
    self->hwnd.reset();  // → DestroyWindow() called immediately
    return 0;            // → suppresses DefWindowProc
}
```

`hwnd` is a `wil::unique_hwnd`. Calling `.reset()` invokes `DestroyWindow()` synchronously from within the window procedure. This is unusual and dangerous:

1. `DestroyWindow()` causes the OS to send `WM_DESTROY` and `WM_NCDESTROY` — while still inside the `WM_CLOSE` handler.
2. The `WM_DESTROY` handler (line 994) calls `plugin.gui->destroy()`, `mainFinish()`, and `quit()`. The plugin GUI is being destroyed while the parent HWND is potentially mid-message.
3. After `hwnd.reset()`, `hwnd.get()` returns `nullptr`. Any subsequent message handler, timer callback, or queued paint that calls `hwnd.get()` will crash via a null handle passed to Win32 APIs.
4. The user's reported crash ("after a couple of clicks") is consistent with this: clicking the taskbar button of a window that is in this broken half-destroyed state generates messages (e.g., `WM_ACTIVATE`, `WM_SETFOCUS`, repaint requests) that arrive after the HWND is already invalid.

The standard Win32 pattern is to let `DefWindowProcW` handle `WM_CLOSE`, which calls `DestroyWindow()` safely after the message handler returns, and only null out the stored handle in `WM_NCDESTROY`.

---

### Bug 5 — `set_size(0, 0)` Called When Window Is Minimized (MEDIUM — crash risk)

**File:** `windows_standalone.cpp:668–672`

```cpp
if (plugin.gui->can_resize(plugin.plugin)) {
    plugin.gui->adjust_size(plugin.plugin, &client.width, &client.height);
    plugin.gui->set_size(plugin.plugin, client.width, client.height);
}
```

This runs on **every** `WM_WINDOWPOSCHANGED`, including when the window is minimized. When minimized, `GetClientRect` returns `{0, 0, 0, 0}`, so `client.width = 0` and `client.height = 0`. Calling `set_size(0, 0)` on a CLAP plugin GUI is likely undefined behaviour and may crash.

A guard like `if (placement.showCmd != SW_SHOWMINIMIZED)` (the pattern already used in `onRequestResize` at line 1132) should be applied here too.

---

### Bug 6 — DPI Width Calculation Is Backwards (LOW — currently dormant)

**File:** `windows_standalone.cpp:316`

```cpp
self->suggested.width = rect->left - rect->right;  // ← WRONG: produces negative value
self->suggested.height = rect->bottom - rect->top;  // ← correct
```

Should be `rect->right - rect->left`. The `suggested` field does not appear to be consumed anywhere in the current code, so this is latent. If it is ever used for `WM_DPICHANGED` repositioning, it will produce a window of negative/enormous width.

---

### Bug 7 — No Error Checking on Critical GUI Calls (MEDIUM — silent failures)

**File:** `windows_standalone.cpp:1088–1116`

```cpp
plugin.gui->create(plugin.plugin, CLAP_WINDOW_API_WIN32, false);  // return value ignored
plugin.gui->set_scale(plugin.plugin, scale);                       // return value ignored
plugin.gui->get_size(plugin.plugin, &pluginSize.width, &pluginSize.height); // ignored
plugin.gui->set_parent(plugin.plugin, &clapWindow);               // return value ignored
```

All of these return `bool`. If any fails, the plugin GUI ends up in an undefined partial state. There is no log output or early exit, so the user sees a black/empty window with no diagnostic information.

---

## Hypotheses for Reported Symptoms

### Symptom 1: "No visible UI, lurks in taskbar, MIDI works"

**Primary hypothesis: `adjustSize(0, 0)` due to premature `get_size()` call (Bug 2)**

If the plugin's `get_size()` returns `(0, 0)` before `set_parent()` has been called, the window is set to zero client area. The frame exists (hence the taskbar entry), MIDI and audio threads are unaffected, but there is nothing to see. Forcing the window to maximize overrides the zero size, which triggers `onRequestResize` with valid dimensions, causing the plugin to properly initialize its layout.

**Secondary hypothesis: Bad saved position from a disconnected monitor**

The code saves the full window position on every `WM_WINDOWPOSCHANGED` and restores it unconditionally at startup (lines 1098–1101) if `width != 0 || height != 0`. If a previous session was closed with the window on a monitor that is no longer connected, the saved position could place the window entirely off-screen. The window exists and processes events but is not visible on any display. Maximizing brings it to the current screen.

---

### Symptom 2: "Doesn't open unless shortcut is set to Maximized and back"

**Hypothesis: Stale off-screen saved position, cleared by the maximize-and-restore cycle**

When the shortcut is set to "Maximized," `SW_SHOWMAXIMIZED` is injected via `STARTUPINFO`. Even though `activate()` calls `ShowWindow(hwnd, SW_NORMAL)`, the OS overrides the first `ShowWindow` call to use `SW_SHOWMAXIMIZED` (standard Windows behaviour for the first show). This forces the window onto the visible desktop at full-screen size. After returning the shortcut to "Normal," the previous bad saved position has been overwritten with valid screen coordinates from the maximized session. The window now opens correctly.

---

### Symptom: Crash after clicking

**Primary hypothesis: Null `hwnd` dereference from Bug 4 (`hwnd.reset()` in `WM_CLOSE`)**

`WM_CLOSE` destroys the window immediately via `hwnd.reset()`. Any messages queued before or during destruction (activation messages, paint requests, taskbar interaction) are then dispatched with an `hwnd.get()` that returns `nullptr`. Passing `nullptr` to APIs like `GetSystemMenu`, `SetWindowPos`, `SendMessage`, or `SetWindowLongPtrW` causes an access violation. This is the most probable crash cause given the description of "after a couple of clicks."

---

## Summary Table

| # | Bug | Severity | Probable Impact |
|---|-----|----------|-----------------|
| 1 | `nCmdShow` ignored; `SW_NORMAL` hardcoded | High | Window ignores shell placement; Symptom 2 workaround |
| 2 | `adjustSize(0,0)` if `get_size()` returns zero before `set_parent()` | High | Window is zero-size / invisible (Symptom 1) |
| 3 | `set_parent()` called on hidden parent | Medium | Potential painting/embedding issues |
| 4 | `hwnd.reset()` in `WM_CLOSE` → dangling HWND | High | Crash after window interaction |
| 5 | `set_size(0,0)` called when minimized | Medium | Likely crash on minimize |
| 6 | DPI width calc backwards (`left - right`) | Low | Currently dormant; breaks `WM_DPICHANGED` resize |
| 7 | No error checking on GUI create/set_parent | Medium | Silent invisible-window failures |

---

## Recommended Fixes (Priority Order)

1. **Fix `WM_CLOSE`:** Remove `hwnd.reset()` from there; set it to `nullptr` only in `WM_NCDESTROY` and let `DefWindowProcW` call `DestroyWindow()` naturally.

2. **Fix `activate()`:** Pass `nCmdShow` through from `wWinMain` instead of hardcoding `SW_NORMAL`. Store it and use it in `activate()`.

3. **Fix `get_size()` call ordering:** Call `get_size()` *after* `set_parent()`, not before. Or validate that width/height are non-zero before calling `adjustSize()`.

4. **Guard `set_size()` on minimized:** Add `if (placement.showCmd != SW_SHOWMINIMIZED)` before the `adjust_size`/`set_size` calls in the `WM_WINDOWPOSCHANGED` handler.

5. **Fix DPI width calculation:** Change `rect->left - rect->right` to `rect->right - rect->left` at line 316.

6. **Add error checking:** Check return values of `gui->create()`, `gui->set_parent()`, and `gui->get_size()`, and log/abort gracefully on failure.

7. **Add off-screen position guard:** Before calling `setPosition(position)` to restore a saved position, verify that the position intersects at least one current monitor. If not, center the window instead.
