# Color Editor Feature — Prompts and Plan

**Date:** 2026-04-15

---

## User Prompts (verbatim)

### Prompt 1

OK I'm looking to add a feature to sst-jucegui to allow a color editor. I want this to be in three parts.

FIRST there's a color editor in sst/jucegui/screens called ColorEditor.  It takes a data structure which looks like a list of color tag strings and colors, and also takes a function of string,color. It uses that to present a panel which contains a scrollable list of those colors with a button - colored in the colour - and a text next to them. When you press the button it pops up a colour picker. The text field has the 32 bit hex code (so like #ff00ff00 for green) or 24 bit hex code (#ff0000 for red) depending on whether the 'includeAlpha' flag is set. This should be in a NamedPanel. When a color is edited it calls the callback. Fine.

SECOND is a simple idiom to show that panel in a standalone window. That can be a static method in the same header probably with a return type like std::unique_ptr<juce::DocumentWindow> and with the constructor args for the picker screen. And also a version of the scren component which implements Modalbase so we can use it as an in app modal overlay.

THIRD is an adapter atain in the same header which takes a style sheet and (ine one mode) a list of style keys and (in another mode) a list of color maps to style keys and updates the style sheet object.

This is quite a big prompt so lets start with you laying out a plan. And also I want to keep the prompt history in @doc/12_prompts/ with the explicit prompt and either plan or summary of action just like i did. Call the file 12_prompts/color_picker.md.

### Prompt 2

I think thats right. On 4 the component can implement hover to draw a white line around the color to make it clear it is active. And yes on 1 2 and 3. For the picker though we want to use a viewport / scroll window if the color set is larger than 15 or so. So include that viewport code also.

Theres one more part of the plan i want. That's to have six sines be able to open one of these from the main menu under UI and get all the editors for value bearing, base label, outlined, etc... which are the base styles we use for the knob, slider, dislplya, etc.... and do that directly on the style sheet. We will then want to soon enough serailize those using (probably) the style id, which we can do by introducint an sst/jucegui/style/Seralizer.h which goes to and from some text file for a style shet. Lets add that to the plan as steps.

### Prompt 3

Great. Actually in jucegui drop the toFile/fromFile and just have toString/fromString method. We cna handle the file io externally since we'll want to use std::filesystem and the likes in the client app. Do the file save in six sines. Modify the plan for me again with that in mind, restate the plan, and then lets look at it

---

## Plan

### Overview

A single new header `include/sst/jucegui/screens/ColorEditor.h` provides:

1. **`ColorEditor`** — a `NamedPanel` subclass showing a scrollable list of named colors, each with a color swatch button and an editable hex text field.
2. **`ColorEditorModal`** — a `ModalBase` subclass that wraps a `ColorEditor` for use as an in-app overlay.
3. **`ColorEditor::asStandaloneWindow()`** — static factory for a `juce::DocumentWindow` hosting a `ColorEditor`.
4. **`ColorEditor::forStyleKeys()`** and **`ColorEditor::forColorMap()`** — static adapters that wire a `StyleSheet` to a `ColorEditor`.

Everything lives in one header (matching the pattern of `AlertOrPrompt.h` and `ModalBase.h`). No new `.cpp` file is needed.

---

### Part 1: `ColorEditor`

#### Data types

```cpp
struct ColorEntry {
    std::string tag;   // human-readable name shown as the row label
    juce::Colour color;
};
using ColorChangedFn = std::function<void(const std::string& tag, juce::Colour newColor)>;
```

#### Class declaration

```cpp
struct ColorEditor : components::NamedPanel,
                     private juce::ChangeListener
{
    ColorEditor(std::vector<ColorEntry> entries,
                ColorChangedFn callback,
                bool includeAlpha = false);

    void resized() override;

    // ... (private row component, listView, picker management)
};
```

`ColorEditor` inherits `NamedPanel` so it gets the styled header bar, outline, and `getContentArea()`. The title string passed to `NamedPanel("Color Editor")` is fixed but callers can call `setName()` after construction.

#### Internal `RowComponent`

`ListView` is used with `BRUTE_FORCE_NO_REUSE` strategy. Each row is a `RowComponent` (private inner struct):

```
[ color swatch button (32px wide) ][ hex text field (fills remainder) ]
```

- **Swatch button**: plain `juce::Component` that `paint()`s itself solid with `entry.color` and a 1px outline. `mouseDown` calls back into `ColorEditor::openPickerFor(idx, screenBounds)`.
- **Hex text field**: `juce::TextEditor`, single-line. Initialized with `toHexString(entry.color)`. `onReturnKey` and `focusLost` parse the typed string with `fromHexString()`, update `entries[idx].color`, and fire `onColorChanged`.

Row height: 28px. The `ListView` `getRowHeight` lambda returns 28.

#### Hex string format

```cpp
std::string toHexString(juce::Colour c) const;
// includeAlpha=true  → "#AARRGGBB" (8 hex digits, uppercase)
// includeAlpha=false → "#RRGGBB"   (6 hex digits, uppercase)

juce::Colour fromHexString(const std::string& s) const;
// Strips leading '#', prepends "FF" if !includeAlpha,
// then calls juce::Colour::fromString(). Returns juce::Colours::transparentBlack on parse failure.
```

#### Color picker

`ColorEditor` implements `juce::ChangeListener`. `openPickerFor(idx, screenBounds)`:

1. Creates a `juce::ColourSelector` with flags:
   ```
   showColourAtTop | showSliders | showColourspace
   ```
   plus `showAlphaChannel` if `includeAlpha`.
2. Sets the current colour to `entries[idx].color`.
3. Adds `*this` as a `ChangeListener`.
4. Launches via `juce::CallOutBox::launchAsynchronously(...)`.
5. Stores `activePickerIdx = idx` and a raw (unowned) pointer to the selector.

`changeListenerCallback()`:
- Reads `selector->getCurrentColour()`.
- Calls `updateEntry(activePickerIdx, newColor)`.

`updateEntry(idx, color)`:
- Updates `entries[idx].color`.
- Updates the row's hex text field text via the `ListView`'s assigned component.
- Calls `onColorChanged(entries[idx].tag, color)`.
- Calls `listView->refresh()`.

---

### Part 2: Standalone Window + Modal Overlay

#### Static factory: `asStandaloneWindow`

```cpp
static std::unique_ptr<juce::DocumentWindow> asStandaloneWindow(
    const std::string& title,
    std::vector<ColorEntry> entries,
    ColorChangedFn callback,
    bool includeAlpha = false,
    style::StyleSheet::ptr_t stylesheet = nullptr);
```

Implementation sketch:
- Creates a `ColorEditor` (heap-allocated, owned by the window).
- If `stylesheet` is non-null, calls `editor->setStyle(stylesheet)`.
- Creates a `juce::DocumentWindow` with title, resizable, with close button.
- Sets `window->setContentOwned(editor.release(), true)`.
- Sets initial size to e.g. `400 × std::min(600, rowHeight * entries.size() + headerHeight + margins)`.
- `window->setVisible(true)`.

The returned `unique_ptr<DocumentWindow>` is owned by the caller; destroying it closes the window.

#### `ColorEditorModal`

```cpp
struct ColorEditorModal : ModalBase
{
    ColorEditorModal(std::vector<ColorEditor::ColorEntry> entries,
                     ColorEditor::ColorChangedFn callback,
                     bool includeAlpha = false);

    juce::Point<int> innerContentSize() override;  // returns {500, 480} or similar
    void resized() override;

private:
    std::unique_ptr<ColorEditor> editor;
};
```

`resized()` positions `editor` to fill `getContentArea()`.
`innerContentSize()` returns a fixed default of `{500, 480}`; callers can subclass to override.

`ColorEditorModal` is used with `ScreenHolder::displayModalOverlay()`, exactly as `AlertOrPrompt` is — the caller calls `setVisible(false)` from within the callback or a close button to dismiss.

---

### Part 3: Stylesheet Adapters

Both are static factories on `ColorEditor`. The `StyleKey` type captures pointers to the long-lived constexpr statics:

```cpp
struct StyleKey {
    const style::StyleSheet::Class& cls;
    const style::StyleSheet::Property& prop;
    std::string label;  // display name; defaults to "ClassName::propName"
};
```

#### Mode 1: flat key list

```cpp
static std::unique_ptr<ColorEditor> forStyleKeys(
    style::StyleSheet::ptr_t stylesheet,
    std::vector<StyleKey> keys,
    bool includeAlpha = false);
```

Builds `ColorEntry` per key: `tag = key.label`, `color = stylesheet->getColour(key.cls, key.prop)`.

`onColorChanged` callback: finds the matching key by tag, calls `stylesheet->setColour(key.cls, key.prop, newColor)`.

#### Mode 2: named color map

```cpp
struct ColorMapEntry {
    std::string name;               // display name for this color group
    std::vector<StyleKey> keys;     // all style properties sharing this color
};

static std::unique_ptr<ColorEditor> forColorMap(
    style::StyleSheet::ptr_t stylesheet,
    std::vector<ColorMapEntry> colorMap,
    bool includeAlpha = false);
```

Builds one `ColorEntry` per `ColorMapEntry`: `tag = entry.name`, `color = stylesheet->getColour(entry.keys[0].cls, entry.keys[0].prop)` (first key is representative).

`onColorChanged` callback: finds the matching group by name, calls `stylesheet->setColour(k.cls, k.prop, newColor)` for **every** key in that group.

---

### File Summary

| File | Action |
|------|--------|
| `include/sst/jucegui/screens/ColorEditor.h` | **New** — all three parts in one header |

No `.cpp` needed; no changes to existing files beyond adding `#include` where callers use it. The header includes:
- `"ModalBase.h"` (Part 2 modal)
- `<juce_gui_basics/juce_gui_basics.h>`
- `"../components/NamedPanel.h"`
- `"../components/ListView.h"`
- `"../style/StyleSheet.h"`
- `"../style/StyleAndSettingsConsumer.h"`

---

### Clarifications / decisions from follow-up

1. Text field: **editable** (type a hex to set). ✓
2. Standalone default size: **400×480**. ✓
3. `ColorEditorModal` size: **`{500, 480}`** (constructor arg not needed for now). ✓
4. Swatch hover: bare `juce::Component`, paints solid color fill + 1px outline; on hover draws a **white border inset** to signal interactivity. No theme-style involvement.
5. **Viewport/scroll**: If the entry count exceeds ~15 rows the list must scroll. `ListView` already wraps a `Viewport` internally; the `ColorEditor` simply fills the content area of the `NamedPanel` with the `ListView` and the `ListView`'s internal `Viewport` handles scrolling automatically. No extra viewport layer needed.

---

### Part 4: Six Sines — Open Color Editor from UI Menu

#### Where

`SixSinesEditor::showMainMenu()` (`src/ui/six-sines-editor.cpp`) already has a "User Interface" submenu (`uim`). A new item "Color Editor…" is added there, after the Dark/Light mode separator.

#### State

```cpp
// in SixSinesEditor (six-sines-editor.h)
std::unique_ptr<juce::DocumentWindow> colorEditorWindow;
```

Opening the window:
```cpp
uim.addItem("Color Editor...", [w = juce::Component::SafePointer(this)]() {
    if (!w) return;
    if (w->colorEditorWindow && w->colorEditorWindow->isVisible()) {
        w->colorEditorWindow->toFront(true);
        return;
    }
    auto ce = sst::jucegui::screens::ColorEditor::forColorMap(
        w->style(), sixSinesBaseColorMap(), false);
    w->colorEditorWindow = sst::jucegui::screens::ColorEditor::asStandaloneWindow(
        "Six Sines Color Editor", std::move(ce));
    w->colorEditorWindow->setVisible(true);
});
```

`sixSinesBaseColorMap()` is a free function (or static helper) in `six-sines-editor.cpp` that returns a `std::vector<ColorEditor::ColorMapEntry>` covering the base styles:

| Entry name | Style keys covered |
|---|---|
| `"Background"` | `Base::background` |
| `"Background (hover)"` | `Base::background_hover` |
| `"Outline"` | `Outlined::outline` |
| `"Bright Outline"` | `Outlined::brightoutline` |
| `"Value"` | `ValueBearing::value` |
| `"Value (hover)"` | `ValueBearing::value_hover` |
| `"Value Label"` | `ValueBearing::valuelabel` |
| `"Value Label (hover)"` | `ValueBearing::valuelabel_hover` |
| `"Value Background"` | `ValueBearing::valuebg` |
| `"Gutter"` | `ValueGutter::gutter` |
| `"Gutter (hover)"` | `ValueGutter::gutter_hover` |
| `"Handle"` | `GraphicalHandle::handle` |
| `"Handle (hover)"` | `GraphicalHandle::handle_hover` |
| `"Handle Outline"` | `GraphicalHandle::handle_outline` |
| `"Modulation Handle"` | `GraphicalHandle::modulation_handle` |
| `"Label Color"` | `BaseLabel::labelcolor` |
| `"Label Color (hover)"` | `BaseLabel::labelcolor_hover` |
| `"Push Button Fill"` | `PushButton::fill` |
| `"Push Button Fill (hover)"` | `PushButton::fill_hover` |
| `"Push Button Fill (pressed)"` | `PushButton::fill_pressed` |

When a color is changed the callback fires `style()->setColour(cls, prop, newColor)` then calls `w->repaint()` so all widgets update in real-time.

The window is closed by the user (DocumentWindow close button). `SixSinesEditor` destructor naturally destroys the window via the `unique_ptr`.

Note: `forColorMap` takes a `StyleSheet::ptr_t` so changes go directly into the live sheet. This is intentional — the color editor is a live preview tool.

---

### Part 5: Stylesheet Serializer (`sst/jucegui/style/Serializer.h`)

#### Motivation

After customizing colors with the color editor, users want to save/load their theme. The serializer writes all colour properties of a `StyleSheet` to a text file and reads them back.

#### File

New header: `include/sst/jucegui/style/Serializer.h` (header-only).

#### Text format

Simple key=value, one per line, comments with `#`:

```
# sst-jucegui color theme
# format: className::propertyName=AARRGGBB
base::background=FF1A1A2E
base::background_hover=FF2A2A3E
outlined::outline=FF444466
...
```

Keys are `Class::cname + "::" + Property::pname`. Values are `juce::Colour::toDisplayString(true)` (always 8-char AARRGGBB, no `#` prefix in the file).

#### API

```cpp
namespace sst::jucegui::style
{
struct Serializer
{
    // Serialize all colour properties in the sheet to text.
    // Font properties are skipped (not serializable this way).
    static std::string toString(const StyleSheet::ptr_t& sheet);

    // Parse text produced by toString() and apply colours to sheet.
    // Unknown keys are silently ignored (forward compatibility).
    // Returns false if the string is unparseable.
    static bool fromString(const StyleSheet::ptr_t& sheet, const std::string& content);
};
}
```

File I/O is the responsibility of the calling application. `toString`/`fromString` deal in `std::string` only; the caller reads/writes the file using whatever mechanism fits (JUCE `File`, `std::filesystem`, etc.).

#### Implementation sketch

**`toString`**: Iterates `StyleSheet::allClasses` → for each class, iterates `StyleSheet::allProperties[cls]` → skips `Property::FONT` type → calls `sheet->getColour(cls, prop)` → formats line as `cname::pname=AARRGGBB`.

**`fromString`**: Splits on newlines, skips `#` comment lines. For each `key=value` line: splits on `::` to get class name and remainder, splits remainder on `=` to get property name and colour hex. Searches `StyleSheet::allClasses` by `strcmp(cls->cname, classNameStr)`, then searches `StyleSheet::allProperties[cls]` by `strcmp(prop->pname, propNameStr)`. If found and `prop->type == Property::COLOUR`, calls `sheet->setColour(*cls, *prop, juce::Colour::fromString(valueStr))`.

#### Six Sines integration

The `ColorEditor`'s hamburger menu gets two items via `addAdditionalHamburgerComponent` — or, more simply, the `ColorEditor`'s `NamedPanel` hamburger callback is set by the caller in `six-sines-editor.cpp` to show a small popup with:

- **"Save Color Theme…"** — `juce::FileChooser` for `.ssttheme` files; on success reads `Serializer::toString(style())` and writes it to the chosen file using `juce::File::replaceWithText`.
- **"Load Color Theme…"** — `juce::FileChooser` to open `.ssttheme`; on success reads file text with `juce::File::loadFileAsString`, calls `Serializer::fromString(style(), text)`, then calls `repaint()` on the main editor and refreshes the color editor's list.

All `juce::File` usage stays in `six-sines-editor.cpp`; `Serializer.h` never sees a file path.

---

### Updated File Summary

| File | Action |
|---|---|
| `include/sst/jucegui/screens/ColorEditor.h` | **New** — Parts 1–3 in one header |
| `include/sst/jucegui/style/Serializer.h` | **New** — Part 5 (stylesheet text serialization) |
| `src/ui/six-sines-editor.h` | Add `colorEditorWindow` member |
| `src/ui/six-sines-editor.cpp` | Add Color Editor menu item + `sixSinesBaseColorMap()` helper; Save/Load theme items |
