/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024-2025, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#include "segmented-ratio-editor.h"
#include "six-sines-editor.h"
#include <cmath>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace baconpaul::six_sines::ui
{

namespace jcmp = sst::jucegui::components;

struct SegmentedRatioEditor::RatioDTE : jcmp::DraggableTextEditableValue
{
    // Layout: monospace text laid out as if every value had max width (e.g.
    // "-32.0000 "), broken into three equal-width slot rects:
    //   slot 0: sign + 2 integer digits   ("-32" / " 32" / "  2")
    //   slot 1: '.' + first 2 fractionals (".00" .. ".99")
    //   slot 2: last 2 fractionals + pad  ("00 " .. "99 ")
    // Equal widths make the jog buttons above/below identical in size.

    SegmentedRatioEditor *parent;
    int activeSlot{-1};
    int hoverSlot{-1};
    int t1Down{1}, t2Down{0}, t3Down{0};
    std::array<juce::Rectangle<int>, numSlots> slotRects;
    float fittedHeight{12.f};
    // Computed in resized() (and refreshed via onStyleChanged) so paint()
    // doesn't reconstruct the font + re-measure char width every frame.
    juce::Font cachedFont{juce::FontOptions{}};
    int cachedTextW{0};

    RatioDTE(SegmentedRatioEditor *p) : parent(p)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        // Re-bind the typein commit callbacks to our ratio-aware parser; the
        // base class's setFromEditor would route the typein string straight
        // into the bound Continuous, which here is the log2 ratio.
        underlyingEditor->onReturnKey = [this]() { commitFromEditor(); };
        underlyingEditor->onFocusLost = [this]()
        {
            if (underlyingEditor->isVisible())
                commitFromEditor();
        };
    }

    juce::Typeface::Ptr typeface()
    {
        return parent->sixSinesEditor ? parent->sixSinesEditor->typefaceFromResources(
                                            "Anonymous_Pro/AnonymousPro-Regular.ttf")
                                      : nullptr;
    }

    static constexpr float ratioKerning = 0.03f;

    juce::Font paintFont()
    {
        if (auto tf = typeface())
        {
            auto f = juce::Font(juce::FontOptions{tf}.withHeight(fittedHeight));
            f.setExtraKerningFactor(ratioKerning);
            return f;
        }
        return getFont(Styles::labelfont);
    }

    int slotFromX(int x) const
    {
        if (slotRects[0].getWidth() == 0)
            return -1;
        if (x < slotRects[0].getX() || x >= slotRects[numSlots - 1].getRight())
            return -1;
        for (int i = numSlots - 1; i >= 0; --i)
            if (x >= slotRects[i].getX())
                return i;
        return -1;
    }

    void resized() override
    {
        assert(parent->sixSinesEditor);
        DraggableTextEditableValue::resized();
        auto b = getLocalBounds();
        int slotW = b.getWidth() / numSlots;
        for (int i = 0; i < numSlots; ++i)
            slotRects[i] =
                juce::Rectangle<int>(b.getX() + i * slotW, b.getY(), slotW, b.getHeight());
        slotRects[numSlots - 1].setRight(b.getRight()); // soak up rounding

        // Pick the largest font height that lets 9 monospace chars fit the
        // editor width — equivalently, 3 chars in a slot. Measure at a
        // reference height and scale; clamp by component height too.
        if (auto tf = typeface())
        {
            constexpr float refH = 20.f;
            auto refFont = juce::Font(juce::FontOptions{tf}.withHeight(refH));
            refFont.setExtraKerningFactor(ratioKerning);
            auto refW = SST_STRING_WIDTH_FLOAT(refFont, "000000000");
            if (refW > 0)
                fittedHeight = std::min(refH * b.getWidth() / refW, (float)b.getHeight());
        }

        // Cache the painted font + the centered-text width so paint() doesn't
        // rebuild a juce::Font and run a glyph layout every frame.
        cachedFont = paintFont();
        cachedTextW = static_cast<int>(std::ceil(SST_STRING_WIDTH_FLOAT(cachedFont, "000000000")));
    }

    // The first SegmentedRatioEditor::resized() runs before the stylesheet is
    // plumbed, so getFont() falls back to a tiny placeholder and our slot rects
    // collapse. Re-run layout (and bubble up so the parent re-aligns jogs)
    // every time the style becomes valid.
    void onStyleChanged() override
    {
        DraggableTextEditableValue::onStyleChanged();
        resized();
        if (parent)
            parent->resized();
    }

    void paint(juce::Graphics &g) override
    {
        assert(parent->sixSinesEditor);
        g.setColour(getColour(Styles::background));
        if (isHovered)
            g.setColour(getColour(Styles::background_hover));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        if (underlyingEditor->isVisible())
            return;

        // hover/drag highlight on the active segment: 2px-radius filled rect
        int hl = activeSlot >= 0 ? activeSlot : hoverSlot;
        if (hl >= 0)
        {
            g.setColour(getColour(Styles::value).withAlpha(0.3f));
            g.fillRoundedRectangle(slotRects[hl].toFloat(), 2.f);
        }

        int t1, t2, t3;
        parent->readDigits(t1, t2, t3);

        g.setFont(cachedFont);
        g.setColour(isEnabled() ? getColour(Styles::value)
                                : getColour(Styles::value).withAlpha(0.5f));

        // Single 9-char monospace string spanning all three slots — drops the
        // micro-gaps that come from drawing each segment in its own rect.
        // Justification::centred measures the rendered string width and
        // centers — leading/trailing spaces in monospace can render slightly
        // narrower than digits, which shifts the decimal point as the integer
        // changes width (e.g. "1" → "-1"). Use a fixed-width target rect sized
        // to the 9-digit reference string and draw centredLeft so the column
        // positions stay locked across values.
        char buf[16];
        std::snprintf(buf, sizeof buf, "%3d.%02d%02d ", t1, t2, t3);
        auto fullRect = juce::Rectangle<int>(slotRects[0].getX(), slotRects[0].getY(),
                                             slotRects[2].getRight() - slotRects[0].getX(),
                                             slotRects[0].getHeight());
        auto target = fullRect.withSizeKeepingCentre(cachedTextW, fullRect.getHeight());
        g.drawText(buf, target, juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent &e) override
    {
        DraggableTextEditableValue::mouseDown(e);
        if (e.mods.isPopupMenu())
            return;
        activeSlot = slotFromX(e.x);
        parent->readDigits(t1Down, t2Down, t3Down);
    }

    void mouseMove(const juce::MouseEvent &e) override
    {
        DraggableTextEditableValue::mouseMove(e);
        int s = slotFromX(e.x);
        if (s != hoverSlot)
        {
            hoverSlot = s;
            repaint();
        }
    }

    void mouseEnter(const juce::MouseEvent &e) override
    {
        DraggableTextEditableValue::mouseEnter(e);
        hoverSlot = slotFromX(e.x);
        repaint();
    }

    void mouseExit(const juce::MouseEvent &e) override
    {
        DraggableTextEditableValue::mouseExit(e);
        hoverSlot = -1;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (e.mods.isPopupMenu())
            return;
        auto d = -e.getDistanceFromDragStartY();
        if (!everDragged)
        {
            parent->inOuterGesture = true;
            onBeginEdit();
        }
        everDragged = true;

        float fineFactor = e.mods.isShiftDown() ? 0.25f : 1.0f;
        float pxPerUnit = (activeSlot == 0 ? 8.0f : 4.0f) / fineFactor;
        int delta = static_cast<int>(std::round(d / pxPerUnit));

        int t1 = t1Down, t2 = t2Down, t3 = t3Down;
        if (activeSlot == 0)
        {
            int v = t1 + delta;
            // Skip 0 in the same direction as the jog buttons: dragging from a
            // positive value down past 0 lands on -1; up past 0 lands on +1.
            if (v == 0)
                v = (delta > 0) ? 1 : -1;
            t1 = std::clamp(v, parent->t1Min(), parent->t1Max());
        }
        else if (activeSlot == 1)
            t2 = std::clamp(t2 + delta, 0, 99);
        else
            t3 = std::clamp(t3 + delta, 0, 99);

        parent->writeDigits(t1, t2, t3);
        repaint();
    }

    void mouseUp(const juce::MouseEvent &e) override
    {
        if (e.mods.isPopupMenu())
            return;
        if (everDragged)
        {
            onEndEdit();
            parent->inOuterGesture = false;
        }
        else
        {
            activateRatioEditor();
        }
        everDragged = false;
        // Clear the drag highlight and re-evaluate hover from the release
        // position; if the user released outside the component, mouseExit may
        // not fire after the drag, so resync explicitly here.
        activeSlot = -1;
        hoverSlot = getLocalBounds().contains(e.getPosition()) ? slotFromX(e.x) : -1;
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent &) override { activateRatioEditor(); }

    void activateRatioEditor()
    {
        // Pre-fill with the same string the underlying source produces, so a
        // round-trip through the typein is value-preserving. PatchContinuous
        // formats fractional ratios as "1/N" (e.g. "1/4"); the segmented
        // editor displays those as "-N" (e.g. "-4"), so rewrite the prefix
        // here for visual consistency. setValueAsString in basic-blocks
        // accepts both forms, so the round-trip still works.
        auto s = continuous()->getValueAsStringWithoutUnits();
        if (s.rfind("1/", 0) == 0)
            s = "-" + s.substr(2);
        underlyingEditor->setText(s);
        underlyingEditor->setVisible(true);
        underlyingEditor->selectAll();
        underlyingEditor->grabKeyboardFocus();
    }

    void commitFromEditor()
    {
        if (!underlyingEditor->isVisible())
            return;
        auto text = underlyingEditor->getText().toStdString();
        underlyingEditor->setVisible(false);

        // Route through the bound source's own parser (PatchContinuous
        // handles the ratio→log2 conversion). The begin/end wrapper has to
        // surround the SET — clap-helpers warns when a CLAP_EVENT_PARAM_VALUE
        // arrives without an open gesture marker. setValueAsString may silently
        // no-op on a parse failure, in which case begin+end fan out to an
        // empty gesture pair; the engine tolerates that, but tolerates a
        // stray SET (begin-after-set) far less.
        onBeginEdit();
        if (text.empty())
            continuous()->setValueFromGUI(continuous()->getDefaultValue());
        else
            continuous()->setValueAsString(text);
        onEndEdit();
        parent->refreshFromExternal();
    }
};

SegmentedRatioEditor::SegmentedRatioEditor()
    : sst::jucegui::components::ContinuousParamEditor(
          sst::jucegui::components::ContinuousParamEditor::Direction::HORIZONTAL)
{
}

SegmentedRatioEditor::~SegmentedRatioEditor() = default;

void SegmentedRatioEditor::decompose(float val, int &t1, int &t2, int &t3)
{
    int sign = (val < 0) ? -1 : 1;
    double absVal = std::fabs(val);
    double decimal = std::pow(2.0, absVal);
    // Snap to the precision we display (X.XXXX) by going through integer space
    // once, then doing the divmod with ints. Going through floating-point
    // arithmetic for the divmod re-introduces drift: e.g. 24000.0 / 10000.0
    // in IEEE double is 2.3999999999999999, so 2.4 → 2|39|99 with std::floor.
    long long totalUnits = static_cast<long long>(std::llround(decimal * 10000.0));
    if (totalUnits < 10000)
        totalUnits = 10000; // can't represent below 1 in the integer slot
    long long integer = totalUnits / 10000;
    long long rem = totalUnits % 10000;

    t1 = sign * static_cast<int>(integer);
    t2 = static_cast<int>(rem / 100);
    t3 = static_cast<int>(rem % 100);
}

float SegmentedRatioEditor::recompose(int t1, int t2, int t3) const
{
    if (t1 == 0)
        t1 = 1;
    int sign = (t1 < 0) ? -1 : 1;
    double decimal = std::abs(t1) + t2 / 100.0 + t3 / 10000.0;
    if (decimal < 1e-6)
        decimal = 1e-6;
    double val = sign * std::log2(decimal);
    auto c = const_cast<SegmentedRatioEditor *>(this)->continuous();
    if (c)
        val = std::clamp(static_cast<float>(val), c->getMin(), c->getMax());
    return static_cast<float>(val);
}

void SegmentedRatioEditor::readDigits(int &t1, int &t2, int &t3) const
{
    if (digitsValid)
    {
        t1 = cachedT1;
        t2 = cachedT2;
        t3 = cachedT3;
        return;
    }
    auto c = const_cast<SegmentedRatioEditor *>(this)->continuous();
    if (!c)
    {
        t1 = 1;
        t2 = 0;
        t3 = 0;
        return;
    }
    decompose(c->getValue(), t1, t2, t3);
    cachedT1 = t1;
    cachedT2 = t2;
    cachedT3 = t3;
    digitsValid = true;
}

int SegmentedRatioEditor::t1Min() const
{
    auto c = const_cast<SegmentedRatioEditor *>(this)->continuous();
    if (!c)
        return -64;
    auto minLog = c->getMin();
    if (minLog < 0)
        return -static_cast<int>(std::floor(std::pow(2.0, -minLog)));
    return -1;
}

int SegmentedRatioEditor::t1Max() const
{
    auto c = const_cast<SegmentedRatioEditor *>(this)->continuous();
    if (!c)
        return 64;
    auto maxLog = c->getMax();
    if (maxLog > 0)
        return static_cast<int>(std::floor(std::pow(2.0, maxLog)));
    return 1;
}

void SegmentedRatioEditor::writeDigits(int t1, int t2, int t3)
{
    auto c = continuous();
    if (!c)
        return;

    int mn = t1Min();
    int mx = t1Max();
    if (t1 <= mn)
    {
        t1 = mn;
        t2 = 0;
        t3 = 0;
    }
    else if (t1 >= mx)
    {
        t1 = mx;
        t2 = 0;
        t3 = 0;
    }

    cachedT1 = t1;
    cachedT2 = t2;
    cachedT3 = t3;
    digitsValid = true;
    auto v = recompose(t1, t2, t3);
    if (inOuterGesture)
    {
        // a drag gesture is already open; just set the value
        c->setValueFromGUI(v);
    }
    else
    {
        // jog button or typein: wrap the write in its own atomic gesture
        onBeginEdit();
        c->setValueFromGUI(v);
        onEndEdit();
    }
    if (editor)
        editor->repaint();
    repaint();
}

void SegmentedRatioEditor::jogSlot(int slot, bool up)
{
    int t1, t2, t3;
    readDigits(t1, t2, t3);
    int delta = up ? 1 : -1;
    if (slot == 0)
    {
        if (t1 == 1 && !up)
            t1 = -1;
        else if (t1 == -1 && up)
            t1 = 1;
        else
            t1 += delta;
    }
    else if (slot == 1)
    {
        t2 = std::clamp(t2 + delta, 0, 99);
    }
    else
    {
        t3 = std::clamp(t3 + delta, 0, 99);
    }
    writeDigits(t1, t2, t3);
}

void SegmentedRatioEditor::finalizeSetup(SixSinesEditor &e)
{
    sixSinesEditor = &e;
    // seed cached digits from the current float so the UI starts coherent
    digitsValid = false;
    if (auto c = continuous())
    {
        decompose(c->getValue(), cachedT1, cachedT2, cachedT3);
        digitsValid = true;
    }

    if (!editor)
    {
        editor = std::make_unique<RatioDTE>(this);
        // Bind the inner editor to the same Continuous as the parent so
        // mouseDown/mouseDrag/setValueAsString go through the canonical source.
        // Side effect: the source ends up with two GUI data listeners (parent
        // + child), so external writes fire dataChanged() on both — i.e. two
        // repaints. Cheap, but worth knowing if you ever debug a redraw loop.
        if (auto c = continuous())
            editor->setSource(c);
        editor->onPopupMenu = [this](const juce::ModifierKeys &m)
        {
            if (onPopupMenu)
                onPopupMenu(m);
        };
        editor->onBeginEdit = [this]() { onBeginEdit(); };
        editor->onEndEdit = [this]() { onEndEdit(); };
        addAndMakeVisible(*editor);

        for (int i = 0; i < numSlots; ++i)
        {
            upButtons[i] = std::make_unique<jcmp::GlyphButton>(jcmp::GlyphPainter::JOG_UP);
            upButtons[i]->setOnCallback([this, i]() { jogSlot(i, true); });
            upButtons[i]->setLongHoldRepeats(true, 500, 100);
            addAndMakeVisible(*upButtons[i]);

            downButtons[i] = std::make_unique<jcmp::GlyphButton>(jcmp::GlyphPainter::JOG_DOWN);
            downButtons[i]->setOnCallback([this, i]() { jogSlot(i, false); });
            downButtons[i]->setLongHoldRepeats(true, 500, 100);
            addAndMakeVisible(*downButtons[i]);
        }
    }
    resized();
    repaint();
}

void SegmentedRatioEditor::resized()
{
    if (!editor)
        return;
    auto b = getLocalBounds();
    int jogH = 11;
    auto top = b.removeFromTop(jogH);
    auto bot = b.removeFromBottom(jogH);
    editor->setBounds(b.reduced(1, 0)); // triggers editor->resized() to compute slotRects
    for (int i = 0; i < numSlots; ++i)
    {
        auto sr = editor->slotRects[i];
        int x = sr.getX() + editor->getX();
        int w = sr.getWidth();
        upButtons[i]->setBounds(juce::Rectangle<int>(x, top.getY(), w, jogH).reduced(1));
        downButtons[i]->setBounds(juce::Rectangle<int>(x, bot.getY(), w, jogH).reduced(1));
    }
}

void SegmentedRatioEditor::refreshFromExternal()
{
    digitsValid = false;
    if (editor)
        editor->repaint();
    repaint();
}

} // namespace baconpaul::six_sines::ui
