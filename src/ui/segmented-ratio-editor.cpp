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
#include <cmath>
#include <algorithm>

namespace baconpaul::six_sines::ui
{

namespace jcmp = sst::jucegui::components;
namespace jdat = sst::jucegui::data;

struct SegmentedRatioEditor::DigitSource : jdat::Continuous
{
    SegmentedRatioEditor *parent;
    int slot; // 0=t1 (integer), 1=t2 (hundredths), 2=t3 (hundred-thousandths)

    DigitSource(SegmentedRatioEditor *p, int s) : parent(p), slot(s) {}

    std::string getLabel() const override
    {
        switch (slot)
        {
        case 0:
            return "Integer";
        case 1:
            return "10^-2";
        case 2:
            return "10^-4";
        }
        return "";
    }

    float getValue() const override
    {
        int t1, t2, t3;
        parent->readDigits(t1, t2, t3);
        if (slot == 0)
            return static_cast<float>(t1);
        if (slot == 1)
            return static_cast<float>(t2);
        return static_cast<float>(t3);
    }

    void setValueFromGUI(const float &f) override
    {
        int t1, t2, t3;
        parent->readDigits(t1, t2, t3);
        int v = static_cast<int>(std::round(f));
        if (slot == 0)
        {
            // Skip 0 in the same direction as the jog buttons: dragging from a
            // positive value down past 0 lands on -1; dragging a negative value
            // up past 0 lands on +1. Without this, dragging through zero stalls
            // because v=0 gets rebounded to the previous side.
            if (v == 0)
                v = (t1 > 0) ? -1 : 1;
            t1 = std::clamp(v, static_cast<int>(getMin()), static_cast<int>(getMax()));
        }
        else if (slot == 1)
        {
            t2 = std::clamp(v, 0, 99);
        }
        else
        {
            t3 = std::clamp(v, 0, 99);
        }
        parent->writeDigits(t1, t2, t3);
    }

    void setValueFromModel(const float &) override {}

    float getDefaultValue() const override
    {
        if (slot == 0)
            return 1.f;
        return 0.f;
    }

    float getMin() const override
    {
        if (slot == 0)
        {
            auto c = parent->continuous();
            if (!c)
                return -64.f;
            auto minLog = c->getMin();
            float mn = -1.f;
            if (minLog < 0)
                mn = -std::floor(std::pow(2.0, -minLog));
            return mn;
        }
        return 0.f;
    }

    float getMax() const override
    {
        if (slot == 0)
        {
            auto c = parent->continuous();
            if (!c)
                return 64.f;
            auto maxLog = c->getMax();
            float mx = 1.f;
            if (maxLog > 0)
                mx = std::floor(std::pow(2.0, maxLog));
            return mx;
        }
        if (slot == 1)
            return 99.f;
        return 99.f;
    }

    std::string getValueAsStringFor(float f) const override
    {
        int v = static_cast<int>(std::round(f));
        if (slot == 0)
            return std::to_string(v);
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02d", v);
        return std::string(buf);
    }

    void setValueAsString(const std::string &s) override
    {
        if (s.empty())
        {
            setValueFromGUI(getDefaultValue());
            return;
        }
        try
        {
            int v = std::stoi(s);
            setValueFromGUI(static_cast<float>(v));
        }
        catch (...)
        {
            setValueFromGUI(getDefaultValue());
        }
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

void SegmentedRatioEditor::writeDigits(int t1, int t2, int t3)
{
    auto c = continuous();
    if (!c)
        return;

    // at the t1 extremes, t2/t3 must be 0 to stay within param min/max
    int t1Min = static_cast<int>(slotSources[0]->getMin());
    int t1Max = static_cast<int>(slotSources[0]->getMax());
    if (t1 <= t1Min)
    {
        t1 = t1Min;
        t2 = 0;
        t3 = 0;
    }
    else if (t1 >= t1Max)
    {
        t1 = t1Max;
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
        // a slot widget already opened the gesture (drag); just set the value
        c->setValueFromGUI(v);
    }
    else
    {
        // jog button or typein: wrap the write in its own atomic gesture
        onBeginEdit();
        c->setValueFromGUI(v);
        onEndEdit();
    }
    for (auto &e : slotEditors)
        if (e)
            e->repaint();
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

void SegmentedRatioEditor::finalizeSetup()
{
    // seed cached digits from the current float so the UI starts coherent
    digitsValid = false;
    if (auto c = continuous())
    {
        decompose(c->getValue(), cachedT1, cachedT2, cachedT3);
        digitsValid = true;
    }

    if (!slotSources[0])
    {
        for (int i = 0; i < numSlots; ++i)
        {
            slotSources[i] = std::make_unique<DigitSource>(this, i);

            slotEditors[i] = std::make_unique<jcmp::DraggableTextEditableValue>();
            slotEditors[i]->setSource(slotSources[i].get());
            // Right-click on a slot routes to the parent's popup (the same one
            // createComponent wired for the segmented editor as a whole), so the
            // user gets the standard param menu wherever they click.
            slotEditors[i]->onPopupMenu = [this](const juce::ModifierKeys &m)
            {
                if (onPopupMenu)
                    onPopupMenu(m);
            };
            // Mark the outer gesture open BEFORE forwarding BEGIN, and clear it
            // AFTER forwarding END, so writeDigits can suppress its inner pair
            // for any sets that arrive between them (i.e. drag ticks).
            slotEditors[i]->onBeginEdit = [this]()
            {
                inOuterGesture = true;
                onBeginEdit();
            };
            slotEditors[i]->onEndEdit = [this]()
            {
                onEndEdit();
                inOuterGesture = false;
            };
            addAndMakeVisible(*slotEditors[i]);

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
    if (!slotEditors[0])
        return;
    auto b = getLocalBounds();
    auto cellW = b.getWidth() / numSlots;
    int jogH = 11;
    for (int i = 0; i < numSlots; ++i)
    {
        auto cell = juce::Rectangle<int>(b.getX() + i * cellW, b.getY(), cellW, b.getHeight());
        upButtons[i]->setBounds(cell.removeFromTop(jogH).reduced(1));
        downButtons[i]->setBounds(cell.removeFromBottom(jogH).reduced(1));
        slotEditors[i]->setBounds(cell.reduced(1, 0));
    }
}

void SegmentedRatioEditor::refreshFromExternal()
{
    digitsValid = false;
    for (auto &e : slotEditors)
        if (e)
            e->repaint();
    repaint();
}

} // namespace baconpaul::six_sines::ui
