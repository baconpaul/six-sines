/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#include "six-sines-editor.h"
#include "patch-data-bindings.h"
#include "main-panel.h"
#include "main-sub-panel.h"
#include "matrix-panel.h"
#include "matrix-sub-panel.h"
#include "self-sub-panel.h"
#include "mixer-panel.h"
#include "mixer-sub-panel.h"
#include "source-panel.h"
#include "source-sub-panel.h"
#include "ui-constants.h"
#include "juce-lnf.h"
#include "preset-manager.h"

namespace baconpaul::six_sines::ui
{
struct IdleTimer : juce::Timer
{
    SixSinesEditor &editor;
    IdleTimer(SixSinesEditor &e) : editor(e) {}
    void timerCallback() override { editor.idle(); }
};

static std::weak_ptr<SixSinesJuceLookAndFeel> sixSinesLookAndFeelWeakPointer;
static std::mutex sixSinesLookAndFeelSetupMutex;

SixSinesEditor::SixSinesEditor(Synth::audioToUIQueue_t &atou, Synth::uiToAudioQueue_T &utoa,
                               std::function<void()> fo)
    : audioToUI(atou), uiToAudio(utoa), flushOperator(fo)
{
    sst::jucegui::style::StyleSheet::initializeStyleSheets([]() {});

    setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
        sst::jucegui::style::StyleSheet::DARK));
    style()->setColour(jcmp::MenuButton::Styles::styleClass, jcmp::MenuButton::Styles::fill,
                       juce::Colour(0x15, 0x15, 0x15));
    style()->setFont(
        jcmp::MenuButton::Styles::styleClass, jcmp::MenuButton::Styles::labelfont,
        style()
            ->getFont(jcmp::MenuButton::Styles::styleClass, jcmp::MenuButton::Styles::labelfont)
            .withHeight(18));

    matrixPanel = std::make_unique<MatrixPanel>(*this);
    mixerPanel = std::make_unique<MixerPanel>(*this);
    singlePanel = std::make_unique<jcmp::NamedPanel>("Edit");
    sourcePanel = std::make_unique<SourcePanel>(*this);
    mainPanel = std::make_unique<MainPanel>(*this);
    addAndMakeVisible(*matrixPanel);
    addAndMakeVisible(*mixerPanel);
    addAndMakeVisible(*singlePanel);
    addAndMakeVisible(*sourcePanel);
    addAndMakeVisible(*mainPanel);

    mainSubPanel = std::make_unique<MainSubPanel>(*this);
    singlePanel->addChildComponent(*mainSubPanel);
    matrixSubPanel = std::make_unique<MatrixSubPanel>(*this);
    selfSubPanel = std::make_unique<SelfSubPanel>(*this);
    singlePanel->addChildComponent(*matrixSubPanel);
    singlePanel->addChildComponent(*selfSubPanel);
    mixerSubPanel = std::make_unique<MixerSubPanel>(*this);
    singlePanel->addChildComponent(*mixerSubPanel);
    sourceSubPanel = std::make_unique<SourceSubPanel>(*this);
    singlePanel->addChildComponent(*sourceSubPanel);

    auto startMsg = Synth::UIToAudioMsg{Synth::UIToAudioMsg::REQUEST_REFRESH};
    uiToAudio.push(startMsg);
    flushOperator();

    idleTimer = std::make_unique<IdleTimer>(*this);
    idleTimer->startTimer(1000. / 60.);

    toolTip = std::make_unique<jcmp::ToolTip>();
    addChildComponent(*toolTip);

    presetButton = std::make_unique<jcmp::MenuButton>();
    presetButton->setLabel("init");
    presetButton->setOnCallback([this]() { showPresetPopup(); });
    addAndMakeVisible(*presetButton);

    presetManager = std::make_unique<PresetManager>();

    {
        std::lock_guard<std::mutex> grd(sixSinesLookAndFeelSetupMutex);
        if (auto sp = sixSinesLookAndFeelWeakPointer.lock())
        {
            lnf = sp;
        }
        else
        {
            lnf = std::make_shared<SixSinesJuceLookAndFeel>(
                style()->getFont(jcmp::Label::Styles::styleClass, jcmp::Label::Styles::labelfont));
            sixSinesLookAndFeelWeakPointer = lnf;

            juce::LookAndFeel::setDefaultLookAndFeel(lnf.get());
        }
    }

    // Make sure to do this last
    setSize(608, 830);
}
SixSinesEditor::~SixSinesEditor() { idleTimer->stopTimer(); }

void SixSinesEditor::idle()
{
    auto aum = audioToUI.pop();
    while (aum.has_value())
    {
        if (aum->action == Synth::AudioToUIMsg::UPDATE_PARAM)
        {
            patchCopy.paramMap[aum->paramId]->value = aum->value;
            auto pit = componentByID.find(aum->paramId);
            if (pit != componentByID.end() && pit->second)
                pit->second->repaint();
        }
        else if (aum->action == Synth::AudioToUIMsg::UPDATE_VU)
        {
            mainPanel->vuMeter->setLevels(aum->value, aum->value2);
        }
        aum = audioToUI.pop();
    }
}

void SixSinesEditor::paint(juce::Graphics &g)
{
    jcmp::WindowPanel::paint(g);
    auto ft = style()->getFont(jcmp::Label::Styles::styleClass, jcmp::Label::Styles::labelfont);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    auto q = ft.withHeight(30);
    g.setFont(q);
    // g.drawText("Six", getLocalBounds().reduced(3,1), juce::Justification::topLeft);
    auto xp = 3;
    auto ht = 30;

    for (int fr = 1; fr < 6; ++fr)
    {
        juce::Path p;
        int np{80};
        for (int i = 0; i < np; ++i)
        {
            auto sx = sin(2.0 * M_PI * fr * i / np);
            if (i == 0)
                p.startNewSubPath(xp + i, 0.45 * (-sx + 1) * ht + 4);
            else
                p.lineTo(xp + i, 0.45 * (-sx + 1) * ht + 4);
        }
        g.setColour(juce::Colours::white.withAlpha(0.9f - sqrt((fr - 1) / 7.0f)));
        g.strokePath(p, juce::PathStrokeType(1));
    }

    xp = getWidth() - 83;
    for (int fr = 1; fr < 6; ++fr)
    {
        juce::Path p;
        int np{80};
        for (int i = 0; i < np; ++i)
        {
            auto sx = -sin(2.0 * M_PI * fr * i / np);
            if (i == 0)
                p.startNewSubPath(xp + i, 0.45 * (-sx + 1) * ht + 4);
            else
                p.lineTo(xp + i, 0.45 * (-sx + 1) * ht + 4);
        }
        g.setColour(juce::Colours::white.withAlpha(0.9f - sqrt((fr - 1) / 7.0f)));
        g.strokePath(p, juce::PathStrokeType(1));
    }

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    q = ft.withHeight(12);
    g.setFont(q);
    g.drawText("https://github.com/baconpaul/six-sines", getLocalBounds().reduced(3, 3),
               juce::Justification::bottomLeft);

    auto bi = std::string(__DATE__) + " " + std::string(__TIME__) + " " + BUILD_HASH;
    g.drawText(bi, getLocalBounds().reduced(3, 3), juce::Justification::bottomRight);
}

void SixSinesEditor::resized()
{
    auto rdx{1};

    int tpt{33}, tpb{15};

    auto but = getLocalBounds().withHeight(tpt).reduced(110, 0).withTrimmedTop(2);
    presetButton->setBounds(but);

    auto area = getLocalBounds().withTrimmedTop(tpt).withTrimmedBottom(tpb);

    auto tp = 100;

    auto rb = area.withTrimmedTop(tp);
    auto edH = 250 - tpt - tpb;

    auto mp = rb.withTrimmedBottom(edH);
    mp = mp.withWidth(numOps * (uicPowerKnobWidth + uicMargin) + 2 * uicMargin + 10);

    matrixPanel->setBounds(mp.reduced(rdx));

    auto sp = rb.withTrimmedBottom(mp.getHeight());
    sp = sp.translated(0, mp.getHeight());
    singlePanel->setBounds(sp.reduced(rdx));

    auto mx = rb.withTrimmedBottom(edH).withTrimmedLeft(mp.getWidth());
    mixerPanel->setBounds(mx.reduced(rdx));

    auto ta = area.withHeight(tp);
    auto sr = ta.withWidth(mp.getWidth());
    sourcePanel->setBounds(sr.reduced(rdx));

    auto mn = ta.withTrimmedLeft(sr.getWidth());
    mainPanel->setBounds(mn.reduced(rdx));

    mainSubPanel->setBounds(singlePanel->getContentArea());
    matrixSubPanel->setBounds(singlePanel->getContentArea());
    selfSubPanel->setBounds(singlePanel->getContentArea());
    mixerSubPanel->setBounds(singlePanel->getContentArea());
    sourceSubPanel->setBounds(singlePanel->getContentArea());
}

void SixSinesEditor::hideAllSubPanels()
{
    for (auto &c : singlePanel->getChildren())
    {
        c->setVisible(false);
    }
    mainPanel->clearHighlight();
    sourcePanel->clearHighlight();
    matrixPanel->clearHighlight();
    mixerPanel->clearHighlight();
}

void SixSinesEditor::showTooltipOn(juce::Component *c)
{
    int x = 0;
    int y = 0;
    juce::Component *component = c;
    while (component != this)
    {
        auto bounds = component->getBoundsInParent();
        x += bounds.getX();
        y += bounds.getY();

        component = component->getParentComponent();
    }
    y += c->getHeight();
    toolTip->resetSizeFromData();
    if (y + toolTip->getHeight() > getHeight())
    {
        y -= c->getHeight() - 3 - toolTip->getHeight();
    }

    toolTip->setTopLeftPosition(x, y);
    toolTip->setVisible(true);
}
void SixSinesEditor::updateTooltip(jdat::Continuous *c)
{
    toolTip->setTooltipTitleAndData(c->getLabel(), {c->getValueAsString()});
    toolTip->resetSizeFromData();
}
void SixSinesEditor::updateTooltip(jdat::Discrete *d)
{
    toolTip->setTooltipTitleAndData(d->getLabel(), {d->getValueAsString()});
    toolTip->resetSizeFromData();
}
void SixSinesEditor::hideTooltip() { toolTip->setVisible(false); }

struct MenuValueTypein : HasEditor, juce::PopupMenu::CustomComponent, juce::TextEditor::Listener
{
    std::unique_ptr<juce::TextEditor> textEditor;
    juce::Component::SafePointer<jcmp::ContinuousParamEditor> underComp;

    MenuValueTypein(SixSinesEditor &editor,
                    juce::Component::SafePointer<jcmp::ContinuousParamEditor> under)
        : HasEditor(editor), underComp(under)
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
                    auto valCol = juce::Colour(0xFF, 0x90, 0x00);
                    textEditor->setColour(juce::TextEditor::ColourIds::backgroundColourId,
                                          valCol.withAlpha(0.1f));
                    textEditor->setColour(juce::TextEditor::ColourIds::highlightColourId,
                                          valCol.withAlpha(0.15f));
                    textEditor->setJustification(juce::Justification::centredLeft);
                    textEditor->setColour(juce::TextEditor::ColourIds::outlineColourId,
                                          juce::Colours::black.withAlpha(0.f));
                    textEditor->setColour(juce::TextEditor::ColourIds::focusedOutlineColourId,
                                          juce::Colours::black.withAlpha(0.f));
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
            if (s.empty())
            {
                underComp->continuous()->setValueFromGUI(
                    underComp->continuous()->getDefaultValue());
            }
            else
            {
                underComp->continuous()->setValueAsString(s);
            }
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

void SixSinesEditor::popupMenuForContinuous(jcmp::ContinuousParamEditor *e)
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
    p.addCustomItem(-1, std::make_unique<MenuValueTypein>(*this, juce::Component::SafePointer(e)));
    p.addSeparator();
    p.addItem("Set to Default",
              [w = juce::Component::SafePointer(e)]()
              {
                  if (!w)
                      return;
                  w->continuous()->setValueFromGUI(w->continuous()->getDefaultValue());
              });

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this));
}

void SixSinesEditor::showPresetPopup()
{
    auto p = juce::PopupMenu();
    p.addSectionHeader("Presets - Coming Soon");
    p.addSeparator();
    p.addItem("Load Patch",
              [w = juce::Component::SafePointer(this)]()
              {
                  if (w)
                      w->doLoadPatch();
              });
    p.addItem("Save Patch",
              [w = juce::Component::SafePointer(this)]()
              {
                  if (w)
                      w->doSavePatch();
              });
    p.addItem("Reset",
              [w = juce::Component::SafePointer(this)]()
              {
                  if (w)
                  {
                      w->resetToDefault();
                  }
              });

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this));
}

void SixSinesEditor::doSavePatch()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Patch", juce::File(presetManager->userPatchesPath.u8string()), "*.sxsnp");
    fileChooser->launchAsync(
        juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [w = juce::Component::SafePointer(this)](const juce::FileChooser &c)
        {
            if (!w)
                return;
            auto result = c.getResults();
            if (result.isEmpty() || result.size() > 1)
            {
                return;
            }
            w->presetManager->saveUserPresetDirect(
                fs::path{result[0].getFullPathName().toStdString()}, w->patchCopy);
        });
}

void SixSinesEditor::doLoadPatch()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Patch", juce::File(presetManager->userPatchesPath.u8string()), "*.sxsnp");
    fileChooser->launchAsync(
        juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::openMode,
        [w = juce::Component::SafePointer(this)](const juce::FileChooser &c)
        {
            if (!w)
                return;
            auto result = c.getResults();
            if (result.isEmpty() || result.size() > 1)
            {
                return;
            }
            auto loadPath = fs::path{result[0].getFullPathName().toStdString()};
            w->presetManager->loadUserPresetDirect(loadPath, w->patchCopy);
            w->sendEntirePatchToAudio();
            w->repaint();
        });
}

void SixSinesEditor::resetToDefault()
{
    SXSNLOG("Resetting to default");
    for (auto [id, p] : patchCopy.paramMap)
    {
        p->value = p->meta.defaultVal;
    }
    sendEntirePatchToAudio();
    repaint();
}

void SixSinesEditor::sendEntirePatchToAudio()
{
    uiToAudio.push({Synth::UIToAudioMsg::STOP_AUDIO});
    for (const auto &p : patchCopy.params)
    {
        uiToAudio.push({Synth::UIToAudioMsg::BEGIN_EDIT, p->meta.id});
        uiToAudio.push({Synth::UIToAudioMsg::SET_PARAM, p->meta.id, p->value});
        uiToAudio.push({Synth::UIToAudioMsg::END_EDIT, p->meta.id});
    }
    uiToAudio.push({Synth::UIToAudioMsg::START_AUDIO});
}

} // namespace baconpaul::six_sines::ui