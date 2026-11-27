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

#include "tuning-screen.h"

#include <fstream>
#include <sstream>
#include "filesystem/import.h"
#include "synth/tuning.h"

namespace baconpaul::six_sines::ui
{
TuningScreen::TuningScreen(SixSinesEditor &e) : HasEditor(e)
{
    processKeys = true;

    panel = std::make_unique<jcmp::NamedPanel>("SCL/KBM Tuning");
    panel->isTabbed = true;
    panel->tabNames = {"SCL", "KBM"};
    panel->onTabSelected = [w = juce::Component::SafePointer(this)](int i)
    {
        if (w)
            w->setActiveTab(i);
    };
    addAndMakeVisible(*panel);

    sclEditor = makeTextEditor();
    panel->addAndMakeVisible(*sclEditor);
    kbmEditor = makeTextEditor();
    panel->addChildComponent(*kbmEditor);

    auto mkButton = [this](auto &slot, const char *label, auto fn)
    {
        slot = std::make_unique<jcmp::TextPushButton>();
        slot->setLabel(label);
        slot->setOnCallback(
            [w = juce::Component::SafePointer(this), fn]()
            {
                if (w)
                    fn(w.getComponent());
            });
        addAndMakeVisible(*slot);
    };
    mkButton(loadButton, "Load...", [](TuningScreen *t) { t->doLoad(); });
    mkButton(resetButton, "Reset", [](TuningScreen *t) { t->doReset(); });
    mkButton(applyButton, "Apply", [](TuningScreen *t) { t->doApply(); });
    mkButton(closeButton, "Close", [](TuningScreen *t) { t->setVisible(false); });

    panel->selectTab(0);
    panel->resetTabState();
    showSessionText();
}

TuningScreen::~TuningScreen() = default;

std::unique_ptr<juce::TextEditor> TuningScreen::makeTextEditor()
{
    using LC = SixSinesSkin::LogicalColor;
    auto &skin = editor.currentSkin;

    auto res = std::make_unique<juce::TextEditor>();
    res->setMultiLine(true);
    res->setReturnKeyStartsNewLine(true);
    res->setTabKeyUsedAsCharacter(true);
    res->setScrollbarsShown(true);
    if (auto tf = editor.typefaceFromResources("Anonymous_Pro/AnonymousPro-Regular.ttf"))
        res->setFont(juce::Font(juce::FontOptions{tf}.withHeight(14)));
    else
        res->setFont(juce::Font(
            juce::FontOptions{juce::Font::getDefaultMonospacedFontName(), 14, juce::Font::plain}));
    res->setColour(juce::TextEditor::backgroundColourId, skin.get(LC::Gutter));
    res->setColour(juce::TextEditor::textColourId, skin.get(LC::Label));
    res->setColour(juce::TextEditor::outlineColourId, skin.get(LC::Border));
    res->setColour(juce::TextEditor::focusedOutlineColourId, skin.get(LC::Value));
    res->setColour(juce::TextEditor::highlightColourId, skin.get(LC::Value).withAlpha(0.4f));
    res->setColour(juce::CaretComponent::caretColourId, skin.get(LC::Value));
    res->onEscapeKey = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setVisible(false);
    };
    return res;
}

void TuningScreen::setActiveTab(int t)
{
    activeTab = t;
    sclEditor->setVisible(t == 0);
    kbmEditor->setVisible(t == 1);
}

void TuningScreen::resized()
{
    ModalBase::resized();

    static constexpr int btnH{24}, btnW{90}, gap{6};
    auto ca = getContentArea().reduced(10);

    auto buttons = ca.removeFromBottom(btnH);
    ca.removeFromBottom(gap);

    panel->setBounds(ca);
    auto inner = panel->getContentArea().reduced(2);
    sclEditor->setBounds(inner);
    kbmEditor->setBounds(inner);

    loadButton->setBounds(buttons.removeFromLeft(btnW));
    closeButton->setBounds(buttons.removeFromRight(btnW));
    buttons.removeFromRight(gap);
    applyButton->setBounds(buttons.removeFromRight(btnW));
    buttons.removeFromRight(gap);
    resetButton->setBounds(buttons.removeFromRight(btnW));
}

void TuningScreen::doApply()
{
    auto scl = sclEditor->getText().toStdString();
    auto kbm = kbmEditor->getText().toStdString();

    // an untouched kbm seed is the identity mapping, so keep the session clean of it
    if (kbm == defaultKbmText())
        kbm.clear();
    editor.applySclKbmText(scl, kbm);
}

void TuningScreen::doReset()
{
    sclEditor->setText(twelveTETSclText(), juce::dontSendNotification);
    kbmEditor->setText(defaultKbmText(), juce::dontSendNotification);
    doApply();
}

void TuningScreen::showSessionText()
{
    // seed empty boxes with well formed no-ops so apply on an untouched box moves nothing
    const auto &m = editor.dawStateMainRef.main;
    sclEditor->setText(m.sclText.empty() ? twelveTETSclText() : m.sclText,
                       juce::dontSendNotification);
    kbmEditor->setText(m.kbmText.empty() ? defaultKbmText() : m.kbmText,
                       juce::dontSendNotification);
}

void TuningScreen::doLoad()
{
    auto isScl = activeTab == 0;
    fileChooser = std::make_unique<juce::FileChooser>(isScl ? "Load Scale (SCL)"
                                                            : "Load Keyboard Mapping (KBM)",
                                                      juce::File(), isScl ? "*.scl" : "*.kbm");
    fileChooser->launchAsync(
        juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::openMode,
        [w = juce::Component::SafePointer(this), isScl](const juce::FileChooser &c)
        {
            if (!w)
                return;
            auto result = c.getResults();
            if (result.size() != 1)
                return;
            auto path = fs::path{result[0].getFullPathName().toStdString()};

            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                w->editor.reportError("Tuning Error", "Unable to open " + path.u8string());
                return;
            }
            std::stringstream ss;
            ss << in.rdbuf();

            // load only fills the box; apply makes it live
            (isScl ? w->sclEditor : w->kbmEditor)->setText(ss.str(), juce::dontSendNotification);
        });
}

} // namespace baconpaul::six_sines::ui
