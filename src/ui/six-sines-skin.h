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

#ifndef BACONPAUL_SIX_SINES_UI_SIX_SINES_SKIN_H
#define BACONPAUL_SIX_SINES_UI_SIX_SINES_SKIN_H

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/style/StyleSheet.h>
#include <sst/jucegui/components/BaseStyles.h>
#include <sst/jucegui/components/JogUpDownButton.h>
#include <sst/jucegui/components/MenuButton.h>
#include <sst/jucegui/components/MultiSwitch.h>
#include <sst/jucegui/components/Knob.h>
#include <sst/jucegui/components/NamedPanel.h>
#include <sst/jucegui/components/WindowPanel.h>
#include <sst/jucegui/components/VUMeter.h>

namespace baconpaul::six_sines::ui
{

/*
 * SixSinesSkin — logical colour layer for the Six Sines UI.
 *
 * Holds a small set of semantic colours and projects them onto the full set of
 * sst-jucegui mechanical stylesheet properties via applyToStylesheet().
 *
 * Hover variants are derived automatically: brighter on dark backgrounds,
 * darker on light backgrounds.
 */
struct SixSinesSkin
{
    enum class LogicalColor : size_t
    {
        Background,              // panel/widget fill
        WindowBackground,        // root window gradient base
        Border,                  // outlines, dividers, rules
        Label,                   // general text (labels, button text via inheritance)
        Value,                   // accent/knob fill — the "orange" in dark mode
        Gutter,                  // slider track, value-background area
        Handle,                  // draggable slider thumb
        MainMenuFill,            // popup menu background
        ButtonFill,              // push button fill
        VUGradStart,             // VU meter gradient start (bottom of range)
        VUGradEnd,               // VU meter gradient end (top of range)
        VUOverload,              // VU meter overload indicator
        PatchSelectorBackground, // preset browser jog fill — distinct from other jogs
        PatchSelectorText,       // preset browser jog label text
        MatrixGridOverlay,       // alternating row/col highlight in the matrix panel
        KnobBody,                // knob body (circle) fill colour
        COUNT
    };

    static constexpr size_t numColors = (size_t)LogicalColor::COUNT;
    std::array<juce::Colour, numColors> colors{};

    // The stylesheet class used by the preset browser jog button (matches the string in
    // six-sines-editor.cpp's local PatchMenu const).
    static constexpr sst::jucegui::style::StyleSheet::Class patchSelectorClass{
        "six-sines.patch-menu"};

    juce::Colour get(LogicalColor lc) const { return colors[(size_t)lc]; }

    SixSinesSkin &set(LogicalColor lc, juce::Colour c)
    {
        colors[(size_t)lc] = c;
        return *this;
    }

    bool isDark() const { return get(LogicalColor::Background).getBrightness() < 0.5f; }

    static const char *nameFor(LogicalColor lc)
    {
        switch (lc)
        {
        case LogicalColor::Background:
            return "Background";
        case LogicalColor::WindowBackground:
            return "Window Background";
        case LogicalColor::Border:
            return "Border";
        case LogicalColor::Label:
            return "Label";
        case LogicalColor::Value:
            return "Value";
        case LogicalColor::Gutter:
            return "Gutter";
        case LogicalColor::Handle:
            return "Handle";
        case LogicalColor::MainMenuFill:
            return "Menu Fill";
        case LogicalColor::ButtonFill:
            return "Button Fill";
        case LogicalColor::VUGradStart:
            return "VU Grad Start";
        case LogicalColor::VUGradEnd:
            return "VU Grad End";
        case LogicalColor::VUOverload:
            return "VU Overload";
        case LogicalColor::PatchSelectorBackground:
            return "Patch Selector Background";
        case LogicalColor::PatchSelectorText:
            return "Patch Selector Text";
        case LogicalColor::MatrixGridOverlay:
            return "Matrix Grid Overlay";
        case LogicalColor::KnobBody:
            return "Knob Body";
        default:
            return "Unknown";
        }
    }

    // Whether this logical color should appear in the colour editor.
    static bool isEditable(LogicalColor lc) { return lc != LogicalColor::COUNT; }

    static SixSinesSkin darkDefault()
    {
        SixSinesSkin s;
        s.set(LogicalColor::Background, juce::Colour(0x25, 0x25, 0x28));
        s.set(LogicalColor::WindowBackground, juce::Colour(0x2A, 0x2B, 0x2E));
        s.set(LogicalColor::Border, juce::Colour(0x50, 0x50, 0x50));
        s.set(LogicalColor::Label, juce::Colour(220, 220, 220));
        s.set(LogicalColor::Value, juce::Colour(0xFF, 0x90, 0x00));
        s.set(LogicalColor::Gutter, juce::Colour(0x05, 0x05, 0x00));
        s.set(LogicalColor::Handle, juce::Colour(0xD0, 0xD0, 0xD0));
        s.set(LogicalColor::MainMenuFill, juce::Colour(0x25, 0x25, 0x28));
        s.set(LogicalColor::ButtonFill, juce::Colour(0x30, 0x30, 0x33));
        s.set(LogicalColor::VUGradStart, juce::Colour(200, 200, 100));
        s.set(LogicalColor::VUGradEnd, juce::Colour(100, 100, 220));
        s.set(LogicalColor::VUOverload, juce::Colour(200, 50, 50));
        s.set(LogicalColor::PatchSelectorBackground, juce::Colour(0x15, 0x15, 0x15));
        s.set(LogicalColor::PatchSelectorText, juce::Colour(0xEE, 0xEE, 0xEE));
        s.set(LogicalColor::MatrixGridOverlay, s.get(LogicalColor::Background).brighter(0.1f));
        s.set(LogicalColor::KnobBody, juce::Colour(82, 82, 82));
        return s;
    }

    void applyToStylesheet(sst::jucegui::style::StyleSheet::ptr_t sheet) const
    {
        namespace jbs = sst::jucegui::components::base_styles;
        namespace jcmp = sst::jucegui::components;

        bool dark = isDark();

        // Returns a brighter colour on dark backgrounds, darker on light backgrounds.
        auto adj = [dark](juce::Colour c, float amount = 0.2f) -> juce::Colour
        { return dark ? c.brighter(amount) : c.darker(amount); };

        // ---- Background ----
        auto bg = get(LogicalColor::Background);
        sheet->setColour(jbs::Base::styleClass, jbs::Base::background, bg);
        sheet->setColour(jbs::Base::styleClass, jbs::Base::background_hover, adj(bg, 0.15f));

        // ---- Window background (vertical gradient) ----
        auto winBg = get(LogicalColor::WindowBackground);
        sheet->setColour(jcmp::WindowPanel::Styles::styleClass, jcmp::WindowPanel::Styles::bgstart,
                         winBg.brighter(0.05f));
        sheet->setColour(jcmp::WindowPanel::Styles::styleClass, jcmp::WindowPanel::Styles::bgend,
                         winBg.darker(0.1f));

        // ---- Border ----
        auto border = get(LogicalColor::Border);
        sheet->setColour(jbs::Outlined::styleClass, jbs::Outlined::outline, border);
        sheet->setColour(jbs::Outlined::styleClass, jbs::Outlined::brightoutline,
                         adj(border, 0.25f));
        // NamedPanel rule/divider line is slightly brighter than the base border
        sheet->setColour(jcmp::NamedPanel::Styles::styleClass, jcmp::NamedPanel::Styles::labelrule,
                         adj(border, 0.25f));

        // ---- Label (general text) ----
        auto label = get(LogicalColor::Label);
        sheet->setColour(jbs::BaseLabel::styleClass, jbs::BaseLabel::labelcolor, label);
        sheet->setColour(jbs::BaseLabel::styleClass, jbs::BaseLabel::labelcolor_hover,
                         adj(label, 0.15f));

        // Declare menuFill early so it can be used in both the popup highlight and
        // the main menu fill sections below.
        auto menuFill = get(LogicalColor::MainMenuFill);

        // ---- Value (accent / knob fill) ----
        auto val = get(LogicalColor::Value);
        sheet->setColour(jbs::ValueBearing::styleClass, jbs::ValueBearing::value, val);
        sheet->setColour(jbs::ValueBearing::styleClass, jbs::ValueBearing::value_hover, adj(val));
        // valuelabel: text drawn ON the value indicator — must contrast strongly with val
        auto valLabel =
            val.getBrightness() > 0.5f ? juce::Colour(20, 10, 20) : juce::Colour(220, 220, 220);
        sheet->setColour(jbs::ValueBearing::styleClass, jbs::ValueBearing::valuelabel, valLabel);
        sheet->setColour(jbs::ValueBearing::styleClass, jbs::ValueBearing::valuelabel_hover,
                         adj(valLabel, 0.1f));
        // NamedPanel accent (selected tab highlight, accented panel outline)
        sheet->setColour(jcmp::NamedPanel::Styles::styleClass,
                         jcmp::NamedPanel::Styles::selectedtab, val);
        sheet->setColour(jcmp::NamedPanel::Styles::styleClass,
                         jcmp::NamedPanel::Styles::accentedPanel, val);
        // Popup menu highlight row: shift the menu fill colour slightly so the selected
        // item stands out without introducing an accent-colour tint.
        auto hlBg =
            menuFill.getBrightness() < 0.5f ? menuFill.brighter(0.15f) : menuFill.darker(0.15f);
        sheet->setColour(jbs::PopupMenu::styleClass, jbs::PopupMenu::hightlightbackground, hlBg);
        sheet->setColour(jbs::PopupMenu::styleClass, jbs::PopupMenu::hightlighttextcolor, label);

        // ---- Gutter (slider track / value background) ----
        auto gutter = get(LogicalColor::Gutter);
        sheet->setColour(jbs::ValueGutter::styleClass, jbs::ValueGutter::gutter, gutter);
        sheet->setColour(jbs::ValueGutter::styleClass, jbs::ValueGutter::gutter_hover,
                         adj(gutter, 0.3f));
        // valuebg: a darker variant of gutter used as the filled area behind value arcs
        sheet->setColour(jbs::ValueBearing::styleClass, jbs::ValueBearing::valuebg,
                         dark ? gutter.darker(0.4f) : gutter.darker(0.1f));

        // ---- Handle (slider thumb) ----
        auto handle = get(LogicalColor::Handle);
        sheet->setColour(jbs::GraphicalHandle::styleClass, jbs::GraphicalHandle::handle, handle);
        sheet->setColour(jbs::GraphicalHandle::styleClass, jbs::GraphicalHandle::handle_hover,
                         adj(handle));
        // handle_outline: subtle blend of background into handle colour
        sheet->setColour(jbs::GraphicalHandle::styleClass, jbs::GraphicalHandle::handle_outline,
                         bg.interpolatedWith(handle, 0.2f));

        // ---- Main menu fill ----
        // Setting Base::background on the PopupMenu class overrides the global base background
        // specifically for popup menu widgets.
        sheet->setColour(jbs::PopupMenu::styleClass, jbs::Base::background, menuFill);
        sheet->setColour(jbs::PopupMenu::styleClass, jbs::Base::background_hover,
                         adj(menuFill, 0.15f));

        // ---- Button fill ----
        auto btnFill = get(LogicalColor::ButtonFill);
        sheet->setColour(jbs::PushButton::styleClass, jbs::PushButton::fill, btnFill);
        sheet->setColour(jbs::PushButton::styleClass, jbs::PushButton::fill_hover, adj(btnFill));
        sheet->setColour(jbs::PushButton::styleClass, jbs::PushButton::fill_pressed,
                         adj(btnFill, 0.4f));

        // ---- JogUpDownButton ----
        // The built-in theme sets these on the jogupdownbutton styleClass directly, so they
        // take precedence over base-class overrides. We must mirror them explicitly.
        sheet->setColour(jcmp::JogUpDownButton::Styles::styleClass,
                         jcmp::JogUpDownButton::Styles::jogbutton_hover, val);
        sheet->setColour(jcmp::JogUpDownButton::Styles::styleClass, jbs::PushButton::fill, btnFill);
        sheet->setColour(jcmp::JogUpDownButton::Styles::styleClass, jbs::BaseLabel::labelcolor,
                         label);
        sheet->setColour(jcmp::JogUpDownButton::Styles::styleClass,
                         jbs::BaseLabel::labelcolor_hover, adj(label));

        // ---- MenuButton ----
        // The built-in theme sets menuarrow_hover on the menubutton styleClass. We must
        // override it explicitly so the accent colour stays in sync with Value.
        sheet->setColour(jcmp::MenuButton::Styles::styleClass,
                         jcmp::MenuButton::Styles::menuarrow_hover, val);
        sheet->setColour(jcmp::MenuButton::Styles::styleClass, jbs::BaseLabel::labelcolor, label);
        sheet->setColour(jcmp::MenuButton::Styles::styleClass, jbs::BaseLabel::labelcolor_hover,
                         adj(label));

        // ---- MultiSwitch ----
        // The built-in theme sets per-class overrides on the multiswitch styleClass for
        // valuebg and unselected_hover. Override value/value_hover here too so the on-state
        // uses the current Value logical colour rather than the built-in default.
        sheet->setColour(jcmp::MultiSwitch::Styles::styleClass, jbs::ValueBearing::value, val);
        sheet->setColour(jcmp::MultiSwitch::Styles::styleClass, jbs::ValueBearing::value_hover,
                         adj(val));
        sheet->setColour(jcmp::MultiSwitch::Styles::styleClass, jbs::ValueBearing::valuelabel,
                         valLabel);
        sheet->setColour(jcmp::MultiSwitch::Styles::styleClass, jbs::ValueBearing::valuelabel_hover,
                         adj(valLabel, 0.1f));
        sheet->setColour(jcmp::MultiSwitch::Styles::styleClass, jbs::ValueBearing::valuebg,
                         dark ? gutter.darker(0.4f) : gutter.darker(0.1f));
        sheet->setColour(jcmp::MultiSwitch::Styles::styleClass,
                         jcmp::MultiSwitch::Styles::unselected_hover, adj(bg, 0.3f));

        // ---- Knob body ----
        sheet->setColour(jcmp::Knob::Styles::styleClass, jcmp::Knob::Styles::knobbase,
                         get(LogicalColor::KnobBody));

        // ---- VU Meter ----
        // vu_gutter is always near-black (the unlit background of the meter)
        sheet->setColour(jcmp::VUMeter::Styles::styleClass, jcmp::VUMeter::Styles::vu_gutter,
                         juce::Colour(0, 0, 0));
        sheet->setColour(jcmp::VUMeter::Styles::styleClass, jcmp::VUMeter::Styles::vu_gradstart,
                         get(LogicalColor::VUGradStart));
        sheet->setColour(jcmp::VUMeter::Styles::styleClass, jcmp::VUMeter::Styles::vu_gradend,
                         get(LogicalColor::VUGradEnd));
        sheet->setColour(jcmp::VUMeter::Styles::styleClass, jcmp::VUMeter::Styles::vu_overload,
                         get(LogicalColor::VUOverload));

        // ---- Patch Selector (preset browser jog) ----
        // Uses a dedicated stylesheet class that inherits JogUpDownButton, giving it a
        // distinct fill so the patch selector stands out from other jog buttons.
        auto psBg = get(LogicalColor::PatchSelectorBackground);
        auto psText = get(LogicalColor::PatchSelectorText);
        sheet->setColour(patchSelectorClass, jbs::PushButton::fill, psBg);
        sheet->setColour(patchSelectorClass, jbs::PushButton::fill_hover, adj(psBg));
        sheet->setColour(patchSelectorClass, jbs::BaseLabel::labelcolor, psText);
        sheet->setColour(patchSelectorClass, jbs::BaseLabel::labelcolor_hover, adj(psText));
        sheet->setColour(patchSelectorClass, jcmp::JogUpDownButton::Styles::jogbutton_hover, val);
    }

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    // Serialize all logical colours to an XML string.
    // Each colour is stored as an ARGB hex string in a <Color name="..." rgba="AARRGGBB"/> child.
    std::string toXmlString() const
    {
        juce::XmlElement root("SixSinesSkin");
        root.setAttribute("version", 1);
        for (size_t i = 0; i < numColors; ++i)
        {
            auto lc = static_cast<LogicalColor>(i);
            auto *child = root.createNewChildElement("Color");
            child->setAttribute("name", nameFor(lc));
            child->setAttribute("rgba", colors[i].toDisplayString(true).toUpperCase());
        }
        return root.toString().toStdString();
    }

    // Deserialize from an XML string produced by toXmlString().
    // Missing entries are taken from `fallback` so the format is forward- and backward-compatible.
    static SixSinesSkin fromXmlString(const std::string &xml,
                                      const SixSinesSkin &fallback = darkDefault())
    {
        SixSinesSkin result = fallback;
        auto parsed = juce::XmlDocument::parse(juce::String(xml));
        if (!parsed || parsed->getTagName() != "SixSinesSkin")
            return result;

        for (auto *child = parsed->getFirstChildElement(); child != nullptr;
             child = child->getNextElement())
        {
            if (child->getTagName() != "Color")
                continue;
            auto name = child->getStringAttribute("name").toStdString();
            auto rgba = child->getStringAttribute("rgba").toStdString();
            if (rgba.empty())
                continue;
            // Match by name to a LogicalColor
            for (size_t i = 0; i < numColors; ++i)
            {
                auto lc = static_cast<LogicalColor>(i);
                if (std::string(nameFor(lc)) == name)
                {
                    // juce::Colour::fromString expects AARRGGBB
                    result.colors[i] = juce::Colour::fromString(juce::String(rgba));
                    break;
                }
            }
        }
        return result;
    }
};

} // namespace baconpaul::six_sines::ui

#endif // BACONPAUL_SIX_SINES_UI_SIX_SINES_SKIN_H
