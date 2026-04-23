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

#ifndef BACONPAUL_SIX_SINES_UI_SIX_SINES_EDITOR_IMPL_H
#define BACONPAUL_SIX_SINES_UI_SIX_SINES_EDITOR_IMPL_H

#include "six-sines-editor.h"

#include "sst/clap_juce_shim/menu_helper.h"
#include <sst/jucegui/component-adapters/ComponentTags.h>

namespace baconpaul::six_sines::ui
{
template <typename T>
    requires HasContinuous<T>
struct MenuValueTypein : HasEditor, juce::PopupMenu::CustomComponent, juce::TextEditor::Listener
{
    std::unique_ptr<juce::TextEditor> textEditor;
    juce::Component::SafePointer<T> underComp;

    MenuValueTypein(SixSinesEditor &editor, juce::Component::SafePointer<T> under)
        : juce::PopupMenu::CustomComponent(false), HasEditor(editor), underComp(under)
    {
        textEditor = std::make_unique<juce::TextEditor>();
        textEditor->setWantsKeyboardFocus(true);
        textEditor->addListener(this);
        textEditor->setIndents(2, 0);

        addAndMakeVisible(*textEditor);
    }

    void getIdealSize(int &w, int &h) override
    {
        w = 180;
        h = 22;
    }
    void resized() override { textEditor->setBounds(getLocalBounds().reduced(3, 1)); }

    void visibilityChanged() override
    {
        juce::Timer::callAfterDelay(
            2,
            [this]()
            {
                if (textEditor->isVisible())
                {
                    textEditor->setText(getInitialText(),
                                        juce::NotificationType::dontSendNotification);
                    auto valCol =
                        editor.style()->getColour(jcmp::base_styles::ValueBearing::styleClass,
                                                  jcmp::base_styles::ValueBearing::value);
                    textEditor->setColour(juce::TextEditor::ColourIds::backgroundColourId,
                                          valCol.withAlpha(0.1f));
                    textEditor->setColour(juce::TextEditor::ColourIds::highlightColourId,
                                          valCol.withAlpha(0.15f));
                    textEditor->setJustification(juce::Justification::centredLeft);
                    textEditor->setColour(juce::TextEditor::ColourIds::outlineColourId,
                                          juce::Colour());
                    textEditor->setColour(juce::TextEditor::ColourIds::focusedOutlineColourId,
                                          juce::Colour());
                    auto labelCol =
                        editor.style()->getColour(jcmp::base_styles::BaseLabel::styleClass,
                                                  jcmp::base_styles::BaseLabel::labelcolor);
                    textEditor->setColour(juce::TextEditor::ColourIds::textColourId, labelCol);
                    textEditor->setColour(juce::TextEditor::ColourIds::highlightedTextColourId,
                                          labelCol);
                    textEditor->setBorder(juce::BorderSize<int>(3));
                    textEditor->applyColourToAllText(valCol, true);
                    textEditor->grabKeyboardFocus();
                    textEditor->selectAll();
                }
            });
    }

    std::string getInitialText() const { return underComp->continuous()->getValueAsString(); }

    void setValueString(const std::string &s)
    {
        if (underComp && underComp->continuous())
        {
            underComp->onBeginEdit();

            if (s.empty())
            {
                underComp->continuous()->setValueFromGUI(
                    underComp->continuous()->getDefaultValue());
            }
            else
            {
                underComp->continuous()->setValueAsString(s);
            }
            underComp->onEndEdit();
            underComp->repaint();
            underComp->grabKeyboardFocus();
            underComp->notifyAccessibleChange();
        }
    }

    void textEditorReturnKeyPressed(juce::TextEditor &ed) override
    {
        auto s = ed.getText().toStdString();
        setValueString(s);
        triggerMenuItem();
    }
    void textEditorEscapeKeyPressed(juce::TextEditor &) override { triggerMenuItem(); }
};

template <typename T>
    requires HasContinuous<T>
void SixSinesEditor::popupMenuForContinuous(T *e)
{
    auto data = e->continuous();
    if (!data)
    {
        return;
    }

    if (!e->isEnabled())
    {
        return;
    }

    auto p = juce::PopupMenu();
    p.addSectionHeader(data->getLabel());
    p.addSeparator();
    p.addCustomItem(-1,
                    std::make_unique<MenuValueTypein<T>>(*this, juce::Component::SafePointer(e)));
    p.addSeparator();
    p.addItem("Set to Default",
              [w = juce::Component::SafePointer(e)]()
              {
                  if (!w)
                      return;
                  w->onBeginEdit();
                  w->continuous()->setValueFromGUI(w->continuous()->getDefaultValue());
                  w->onEndEdit();
                  w->repaint();
              });

    auto pid = sst::jucegui::component_adapters::getClapParamId(e);
    if (pid.has_value())
    {
        sst::clap_juce_shim::populateMenuForClapParam(p, *pid, clapHost);
    }

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this));
}
} // namespace baconpaul::six_sines::ui

#endif
