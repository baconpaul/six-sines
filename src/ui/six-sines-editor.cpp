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

#include "six-sines-editor.h"

#include "sst/plugininfra/version_information.h"
#include "sst/clap_juce_shim/menu_helper.h"

#include "finetune-sub-panel.h"
#include "patch-data-bindings.h"
#include "main-panel.h"
#include "main-sub-panel.h"
#include "matrix-panel.h"
#include "matrix-sub-panel.h"
#include "finetune-sub-panel.h"
#include "playmode-sub-panel.h"
#include "mainpan-sub-panel.h"
#include "self-sub-panel.h"
#include "mixer-panel.h"
#include "mixer-sub-panel.h"
#include "source-panel.h"
#include "source-sub-panel.h"
#include "ui-constants.h"
#include "juce-lnf.h"
#include "preset-manager.h"
#include "macro-panel.h"

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

namespace jstl = sst::jucegui::style;
using sheet_t = jstl::StyleSheet;
static constexpr sheet_t::Class PatchMenu("six-sines.patch-menu");

SixSinesEditor::SixSinesEditor(Synth::audioToUIQueue_t &atou, Synth::uiToAudioQueue_T &utoa,
                               std::function<void()> fo)
    : jcmp::WindowPanel(true), audioToUI(atou), uiToAudio(utoa), flushOperator(fo)
{
    setTitle("Six Sines - an Audio Rate Modulation Synthesizer");
    setAccessible(true);
    sst::jucegui::style::StyleSheet::initializeStyleSheets([]() {});

    sheet_t::addClass(PatchMenu).withBaseClass(jcmp::JogUpDownButton::Styles::styleClass);

    setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
        sst::jucegui::style::StyleSheet::DARK));

    style()->setFont(
        PatchMenu, jcmp::MenuButton::Styles::labelfont,
        style()
            ->getFont(jcmp::MenuButton::Styles::styleClass, jcmp::MenuButton::Styles::labelfont)
            .withHeight(18));

    matrixPanel = std::make_unique<MatrixPanel>(*this);
    mixerPanel = std::make_unique<MixerPanel>(*this);
    macroPanel = std::make_unique<MacroPanel>(*this);
    singlePanel = std::make_unique<jcmp::NamedPanel>("Edit");
    sourcePanel = std::make_unique<SourcePanel>(*this);
    mainPanel = std::make_unique<MainPanel>(*this);
    addAndMakeVisible(*matrixPanel);
    addAndMakeVisible(*mixerPanel);
    addAndMakeVisible(*singlePanel);
    addAndMakeVisible(*sourcePanel);
    addAndMakeVisible(*mainPanel);
    addAndMakeVisible(*macroPanel);

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
    fineTuneSubPanel = std::make_unique<FineTuneSubPanel>(*this);
    singlePanel->addChildComponent(*fineTuneSubPanel);
    mainPanSubPanel = std::make_unique<MainPanSubPanel>(*this);
    singlePanel->addChildComponent(*mainPanSubPanel);
    playModeSubPanel = std::make_unique<PlayModeSubPanel>(*this);
    singlePanel->addChildComponent(*playModeSubPanel);

    sst::jucegui::component_adapters::setTraversalId(sourcePanel.get(), 20000);
    sst::jucegui::component_adapters::setTraversalId(mainPanel.get(), 30000);
    sst::jucegui::component_adapters::setTraversalId(matrixPanel.get(), 40000);
    sst::jucegui::component_adapters::setTraversalId(mixerPanel.get(), 50000);
    sst::jucegui::component_adapters::setTraversalId(macroPanel.get(), 60000);
    sst::jucegui::component_adapters::setTraversalId(singlePanel.get(), 70000);

    auto startMsg = Synth::UIToAudioMsg{Synth::UIToAudioMsg::REQUEST_REFRESH};
    uiToAudio.push(startMsg);
    flushOperator();

    idleTimer = std::make_unique<IdleTimer>(*this);
    idleTimer->startTimer(1000. / 60.);

    toolTip = std::make_unique<jcmp::ToolTip>();
    addChildComponent(*toolTip);

    presetManager = std::make_unique<PresetManager>(patchCopy);
    presetManager->onPresetLoaded = [this](auto &s) { this->postPatchChange(s); };

    presetButton = std::make_unique<jcmp::JogUpDownButton>();
    presetButton->setCustomClass(PatchMenu);
    presetButton->setSource(presetManager->getDiscreteData());
    presetButton->onPopupMenu = [this]() { showPresetPopup(); };
    addAndMakeVisible(*presetButton);
    setPatchNameDisplay();
    sst::jucegui::component_adapters::setTraversalId(presetButton.get(), 174);

    defaultsProvider = std::make_unique<defaultsProvder_t>(
        presetManager->userPath, "SixSinesUI", defaultName,
        [](auto e, auto b) { SXSNLOG("[ERROR]" << e << " " << b); });
    SXSNLOG("Preset namager user path is " << presetManager->userPath.u8string())
    setSkinFromDefaults();

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

    vuMeter = std::make_unique<jcmp::VUMeter>(jcmp::VUMeter::HORIZONTAL);
    addAndMakeVisible(*vuMeter);

    // Make sure to do this last
    setSize(edWidth, edHeight);

    uiToAudio.push({Synth::UIToAudioMsg::EDITOR_ATTACH_DETATCH, true});
    uiToAudio.push({Synth::UIToAudioMsg::REQUEST_REFRESH, true});
    if (flushOperator)
        flushOperator();

    auto pzf = defaultsProvider->getUserDefaultValue(Defaults::zoomLevel, 100);
    zoomFactor = pzf * 0.01;
#define DEBUG_FOCUS 0
#if DEBUG_FOCUS
    focusDebugger = std::make_unique<sst::jucegui::accessibility::FocusDebugger>(*this);
    focusDebugger->setDoFocusDebug(true);
#endif
}
SixSinesEditor::~SixSinesEditor()
{
    uiToAudio.push({Synth::UIToAudioMsg::EDITOR_ATTACH_DETATCH, false});
    idleTimer->stopTimer();
}

void SixSinesEditor::idle()
{
    auto aum = audioToUI.pop();
    while (aum.has_value())
    {
        if (aum->action == Synth::AudioToUIMsg::UPDATE_PARAM)
        {
            setParamValueOnCopy(aum->paramId, aum->value, false);
        }
        else if (aum->action == Synth::AudioToUIMsg::UPDATE_VU)
        {
            vuMeter->setLevels(aum->value, aum->value2);
        }
        else if (aum->action == Synth::AudioToUIMsg::UPDATE_VOICE_COUNT)
        {
            mainPanel->setVoiceCount(aum->paramId);
            mainPanel->repaint();
        }
        else if (aum->action == Synth::AudioToUIMsg::SET_PATCH_NAME)
        {
            memset(patchCopy.name, 0, sizeof(patchCopy.name));
            strncpy(patchCopy.name, aum->hackPointer, 255);
            setPatchNameDisplay();
        }
        else if (aum->action == Synth::AudioToUIMsg::SET_PATCH_DIRTY_STATE)
        {
            patchCopy.dirty = (bool)aum->paramId;
            presetManager->setDirtyState(patchCopy.dirty);
            presetButton->repaint();
        }
        else
        {
            SXSNLOG("Ignored patch message " << aum->action);
        }
        aum = audioToUI.pop();
    }
}

void SixSinesEditor::paint(juce::Graphics &g)
{
    jcmp::WindowPanel::paint(g);
    auto ft = style()->getFont(jcmp::Label::Styles::styleClass, jcmp::Label::Styles::labelfont);

    bool isLight = defaultsProvider->getUserDefaultValue(Defaults::useLightSkin, false);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    auto q = ft.withHeight(30);
    g.setFont(q);
    // g.drawText("Six", getLocalBounds().reduced(3,1), juce::Justification::topLeft);
    auto xp = 3;
    auto ht = 30;

    int np{110};

    for (int fr = 1; fr < 6; ++fr)
    {
        juce::Path p;
        for (int i = 0; i < np; ++i)
        {
            auto sx = sin(2.0 * M_PI * fr * i / np);
            if (i == 0)
                p.startNewSubPath(xp + i, 0.45 * (-sx + 1) * ht + 4);
            else
                p.lineTo(xp + i, 0.45 * (-sx + 1) * ht + 4);
        }
        if (isLight)
            g.setColour(juce::Colours::navy.withAlpha(0.9f - sqrt((fr - 1) / 7.0f)));
        else
            g.setColour(juce::Colours::white.withAlpha(0.9f - sqrt((fr - 1) / 7.0f)));
        g.strokePath(p, juce::PathStrokeType(1));
    }

    if (isLight)
        g.setColour(juce::Colours::navy);
    else
        g.setColour(juce::Colours::white.withAlpha(0.5f));
    q = ft.withHeight(12);
    g.setFont(q);
    g.drawText("https://github.com/baconpaul/six-sines", getLocalBounds().reduced(3, 3),
               juce::Justification::bottomLeft);

    std::string os = "";
#if JUCE_MAC
    os = "macOS";
#endif
#if JUCE_WINDOWS
    os = "Windows";
#endif
#if JUCE_LINUX
    os = "Linux";
#endif

    auto bi = os + " " + sst::plugininfra::VersionInformation::git_commit_hash;
    g.drawText(bi, getLocalBounds().reduced(3, 3), juce::Justification::bottomRight);

    g.drawText(sst::plugininfra::VersionInformation::git_implied_display_version,
               getLocalBounds().reduced(3, 3), juce::Justification::centredBottom);

#if !defined(NDEBUG) || !NDEBUG
    g.setColour(juce::Colours::red);
    g.setFont(juce::FontOptions(30));
    auto dr = juce::Rectangle<int>(0, 0, np, ht);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(dr);

    g.setColour(juce::Colours::white);
    g.drawRect(dr, 1);
    g.drawText("DEBUG", dr.translated(1, 1), juce::Justification::centred);
    g.setColour(juce::Colours::red);
    g.drawText("DEBUG", dr, juce::Justification::centred);
#endif
}

void SixSinesEditor::resized()
{
    int presetHeight{33}, footerHeight{15};

    auto lb = getLocalBounds();
    auto presetArea = lb.withHeight(presetHeight);
    auto panelArea = lb.withTrimmedTop(presetHeight).withTrimmedBottom(footerHeight);

    auto panelPadX{2 * uicMargin + 11}, panelPadY{34U};
    auto sourceHeight = uicLabeledKnobHeight + panelPadY;

    auto matrixWidth = numOps * (uicPowerKnobWidth) + (numOps - 1) * uicMargin + panelPadX;
    auto matrixHeight = numOps * uicLabeledKnobHeight + (numOps - 1) * uicMargin + panelPadY;
    auto mixerWidth = uicKnobSize + uicPowerKnobWidth + uicMargin + panelPadX;
    auto macroWidth = uicKnobSize + panelPadX + 6;
    auto mainWidth = mixerWidth + macroWidth;

    auto editHeight = 220;

    auto panelMargin{1};

    // Preset button
    auto but = presetArea.reduced(110, 0).withTrimmedTop(uicMargin);
    presetButton->setBounds(but);
    but = but.withLeft(presetButton->getRight() + uicMargin).withRight(getWidth() - uicMargin);

    vuMeter->setBounds(but);

    auto sourceRect =
        juce::Rectangle<int>(panelArea.getX(), panelArea.getY(), matrixWidth, sourceHeight);
    auto matrixRect = juce::Rectangle<int>(panelArea.getX(), panelArea.getY() + sourceHeight,
                                           matrixWidth, matrixHeight);
    auto mainRect = juce::Rectangle<int>(panelArea.getX() + matrixWidth, panelArea.getY(),
                                         mainWidth, sourceHeight);
    auto mixerRect = juce::Rectangle<int>(
        panelArea.getX() + matrixWidth, panelArea.getY() + sourceHeight, mixerWidth, matrixHeight);
    auto macroRect =
        juce::Rectangle<int>(panelArea.getX() + matrixWidth + mixerWidth,
                             panelArea.getY() + sourceHeight, macroWidth, matrixHeight);
    auto editRect =
        juce::Rectangle<int>(panelArea.getX(), panelArea.getY() + sourceHeight + matrixHeight,
                             matrixWidth + mainWidth, editHeight);

    sourcePanel->setBounds(sourceRect.reduced(panelMargin));
    matrixPanel->setBounds(matrixRect.reduced(panelMargin));
    mainPanel->setBounds(mainRect.reduced(panelMargin));
    mixerPanel->setBounds(mixerRect.reduced(panelMargin));
    macroPanel->setBounds(macroRect.reduced(panelMargin));
    singlePanel->setBounds(editRect.reduced(panelMargin));

    mainSubPanel->setBounds(singlePanel->getContentArea());
    matrixSubPanel->setBounds(singlePanel->getContentArea());
    selfSubPanel->setBounds(singlePanel->getContentArea());
    mixerSubPanel->setBounds(singlePanel->getContentArea());
    sourceSubPanel->setBounds(singlePanel->getContentArea());
    mainPanSubPanel->setBounds(singlePanel->getContentArea());
    fineTuneSubPanel->setBounds(singlePanel->getContentArea());
    playModeSubPanel->setBounds(singlePanel->getContentArea());
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
    if (y + toolTip->getHeight() > getHeight() - 40)
    {
        y -= c->getHeight() + 3 + toolTip->getHeight();
    }

    if (x + toolTip->getWidth() > getWidth())
    {
        x -= toolTip->getWidth();
        x += c->getWidth() - 3;
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
                  w->repaint();
              });

    // I could also stick the param id onto the component properties I guess
    auto pid = sst::jucegui::component_adapters::getClapParamId(e);
    if (pid.has_value())
    {
        sst::clap_juce_shim::populateMenuForClapParam(p, *pid, clapHost);
    }

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this));
}

void SixSinesEditor::showPresetPopup()
{
    auto p = juce::PopupMenu();
    p.addSectionHeader("Main Menu");

    auto f = juce::PopupMenu();
    for (auto &[c, ent] : presetManager->factoryPatchNames)
    {
        auto em = juce::PopupMenu();
        for (auto &e : ent)
        {
            auto noExt = e;
            auto ps = noExt.find(".sxsnp");
            if (ps != std::string::npos)
            {
                noExt = noExt.substr(0, ps);
            }
            em.addItem(noExt, [cat = c, pat = e, this]()
                       { this->presetManager->loadFactoryPreset(cat, pat); });
        }
        f.addSubMenu(c, em);
    }

    auto u = juce::PopupMenu();
    auto cat = fs::path();
    auto s = juce::PopupMenu();
    for (const auto &up : presetManager->userPatches)
    {
        auto pp = up.parent_path();
        auto dn = up.filename().replace_extension("").u8string();
        if (pp.empty())
        {
            u.addItem(dn,
                      [this, pth = up, dn]() {
                          presetManager->loadUserPresetDirect(presetManager->userPatchesPath / pth);
                      });
        }
        else
        {
            if (pp != cat)
            {
                if (cat.empty())
                {
                    u.addSeparator();
                }
                if (s.getNumItems() > 0)
                {
                    u.addSubMenu(cat.u8string(), s);
                    s = juce::PopupMenu();
                }
                cat = pp;
            }
            s.addItem(dn,
                      [this, pth = up, dn]() {
                          presetManager->loadUserPresetDirect(presetManager->userPatchesPath / pth);
                      });
        }
    }
    if (s.getNumItems() > 0 && !cat.empty())
    {
        u.addSubMenu(cat.u8string(), s);
    }
    p.addSeparator();
    p.addSubMenu("Factory Presets", f);
    p.addSubMenu("User Presets", u);
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
    p.addSeparator();
    p.addItem("Reset to Init",
              [w = juce::Component::SafePointer(this)]()
              {
                  if (w)
                  {
                      w->resetToDefault();
                  }
              });
    p.addSeparator();

    auto uim = juce::PopupMenu();
    auto isLight = defaultsProvider->getUserDefaultValue(Defaults::useLightSkin, 0);

    for (auto scale : {75, 90, 100, 110, 125, 150})
    {
        uim.addItem("Zoom " + std::to_string(scale) + "%", true,
                    std::fabs(zoomFactor * 100 - scale) < 2,
                    [s = scale, w = juce::Component::SafePointer(this)]()
                    {
                        if (!w)
                            return;
                        w->setZoomFactor(s * 0.01);
                    });
    }

    uim.addSeparator();
    uim.addItem("Dark Mode", true, !isLight,
                [w = juce::Component::SafePointer(this)]()
                {
                    if (!w)
                        return;
                    w->defaultsProvider->updateUserDefaultValue(Defaults::useLightSkin, false);
                    w->setSkinFromDefaults();
                });

    uim.addItem("Light Mode", true, isLight,
                [w = juce::Component::SafePointer(this)]()
                {
                    if (!w)
                        return;
                    w->defaultsProvider->updateUserDefaultValue(Defaults::useLightSkin, true);
                    w->setSkinFromDefaults();
                });
    p.addSubMenu("User Interface", uim);

    p.addSeparator();
    p.addItem("Read the Manual",
              []()
              {
                  juce::URL("https://github.com/baconpaul/six-sines/blob/main/doc/manual.md")
                      .launchInDefaultBrowser();
              });
    p.addItem("Get the Source", []()
              { juce::URL("https://github.com/baconpaul/six-sines/").launchInDefaultBrowser(); });
    p.addItem("Acknowledgements",
              []()
              {
                  juce::URL("https://github.com/baconpaul/six-sines/blob/main/doc/ack.md")
                      .launchInDefaultBrowser();
              });
    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this));
}

void SixSinesEditor::doSavePatch()
{
    auto svP = presetManager->userPatchesPath;
    if (strcmp(patchCopy.name, "Init") != 0)
    {
        svP = (svP / patchCopy.name).replace_extension(".sxsnp");
    }
    fileChooser =
        std::make_unique<juce::FileChooser>("Save Patch", juce::File(svP.u8string()), "*.sxsnp");
    fileChooser->launchAsync(juce::FileBrowserComponent::canSelectFiles |
                                 juce::FileBrowserComponent::saveMode |
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
                                 auto pn = fs::path{result[0].getFullPathName().toStdString()};
                                 w->setPatchNameTo(pn.filename().replace_extension("").u8string());

                                 w->presetManager->saveUserPresetDirect(pn);
                             });
}

void SixSinesEditor::setPatchNameTo(const std::string &s)
{
    memset(patchCopy.name, 0, sizeof(patchCopy.name));
    strncpy(patchCopy.name, s.c_str(), 255);
    uiToAudio.push({Synth::UIToAudioMsg::SEND_PATCH_NAME, 0, 0, patchCopy.name});
    setPatchNameDisplay();
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
            w->presetManager->loadUserPresetDirect(loadPath);
        });
}

void SixSinesEditor::resetToDefault() { presetManager->loadInit(); }

void SixSinesEditor::sendEntirePatchToAudio(const std::string &s)
{
    static char tmpDat[256];
    memset(tmpDat, 0, sizeof(tmpDat));
    strncpy(tmpDat, s.c_str(), 255);
    uiToAudio.push({Synth::UIToAudioMsg::SEND_PATCH_NAME, 0, 0.f, tmpDat});
    uiToAudio.push({Synth::UIToAudioMsg::STOP_AUDIO});
    for (const auto &p : patchCopy.params)
    {
        uiToAudio.push({Synth::UIToAudioMsg::BEGIN_EDIT, p->meta.id});
        uiToAudio.push({Synth::UIToAudioMsg::SET_PARAM, p->meta.id, p->value});
        uiToAudio.push({Synth::UIToAudioMsg::END_EDIT, p->meta.id});
    }
    uiToAudio.push({Synth::UIToAudioMsg::START_AUDIO});
    uiToAudio.push({Synth::UIToAudioMsg::SEND_PATCH_IS_CLEAN, true});
}

void SixSinesEditor::sendParamSetValue(uint32_t id, float value)
{
    patchCopy.paramMap.at(id)->value = value;
    uiToAudio.push({Synth::UIToAudioMsg::Action::SET_PARAM, id, value});
    flushOperator();
}

void SixSinesEditor::setParamValueOnCopy(uint32_t paramId, float value, bool notifyAudio)
{
    patchCopy.paramMap[paramId]->value = value;

    auto rit = componentRefreshByID.find(paramId);
    if (rit != componentRefreshByID.end())
    {
        rit->second();
    }

    auto pit = componentByID.find(paramId);
    if (pit != componentByID.end() && pit->second)
        pit->second->repaint();

    if (notifyAudio)
    {
        uiToAudio.push({Synth::UIToAudioMsg::SET_PARAM, paramId, value});
    }
}

void SixSinesEditor::setPatchNameDisplay()
{
    if (!presetButton)
        return;
    presetManager->setStateForDisplayName(patchCopy.name);
    presetButton->repaint();
}

void SixSinesEditor::postPatchChange(const std::string &dn)
{
    sendEntirePatchToAudio(dn);
    for (auto [id, f] : componentRefreshByID)
        f();

    repaint();
}

void SixSinesEditor::showNavigationMenu()
{
    auto p = juce::PopupMenu();
    p.addSectionHeader("Navigation");
    p.addSeparator();
    p.addItem("Preset Browser",
              [w = juce::Component::SafePointer(this)]()
              {
                  if (w)
                  {
                      w->presetButton->grabKeyboardFocus();
                  }
              });
    p.addSeparator();

    auto src = juce::PopupMenu();
    for (int i = 0; i < numOps; ++i)
    {
        src.addItem(sourcePanel->knobs[i]->getTitle(),
                    [i, w = juce::Component::SafePointer(this)]()
                    {
                        if (w)
                        {
                            w->sourcePanel->beginEdit(i);
                            w->sourcePanel->knobs[i]->grabKeyboardFocus();
                        }
                    });
    }
    p.addSubMenu("Sources", src);

    auto fb = juce::PopupMenu();
    for (int i = 0; i < numOps; ++i)
    {
        fb.addItem(matrixPanel->Sknobs[i]->getTitle(),
                   [i, w = juce::Component::SafePointer(this)]()
                   {
                       if (w)
                       {
                           w->matrixPanel->beginEdit(i, true);
                           w->matrixPanel->Sknobs[i]->grabKeyboardFocus();
                       }
                   });
    }
    p.addSubMenu("Matrix Feedback", fb);

    auto md = juce::PopupMenu();
    for (int i = 0; i < matrixSize; ++i)
    {
        md.addItem(matrixPanel->Mknobs[i]->getTitle(),
                   [i, w = juce::Component::SafePointer(this)]()
                   {
                       if (w)
                       {
                           w->matrixPanel->beginEdit(i, false);
                           w->matrixPanel->Mknobs[i]->grabKeyboardFocus();
                       }
                   });
    }
    p.addSubMenu("Matrix Modulation", md);

    auto mx = juce::PopupMenu();
    for (int i = 0; i < numOps; ++i)
    {
        mx.addItem(mixerPanel->knobs[i]->getTitle(),
                   [i, w = juce::Component::SafePointer(this)]()
                   {
                       if (w)
                       {
                           w->mixerPanel->beginEdit(i);
                           w->mixerPanel->knobs[i]->grabKeyboardFocus();
                       }
                   });
    }
    p.addSubMenu("Mixer", mx);

    auto mc = juce::PopupMenu();
    for (int i = 0; i < numOps; ++i)
    {
        mc.addItem(macroPanel->knobs[i]->getTitle(),
                   [i, w = juce::Component::SafePointer(this)]()
                   {
                       if (w)
                       {
                           w->macroPanel->knobs[i]->grabKeyboardFocus();
                       }
                   });
    }
    p.addSubMenu("Macros", mc);

    auto mn = juce::PopupMenu();
    int idx{0};
    for (const auto &l : {&mainPanel->lev, &mainPanel->pan, &mainPanel->tun})
    {
        mn.addItem((*l)->getTitle(),
                   [idx, x = (*l).get(), w = juce::Component::SafePointer(this)]()
                   {
                       if (w)
                       {
                           w->mainPanel->beginEdit(idx);
                           x->grabKeyboardFocus();
                       }
                   });
        idx++;
    }
    p.addSubMenu("Main", mn);

    p.addSeparator();

    auto enam = std::string("Edit Area");
    if (!singlePanel->getName().isEmpty())
        enam = "Edit " + singlePanel->getName().toStdString();
    p.addItem(enam,
              [w = juce::Component::SafePointer(this)]()
              {
                  if (w)
                  {
                      w->singlePanel->grabKeyboardFocus();
                  }
              });

    p.addItem("Settings",
              [w = juce::Component::SafePointer(this)]()
              {
                  if (w)
                  {
                      w->mainPanel->beginEdit(3);
                      w->singlePanel->grabKeyboardFocus();
                  }
              });

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this));
}

bool SixSinesEditor::keyPressed(const juce::KeyPress &key)
{
    if (key.getModifiers().isCommandDown() && (char)key.getKeyCode() == 'N')
    {
        showNavigationMenu();
        return true;
    }

    if (key.getModifiers().isCommandDown() && (char)key.getKeyCode() == 'A')
    {
        auto c = getCurrentlyFocusedComponent();
        auto psgf = panelSelectGestureFor.find(c);

        if (psgf != panelSelectGestureFor.end())
            psgf->second();
        return true;
    }

    if (key.getModifiers().isCommandDown() && (char)key.getKeyCode() == 'J')
    {
        singlePanel->grabKeyboardFocus();
        return true;
    }

    return false;
}

void SixSinesEditor::visibilityChanged()
{
    if (isVisible() && isShowing())
    {
        presetButton->setWantsKeyboardFocus(true);
        presetButton->grabKeyboardFocus();
    }
}

void SixSinesEditor::parentHierarchyChanged()
{
    if (isVisible() && isShowing())
    {
        presetButton->setWantsKeyboardFocus(true);
        presetButton->grabKeyboardFocus();
    }
}

void SixSinesEditor::setSkinFromDefaults()
{
    auto b = defaultsProvider->getUserDefaultValue(Defaults::useLightSkin, 0);
    if (b)
    {
        setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
            sst::jucegui::style::StyleSheet::LIGHT));
        style()->setColour(PatchMenu, jcmp::MenuButton::Styles::fill,
                           style()
                               ->getColour(jcmp::base_styles::Base::styleClass,
                                           jcmp::base_styles::Base::background)
                               .darker(0.3f));
    }
    else
    {
        setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
            sst::jucegui::style::StyleSheet::DARK));
    }

    style()->setFont(
        PatchMenu, jcmp::MenuButton::Styles::labelfont,
        style()
            ->getFont(jcmp::MenuButton::Styles::styleClass, jcmp::MenuButton::Styles::labelfont)
            .withHeight(18));
}

void SixSinesEditor::setZoomFactor(float zf)
{
    // SCLOG("Setting zoom factor to " << zf);
    zoomFactor = zf;
    setTransform(juce::AffineTransform().scaled(zoomFactor));
    setBounds(0, 0, edWidth * zf, edHeight * zf);
    defaultsProvider->updateUserDefaultValue(Defaults::zoomLevel, zoomFactor * 100);
    if (onZoomChanged)
        onZoomChanged(zoomFactor);
}

} // namespace baconpaul::six_sines::ui