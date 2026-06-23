# Manual 1.2 — Screenshot Shot List

Capture targets for `doc/manual.md`. The 1.1 images were portrait
(1386×1692); the 1.2 UI is landscape (engine default 1048×690), so every
shot is fresh. Shoot at a zoom that gives crisp text — 125–150% then
crop — and use the **dark** theme as the default look unless noted.

Suggested workflow: launch the standalone
(`cmake-build-debug/six-sines_assets/Six Sines.app`), set the state
described, grab the window, crop to the relevant region, save into
`doc/` under the filename given.

| File | Status | What to set up |
|------|--------|----------------|
| `sxsn_main.png` | re-shoot | Full window overview |
| `sxsn_sigpath.png` | re-shoot + annotate | Full window, then hand-draw the path lines |
| `sxsn_source.png` | new | A source selected, extended mode showing |
| `sxsn_node.png` | new | A node's edit panel (env + LFO + mod) |
| `sxsn_macro.png` | new | A SuperMacro's edit panel |
| `sxsn_settings.png` | new | The Play / Settings screen |
| `sxsn_analyzer.png` | new | The analyzer window |
| `sxsn_theme.png` | new | The Color Editor window |

Old files `sxsn_sub.png` (the old portrait node panel) is no longer
referenced and can be deleted once the new shots land.

---

## `sxsn_main.png` — main interface overview
- Load a representative patch (or Init) so the matrix/mixer show some
  life, nothing selected (or just the main output) so the edit panel is
  in a neutral state.
- Capture the **whole window**: top bar (patch selector, analyzer
  toggle, VU), Sources, Matrix, Main, Mixer, Macro strip, settings
  strip, and the edit panel down the right.
- This is the anchor image; make sure all regions named in the
  "Main Interface" section are visible.

## `sxsn_sigpath.png` — the visual signal path (HAND-ANNOTATED)
- Same framing as `sxsn_main.png`.
- Then, as in the 1.1 manual, **hand-draw the path lines in an image
  editor**: yellow from op3's ratio → its feedback cell → mixer → main
  output; green from op3 into the op3→4, op3→5, op3→6 matrix cells.
- This is the one image that isn't a straight screen grab. Yellow =
  audio to output, green = audio used as modulation.

## `sxsn_source.png` — a source and its extended mode
- Select a source/operator so its editor fills the right panel.
- Put it in an extended mode that reads well — **Resonant Sweep** or
  **Phase Map** — so the M/N controls and the mode-specific plot show.
- If easy, show the **segmented ratio editor** variant (the '...' swap)
  rather than the plain knob, since that's a new 1.2 widget.
- Crop to the source region + the edit panel.

## `sxsn_node.png` — inside a node
- Select an ordinary node (a mixer level or a matrix cell is good) so
  the editor shows the **DAHDSR envelope, the LFO, and the 3 modulation
  slots** together.
- Bonus if the LFO is in **step-sequencer** mode, since that's new and
  visually distinct from the curve LFO.
- Crop to the edit panel (plus a little of the selected knob with its
  highlight, to show the click→edit relationship).

## `sxsn_macro.png` — a SuperMacro
- Select a macro so its editor shows the per-voice **envelope + LFO +
  3-slot matrix**.
- If a macro is actually wired to something in the loaded patch, open
  its **in-use** display so the "where am I used" list is visible.
- Crop to the macro strip + edit panel.

## `sxsn_settings.png` — the Play / Settings screen
- From the Main panel, open the **Play / Settings** screen in the edit
  panel.
- Aim to show as much of it as fits: Play (poly/mono/piano, voices,
  unison, porta, trigger), Bend, MPE, Smoothing, Zoom/Theme, and the
  **output stage** (Saturation, Sample Rate ZOH, Bit Depth, High Pass).
- If it doesn't all fit in one frame, prioritize the output stage row,
  since that's the most new-to-1.2 content.

## `sxsn_analyzer.png` — the analyzer
- Press the analyzer toggle next to the VU (or main menu → Show
  Analyzer).
- Play/hold a note with some harmonic content so the **spectrum and
  scope** both show signal.
- Capture the analyzer window itself (a sliver of the main window behind
  it is fine for context).

## `sxsn_theme.png` — the Color Editor
- Main menu → "Color Editor...".
- Capture the Color Editor window with the palette controls visible.
- Optional: if you want to show off theming, capture it next to the main
  window so the live recolor is apparent.
