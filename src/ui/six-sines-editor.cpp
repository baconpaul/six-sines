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

#include "dahdsr-components.h"
#include "sst/plugininfra/version_information.h"
#include "sst/clap_juce_shim/menu_helper.h"
#include "sst/plugininfra/misc_platform.h"

#include "finetune-sub-panel.h"
#include "patch-data-bindings.h"
#include "main-panel.h"
#include "main-sub-panel.h"
#include "matrix-panel.h"
#include "matrix-sub-panel.h"
#include "finetune-sub-panel.h"
#include "playmode-sub-panel.h"
#include "settings-panel.h"
#include "mainpan-sub-panel.h"
#include "self-sub-panel.h"
#include "mixer-panel.h"
#include "mixer-sub-panel.h"
#include "source-panel.h"
#include "source-sub-panel.h"
#include "ui-constants.h"
#include "presets/preset-manager.h"
#include "macro-panel.h"
#include "clipboard.h"
#include "patch-data-bindings.h"
#include "preset-data-binding.h"

namespace baconpaul::six_sines::ui
{
struct IdleTimer : juce::Timer
{
    SixSinesEditor &editor;
    IdleTimer(SixSinesEditor &e) : editor(e) {}
    void timerCallback() override { editor.idle(); }
};

struct DesPushTimer : juce::Timer
{
    SixSinesEditor &editor;
    DesPushTimer(SixSinesEditor &e) : editor(e) {}
    void timerCallback() override
    {
        stopTimer();
        editor.pushDawExtraStateToAudio();
    }
};

namespace jstl = sst::jucegui::style;
using sheet_t = jstl::StyleSheet;
static constexpr sheet_t::Class PatchMenu("six-sines.patch-menu");

SixSinesEditor::SixSinesEditor(Synth::audioToUIQueue_t &atou, Synth::mainToAudioQueue_T &utoa,
                               const clap_host_t *h)
    : jcmp::WindowPanel(true), audioToUI(atou), mainToAudio(utoa), clapHost(h)
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

    clipboard = std::make_unique<Clipboard>();
    matrixPanel = std::make_unique<MatrixPanel>(*this);
    mixerPanel = std::make_unique<MixerPanel>(*this);
    macroPanel = std::make_unique<MacroPanel>(*this);
    singlePanel = std::make_unique<jcmp::NamedPanel>("Edit");
    singlePanel->hasHamburger = false;
    singlePanel->onHamburger = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->doSinglePanelHamburger();
    };
    sourcePanel = std::make_unique<SourcePanel>(*this);
    mainPanel = std::make_unique<MainPanel>(*this);
    addAndMakeVisible(*matrixPanel);
    addAndMakeVisible(*mixerPanel);
    addAndMakeVisible(*singlePanel);
    addAndMakeVisible(*sourcePanel);
    addAndMakeVisible(*mainPanel);
    addAndMakeVisible(*macroPanel);
    settingsPanel = std::make_unique<SettingsPanel>(*this);
    addAndMakeVisible(*settingsPanel);

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

    auto startMsg = Synth::MainToAudioMsg{Synth::MainToAudioMsg::REQUEST_REFRESH};
    mainToAudio.push(startMsg);
    requestParamsFlush();

    idleTimer = std::make_unique<IdleTimer>(*this);
    idleTimer->startTimer(1000. / 60.);

    dawExtraStatePushTimer = std::make_unique<DesPushTimer>(*this);

    toolTip = std::make_unique<jcmp::ToolTip>();
    addChildComponent(*toolTip);

    presetManager = std::make_unique<presets::PresetManager>(clapHost);
    uiThemeManager = std::make_unique<presets::UIThemeManager>();
    presetManager->onPresetLoaded = [this](auto s)
    {
        this->postPatchChange(s);
        repaint();
    };

    presetDataBinding = std::make_unique<PresetDataBinding>(*presetManager, patchCopy, mainToAudio);
    presetDataBinding->setStateForDisplayName(patchCopy.name);

    presetButton = std::make_unique<jcmp::JogUpDownButton>();
    presetButton->setCustomClass(PatchMenu);
    presetButton->setSource(presetDataBinding.get());
    presetButton->onPopupMenu = [this]() { showPresetPopup(); };
    addAndMakeVisible(*presetButton);
    setPatchNameDisplay();
    sst::jucegui::component_adapters::setTraversalId(presetButton.get(), 174);

    defaultsProvider = std::make_unique<defaultsProvder_t>(
        presetManager->userPath, "SixSinesUI", defaultName,
        [](auto e, auto b) { SXSNLOG("[ERROR]" << e << " " << b); });
    // initializeBaseSkin() calls applyToStylesheet and guards lnf->setStyle with
    // "if (lnf)" — lnf is null at this point so that guard is a no-op.  lnf is
    // created immediately after and explicitly calls setStyle, which is correct.
    // Do NOT move lnf construction before initializeBaseSkin(): LookAndFeelManager
    // attaches to 'this' via JUCE's LNF mechanism and must see the fully-built
    // component tree (all child panels are added above this line).
    initializeBaseSkin();

    lnf = std::make_unique<sst::jucegui::style::LookAndFeelManager>(this);
    lnf->setStyle(style());

    // Override the dark-default startup skin with the user's saved theme preference.
    setThemeFromPreference();

    vuMeter = std::make_unique<jcmp::VUMeter>(jcmp::VUMeter::HORIZONTAL);
    addAndMakeVisible(*vuMeter);

    if (defaultsProvider->getUserDefaultValue(Defaults::designModeRunAllNodes, false))
        sessionRunAllNodes = true;
    if (defaultsProvider->getUserDefaultValue(Defaults::designModeAllSoundsOffOnToggle, false))
        sessionAllSoundsOffOnToggle = true;

    mainToAudio.push({Synth::MainToAudioMsg::EDITOR_ATTACH_DETATCH, true});
    sendDesignModeToAudio();
    mainToAudio.push({Synth::MainToAudioMsg::REQUEST_REFRESH, true});
    requestParamsFlush();

    auto defaultZoomPct = 100;
    if (auto *d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        auto sz = d->totalArea;
        if (sz.getWidth() <= 1366 && sz.getHeight() <= 768)
            defaultZoomPct = 90;
    }
    auto pzf = defaultsProvider->getUserDefaultValue(Defaults::zoomLevel, defaultZoomPct);
    zoomFactor = pzf * 0.01;
    setTransform(juce::AffineTransform().scaled(zoomFactor));

    setSize(edWidth, edHeight);
#define DEBUG_FOCUS 0
#if DEBUG_FOCUS
    focusDebugger = std::make_unique<sst::jucegui::accessibility::FocusDebugger>(*this);
    focusDebugger->setDoFocusDebug(true);
#endif
}
SixSinesEditor::~SixSinesEditor()
{
    sessionRunAllNodes = false;
    mainToAudio.push({Synth::MainToAudioMsg::SET_DESIGN_MODE_RUN_ALL, 0, 0.f});
    mainToAudio.push({Synth::MainToAudioMsg::EDITOR_ATTACH_DETATCH, false});
    idleTimer->stopTimer();
    setLookAndFeel(nullptr);
}

void SixSinesEditor::idle()
{
    if (getHeight() / getWidth() > edHeight / edWidth * 3)
    {
        SXSNLOG("Seems I have hit that renoise too-much-height bug");
        setZoomFactor(zoomFactor);
    }
    auto aum = audioToUI.pop();
    while (aum.has_value())
    {
        if (aum->action == Synth::AudioToUIMsg::UPDATE_PARAM)
        {
            setAndSendParamValue(aum->paramId, aum->value, false);
        }
        else if (aum->action == Synth::AudioToUIMsg::UPDATE_VU)
        {
            assert(aum->paramId < numOps + 1);
            if (aum->paramId == 0)
                vuMeter->setLevels(aum->value, aum->value2);
            else
            {
                mixerPanel->vuMeters[aum->paramId - 1]->setLevels(aum->value, aum->value2);
            }
        }
        else if (aum->action == Synth::AudioToUIMsg::UPDATE_VOICE_COUNT)
        {
            settingsPanel->setVoiceCount(aum->paramId);
            settingsPanel->repaint();
        }
        else if (aum->action == Synth::AudioToUIMsg::SET_PATCH_NAME)
        {
            memset(patchCopy.name, 0, sizeof(patchCopy.name));
            strncpy(patchCopy.name, aum->patchNamePointer, 255);
            setPatchNameDisplay();
        }
        else if (aum->action == Synth::AudioToUIMsg::SET_PATCH_DIRTY_STATE)
        {
            patchCopy.dirty = (bool)aum->paramId;
            presetDataBinding->setDirtyState(patchCopy.dirty);
            presetButton->repaint();
        }
        else if (aum->action == Synth::AudioToUIMsg::DO_PARAM_RESCAN)
        {
            if (!clapParamsExtension)
                clapParamsExtension = static_cast<const clap_host_params_t *>(
                    clapHost->get_extension(clapHost, CLAP_EXT_PARAMS));
            if (clapParamsExtension)
            {
                clapParamsExtension->rescan(clapHost,
                                            CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
                clapParamsExtension->request_flush(clapHost);
            }
        }
        else if (aum->action == Synth::AudioToUIMsg::SEND_SAMPLE_RATE)
        {
            engineSR = aum->value2;
            hostSR = aum->value;
            repaint();
        }
        else if (aum->action == Synth::AudioToUIMsg::SET_DAW_EXTRA_STATE)
        {
            if (aum->dawExtraStatePointer)
            {
                editorDawExtraState =
                    *static_cast<const Synth::DawExtraState *>(aum->dawExtraStatePointer);
                applyDawExtraStateFromAudio();
            }
        }
        else if (aum->action == Synth::AudioToUIMsg::UPDATE_CPU_USAGE)
        {
            settingsPanel->setCpuUsage(aum->value);
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

    auto labelCol = style()->getColour(jcmp::base_styles::BaseLabel::styleClass,
                                       jcmp::base_styles::BaseLabel::labelcolor);

    g.setColour(labelCol.withAlpha(0.9f));
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
        g.setColour(labelCol.withAlpha(0.9f - sqrt((fr - 1) / 7.0f)));
        g.strokePath(p, juce::PathStrokeType(1));
    }

    g.setColour(labelCol.withAlpha(0.5f));
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
    bi += fmt::format(" @ {:.1f}k", hostSR / 1000.0);
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

    auto panelPadX{2 * uicMargin + 11}, panelPadY{34U};
    auto sourceHeight = uicLabeledKnobHeight + panelPadY;
    auto macroStripHeight = sourceHeight;

    auto panelArea =
        lb.withTrimmedTop(presetHeight).withTrimmedBottom(footerHeight + macroStripHeight);

    auto matrixWidth = numOps * (uicPowerKnobWidth) + (numOps - 1) * uicMargin + panelPadX;
    auto matrixHeight = numOps * uicLabeledKnobHeight + (numOps - 1) * uicMargin + panelPadY;
    // Right column (main above mixer). Sized to fit the mixer row:
    //   power+level (uicPowerKnobWidth) + gap + pan knob + gap + small vertical VU.
    // This is wider than the 3-knob main panel so main just has some slack.
    auto mixerVuWidth = 22U;
    auto rightColWidth =
        uicPowerKnobWidth + uicMargin + uicKnobSize + uicMargin + mixerVuWidth + panelPadX;

    auto panelMargin{1};

    // Top bar: [ preset | vu ]
    auto topInner = presetArea.withTrimmedTop(uicMargin);
    int vuWidth = 150;

    auto vuRect = juce::Rectangle<int>(getWidth() - uicMargin - vuWidth, topInner.getY(), vuWidth,
                                       topInner.getHeight());
    vuMeter->setBounds(vuRect);

    int presetLeft = 110;
    auto presetRect = juce::Rectangle<int>(
        presetLeft, topInner.getY(), vuRect.getX() - uicMargin - presetLeft, topInner.getHeight());
    presetButton->setBounds(presetRect);

    auto sourceRect =
        juce::Rectangle<int>(panelArea.getX(), panelArea.getY(), matrixWidth, sourceHeight);
    auto matrixRect = juce::Rectangle<int>(panelArea.getX(), panelArea.getY() + sourceHeight,
                                           matrixWidth, matrixHeight);
    auto mainRect = juce::Rectangle<int>(panelArea.getX() + matrixWidth, panelArea.getY(),
                                         rightColWidth, sourceHeight);
    auto mixerRect =
        juce::Rectangle<int>(panelArea.getX() + matrixWidth, panelArea.getY() + sourceHeight,
                             rightColWidth, matrixHeight);
    auto editX = panelArea.getX() + matrixWidth + rightColWidth;
    auto editRect = juce::Rectangle<int>(editX, panelArea.getY(), panelArea.getRight() - editX,
                                         sourceHeight + matrixHeight + macroStripHeight);

    // Macro strip sits under the matrix, sized to match it. The Settings sub-panel
    // takes the remaining width (under the mixer column). Edit panel is to the right.
    auto macroRect =
        juce::Rectangle<int>(lb.getX(), panelArea.getBottom(), matrixWidth, macroStripHeight);
    auto settingsPanelRect = juce::Rectangle<int>(lb.getX() + matrixWidth, panelArea.getBottom(),
                                                  rightColWidth, macroStripHeight);

    bool flipSourceAndMatrix =
        defaultsProvider->getUserDefaultValue(Defaults::flipSourceAndMatrix, false);
    if (flipSourceAndMatrix)
    {
        auto sy = sourceRect.getY();
        auto mb = matrixRect.getBottom() - sourceRect.getHeight();
        matrixRect.setY(sy);
        mixerRect.setY(sy);
        sourceRect.setY(mb);
        mainRect.setY(mb);
    }

    sourcePanel->setBounds(sourceRect.reduced(panelMargin));
    matrixPanel->setBounds(matrixRect.reduced(panelMargin));
    mainPanel->setBounds(mainRect.reduced(panelMargin));
    mixerPanel->setBounds(mixerRect.reduced(panelMargin));
    macroPanel->setBounds(macroRect.reduced(panelMargin));
    settingsPanel->setBounds(settingsPanelRect.reduced(panelMargin));
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
    settingsPanel->clearHighlight();
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
            em.addItem(
                noExt, [cat = c, pat = e, this]()
                { this->presetManager->loadFactoryPreset(patchCopy, mainToAudio, cat, pat); });
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
                      [this, pth = up, dn]()
                      {
                          presetManager->loadUserPresetDirect(patchCopy, mainToAudio,
                                                              presetManager->userPatchesPath / pth);
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
                      [this, pth = up, dn]()
                      {
                          presetManager->loadUserPresetDirect(patchCopy, mainToAudio,
                                                              presetManager->userPatchesPath / pth);
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
    // Factory theme shortcuts — apply without opening the color editor.  The current
    // selection is read from the stored themePath default and shown as a checkmark.
    if (uiThemeManager)
    {
        auto storedTheme =
            defaultsProvider->getUserDefaultValue(Defaults::themePath, std::string("factory:Dark"));
        for (auto &ft : uiThemeManager->factoryThemes)
        {
            auto isCurrent = (storedTheme == std::string(factoryThemeSentinel) + ft.name);
            uim.addItem(ft.name, true, isCurrent,
                        [w = juce::Component::SafePointer(this), skin = ft.skin, name = ft.name]()
                        {
                            if (!w)
                                return;
                            w->applyTheme(skin, std::string(factoryThemeSentinel) + name);
                        });
        }

        // User themes submenu (only shown if there are any on disk)
        uiThemeManager->rescanUserThemes();
        if (!uiThemeManager->userThemes.empty())
        {
            auto userMenu = juce::PopupMenu();
            for (auto &path : uiThemeManager->userThemes)
            {
                auto name = path.stem().u8string();
                auto isCurrent = (storedTheme == path.u8string());
                userMenu.addItem(name, true, isCurrent,
                                 [w = juce::Component::SafePointer(this), path]()
                                 {
                                     if (!w || !w->uiThemeManager)
                                         return;
                                     w->applyTheme(w->uiThemeManager->loadThemeFromPath(path),
                                                   path.u8string());
                                 });
            }
            uim.addSubMenu("User Themes", userMenu);
        }
    }
    uim.addItem("Color Editor...",
                [w = juce::Component::SafePointer(this)]()
                {
                    if (!w)
                        return;
                    if (w->colorEditorWindow && w->colorEditorWindow->isVisible())
                    {
                        w->colorEditorWindow->toFront(true);
                        return;
                    }
                    w->openColorEditor();
                });
    uim.addSeparator();
    auto fsm = defaultsProvider->getUserDefaultValue(Defaults::flipSourceAndMatrix, false);
    uim.addItem("Sources Above Matrix", true, !fsm,
                [w = juce::Component::SafePointer(this)]()
                {
                    if (!w)
                        return;
                    w->defaultsProvider->updateUserDefaultValue(Defaults::flipSourceAndMatrix,
                                                                false);
                    w->resized();
                    w->repaint();
                });
    uim.addItem("Matrix Above Sources", true, fsm,
                [w = juce::Component::SafePointer(this)]()
                {
                    if (!w)
                        return;
                    w->defaultsProvider->updateUserDefaultValue(Defaults::flipSourceAndMatrix,
                                                                true);
                    w->resized();
                    w->repaint();
                });
    uim.addSeparator();
    uim.addItem("Activate Debug Log", true, debugLevel > 0,
                [w = juce::Component::SafePointer(this)]()
                {
                    if (w)
                        w->toggleDebug();
                });

#if JUCE_WINDOWS
    auto swr = defaultsProvider->getUserDefaultValue(Defaults::useSoftwareRenderer, false);

    uim.addItem(
        "Use Software Renderer", true, swr,
        [w = juce::Component::SafePointer(this), swr]()
        {
            if (!w)
                return;
            w->defaultsProvider->updateUserDefaultValue(Defaults::useSoftwareRenderer, !swr);
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon, "Software Renderer Change",
                "A software renderer change is only active once you restart/reload the plugin.");
        });
#endif

    p.addSubMenu("User Interface", uim);

    auto dm = juce::PopupMenu();
    auto runAll = isRunAllNodes();
    auto asoToggle = isAllSoundsOffOnToggle();
    dm.addItem("Run all nodes independent of power", true, sessionRunAllNodes,
               [w = juce::Component::SafePointer(this)]()
               {
                   if (!w)
                       return;
                   w->sessionRunAllNodes = !w->sessionRunAllNodes;
                   w->sendDesignModeToAudio();
               });
    dm.addItem("All sounds off on power toggle", true, sessionAllSoundsOffOnToggle,
               [w = juce::Component::SafePointer(this)]()
               {
                   if (!w)
                       return;
                   w->sessionAllSoundsOffOnToggle = !w->sessionAllSoundsOffOnToggle;
               });
    dm.addSeparator();
    auto prefRunAll = defaultsProvider->getUserDefaultValue(Defaults::designModeRunAllNodes, false);
    dm.addItem("Always run all nodes when UI is open", true, prefRunAll,
               [w = juce::Component::SafePointer(this), prefRunAll]()
               {
                   if (!w)
                       return;
                   w->defaultsProvider->updateUserDefaultValue(Defaults::designModeRunAllNodes,
                                                               !prefRunAll);
                   if (!prefRunAll)
                       w->sessionRunAllNodes = true;
                   w->sendDesignModeToAudio();
               });
    auto prefASO =
        defaultsProvider->getUserDefaultValue(Defaults::designModeAllSoundsOffOnToggle, false);
    dm.addItem("Always send all sounds off on toggle when UI is open", true, prefASO,
               [w = juce::Component::SafePointer(this), prefASO]()
               {
                   if (!w)
                       return;
                   w->defaultsProvider->updateUserDefaultValue(
                       Defaults::designModeAllSoundsOffOnToggle, !prefASO);
                   if (!prefASO)
                       w->sessionAllSoundsOffOnToggle = true;
               });
    p.addSubMenu("Design Mode", dm);

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

#if USE_WCHAR_PRESET
                                 w->presetManager->saveUserPresetDirect(
                                     w->patchCopy, result[0].getFullPathName().toUTF16());
#else
                                 w->presetManager->saveUserPresetDirect(w->patchCopy, pn);
#endif

                                 w->presetDataBinding->setDirtyState(false);
                                 w->repaint();
                             });
}

void SixSinesEditor::setPatchNameTo(const std::string &s)
{
    memset(patchCopy.name, 0, sizeof(patchCopy.name));
    strncpy(patchCopy.name, s.c_str(), 255);
    mainToAudio.push({Synth::MainToAudioMsg::SEND_PATCH_NAME, 0, 0, patchCopy.name});
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
            w->presetManager->loadUserPresetDirect(w->patchCopy, w->mainToAudio, loadPath);
        });
}

void SixSinesEditor::resetToDefault() { presetManager->loadInit(patchCopy, mainToAudio); }

void SixSinesEditor::setAndSendParamValue(uint32_t paramId, float value, bool notifyAudio,
                                          bool sendBeginEnd)
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
        if (sendBeginEnd)
            mainToAudio.push({Synth::MainToAudioMsg::Action::BEGIN_EDIT, paramId});
        mainToAudio.push({Synth::MainToAudioMsg::Action::SET_PARAM, paramId, value});
        if (sendBeginEnd)
            mainToAudio.push({Synth::MainToAudioMsg::Action::END_EDIT, paramId});
        requestParamsFlush();
    }
}

void SixSinesEditor::setPatchNameDisplay()
{
    if (!presetButton)
        return;
    presetDataBinding->setStateForDisplayName(patchCopy.name);
    presetButton->repaint();
}

void SixSinesEditor::postPatchChange(const std::string &s)
{
    presetDataBinding->setStateForDisplayName(s);
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

#if JUCE_WINDOWS
    auto swr = defaultsProvider->getUserDefaultValue(Defaults::useSoftwareRenderer, false);

    if (swr)
    {
        if (auto peer = getPeer())
        {
            SXSNLOG("Enabling software rendering engine");
            peer->setCurrentRenderingEngine(0); // 0 for software mode, 1 for Direct2D mode
        }
    }
#endif
}

void SixSinesEditor::initializeBaseSkin()
{
    setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
        sst::jucegui::style::StyleSheet::DARK));
    currentSkin = SixSinesSkin::darkDefault();
    currentSkin.applyToStylesheet(style());
    if (lnf)
        lnf->setStyle(style());

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
    defaultsProvider->updateUserDefaultValue(Defaults::zoomLevel, zoomFactor * 100);
    if (onZoomChanged)
        onZoomChanged(zoomFactor);
}

void SixSinesEditor::doSinglePanelHamburger()
{
    juce::Component *vis;
    for (auto c : singlePanel->getChildren())
    {
        if (c->isVisible())
        {
            vis = c;
        }
    }
    if (!vis)
        return;

    if (auto sc = dynamic_cast<SupportsClipboard *>(vis))
    {
        auto p = juce::PopupMenu();
        p.addSectionHeader(singlePanel->getName());
        p.addSeparator();

        if (sc->supportsFullNode())
        {
            p.addItem("Copy Node",
                      [this, w = juce::Component::SafePointer(vis)]()
                      {
                          if (!w)
                              return;
                          auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                          s->copyFullNodeTo(*clipboard);
                      });
            p.addItem("Paste Node", clipboard->clipboardType == sc->getFullNodeType(), false,
                      [this, w = juce::Component::SafePointer(vis)]()
                      {
                          if (!w)
                              return;
                          auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                          s->pasteFullNodeFrom(*clipboard);
                      });
            p.addItem("Reset Node",
                      [this, w = juce::Component::SafePointer(vis)]()
                      {
                          if (!w)
                              return;
                          auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                          s->resetFullNode(*clipboard);
                      });
            p.addSeparator();
        }
        p.addItem("Copy Envelope",
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->copyEnvelopeTo(*clipboard);
                  });
        p.addItem("Paste Envelope", clipboard->clipboardType == Clipboard::ENVELOPE, false,
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->pasteEnvelopeFrom(*clipboard);
                  });
        p.addItem("Reset Envelope",
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->resetEnvelope(*clipboard);
                  });
        p.addSeparator();
        p.addItem("Copy LFO",
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->copyLFOTo(*clipboard);
                  });
        p.addItem("Paste LFO", clipboard->clipboardType == Clipboard::LFO, false,
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->pasteLFOFrom(*clipboard);
                  });
        p.addItem("Reset LFO",
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->resetLFO(*clipboard);
                  });
        p.addSeparator();
        p.addItem("Copy Modulation",
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->copyModulationTo(*clipboard);
                  });
        p.addItem("Paste Modulation", clipboard->clipboardType == Clipboard::MODULATION, false,
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->pasteModulationFrom(*clipboard);
                  });
        p.addItem("Reset Modulation",
                  [this, w = juce::Component::SafePointer(vis)]()
                  {
                      if (!w)
                          return;
                      auto s = dynamic_cast<SupportsClipboard *>(w.getComponent());
                      s->resetModulation(*clipboard);
                  });

        p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this));
    }
}

void SixSinesEditor::activateHamburger(bool b)
{
    singlePanel->hasHamburger = b;
    singlePanel->repaint();
}

void SixSinesEditor::requestParamsFlush()
{
    if (!clapParamsExtension)
        clapParamsExtension = static_cast<const clap_host_params_t *>(
            clapHost->get_extension(clapHost, CLAP_EXT_PARAMS));
    if (clapParamsExtension)
    {
        clapParamsExtension->request_flush(clapHost);
    }
}

void SixSinesEditor::sneakyStartupGrabFrom(Patch &other)
{
    for (auto &p : other.params)
    {
        patchCopy.paramMap.at(p->meta.id)->value = p->value;
    }
    strncpy(patchCopy.name, other.name, 255);
    postPatchChange(other.name);
}

bool SixSinesEditor::isRunAllNodes() const { return sessionRunAllNodes; }

bool SixSinesEditor::isAllSoundsOffOnToggle() const { return sessionAllSoundsOffOnToggle; }

void SixSinesEditor::sendDesignModeToAudio()
{
    mainToAudio.push(
        {Synth::MainToAudioMsg::SET_DESIGN_MODE_RUN_ALL, 0, isRunAllNodes() ? 1.f : 0.f});
}

bool SixSinesEditor::toggleDebug()
{
    if (debugLevel == 0)
    {
        sst::plugininfra::misc_platform::allocateConsole();
    }
    if (debugLevel <= 0)
        debugLevel = 1;
    else
        debugLevel = -1;
    SXSNLOG("Started debug session");
    SXSNLOG("If you are on windows and you close this window it may end your entire session");
    return debugLevel > 0;
}

void SixSinesEditor::onStyleChanged()
{
    jcmp::WindowPanel::onStyleChanged();
    if (lnf)
        lnf->setStyle(style());
    // Propagate to the colour editor window: it is a separate top-level DocumentWindow,
    // not a child of this component, so it does not receive onStyleChanged automatically.
    if (colorEditorContent)
        colorEditorContent->setStyle(style());
}

void SixSinesEditor::scheduleDawExtraStatePush()
{
    // Debounce: restart the countdown on every call; the latest state is pushed
    // once the user pauses editing (see DesPushTimer::timerCallback).
    if (dawExtraStatePushTimer)
    {
        dawExtraStatePushTimer->stopTimer();
        dawExtraStatePushTimer->startTimer(300);
    }
}

void SixSinesEditor::pushDawExtraStateToAudio()
{
    editorDawExtraState.colorMapXml = currentSkin.toXmlString();
    Synth::MainToAudioMsg msg{Synth::MainToAudioMsg::SET_DAW_EXTRA_STATE};
    msg.dawExtraStatePointer = &editorDawExtraState;
    mainToAudio.push(msg);
}

void SixSinesEditor::applyDawExtraStateFromAudio()
{
    if (editorDawExtraState.colorMapXml.empty())
        return;
    // Apply without writing to the themePath user default: the session's colour map
    // takes precedence over the user's per-installation preference only within this
    // session.
    auto skin = SixSinesSkin::fromXmlString(editorDawExtraState.colorMapXml);
    applyTheme(skin);
}

void SixSinesEditor::applyTheme(const SixSinesSkin &skin, const std::string &preference)
{
    if (!preference.empty())
    {
        defaultsProvider->updateUserDefaultValue(Defaults::themePath, preference);
        // A non-empty preference implies user action (theme picker or saved-theme load).
        // Persist the resulting colour map into the session DawExtraState.
        scheduleDawExtraStatePush();
    }
    currentSkin = skin;
    currentSkin.applyToStylesheet(style());
    // applyToStylesheet mutates the stylesheet in-place and does not fire
    // onStyleChanged, so we explicitly notify the LookAndFeel and the colour editor
    // window (further below) to keep popup menu colours and floating window styles in sync.
    if (lnf)
        lnf->setStyle(style());
    repaint();
    // Propagate the refreshed stylesheet to the colour editor if open.
    if (colorEditorContent)
        colorEditorContent->setStyle(style());
}

void SixSinesEditor::setThemeFromPreference()
{
    if (!uiThemeManager)
    {
        // Defensive: should always be non-null after construction, but if theme manager
        // failed to initialise we still want to land on a usable dark skin.
        applyTheme(SixSinesSkin::darkDefault());
        return;
    }

    auto stored =
        defaultsProvider->getUserDefaultValue(Defaults::themePath, std::string("factory:Dark"));

    if (stored.rfind(factoryThemeSentinel, 0) == 0)
    {
        // Factory theme: look up by name in the theme manager.
        auto name = stored.substr(strlen(factoryThemeSentinel));
        for (auto &ft : uiThemeManager->factoryThemes)
        {
            if (ft.name == name)
            {
                applyTheme(ft.skin); // no preference write — we're reading the preference
                return;
            }
        }
    }
    else
    {
        // User theme file: apply if it still exists on disk.
        fs::path path(stored);
        if (fs::exists(path))
        {
            applyTheme(uiThemeManager->loadThemeFromPath(path)); // no preference write
            return;
        }
    }

    // Fallback: dark theme (factory name not found or file deleted).
    applyTheme(
        SixSinesSkin::darkDefault()); // no preference write — don't overwrite the stored name
}

void SixSinesEditor::openColorEditor()
{
    namespace jscr = sst::jucegui::screens;

    // Build entries from the current skin's logical colours.
    // ButtonText is skipped — it shares BaseLabel::labelcolor with Label.
    auto buildEntries = [](const SixSinesSkin &skin)
    {
        std::vector<jscr::ColorEditor::ColorEntry> ents;
        for (size_t i = 0; i < SixSinesSkin::numColors; i++)
        {
            auto lc = static_cast<SixSinesSkin::LogicalColor>(i);
            if (!SixSinesSkin::isEditable(lc))
                continue;
            ents.push_back({SixSinesSkin::nameFor(lc), skin.get(lc)});
        }
        return ents;
    };

    // When any colour changes: update the logical skin and re-project everything.
    // Note: this is a single-colour edit (not a full theme swap) so we mutate currentSkin
    // in-place and call applyToStylesheet directly rather than applyTheme(). The colour
    // editor's entries are already updated by ColorEditor::updateEntry before this fires.
    jscr::ColorEditor::ColorChangedFn cb =
        [w = juce::Component::SafePointer(this)](const std::string &tag, juce::Colour c)
    {
        if (!w)
            return;
        for (size_t i = 0; i < SixSinesSkin::numColors; i++)
        {
            auto lc = static_cast<SixSinesSkin::LogicalColor>(i);
            if (std::string(SixSinesSkin::nameFor(lc)) == tag)
            {
                w->currentSkin.set(lc, c);
                w->currentSkin.applyToStylesheet(w->style());
                if (w->lnf)
                    w->lnf->setStyle(w->style());
                // Re-propagate the updated stylesheet to the colour editor window so
                // that label and hex-field colours in RowComponents stay in sync.
                if (w->colorEditorContent)
                    w->colorEditorContent->setStyle(w->style());
                w->scheduleDawExtraStatePush();
                break;
            }
        }
    };

    auto ce = std::make_unique<jscr::ColorEditor>(buildEntries(currentSkin), std::move(cb), false);

    // Capture a safe pointer to the ColorEditor *before* moving it into the window content.
    // onAnyColorChanged fires after every colour change; we need to repaint both the main
    // editor (theme colours changed) and the colour editor itself (swatches updated).
    auto cePtr = juce::Component::SafePointer<jscr::ColorEditor>(ce.get());
    ce->onAnyColorChanged = [w = juce::Component::SafePointer(this), cePtr]()
    {
        if (w)
            w->repaint();
        if (cePtr)
        {
            // Repaint the whole window so swatches, buttons, and title bar all update.
            if (auto *top = cePtr->getTopLevelComponent())
                top->repaint();
        }
    };

    // ---- Build the three action buttons ----
    // Styles are applied later via ColorEditorContent::setStyle which propagates to all children.
    auto wp = juce::Component::SafePointer(this);

    auto makeBtn = [](const std::string &label) -> std::unique_ptr<jcmp::TextPushButton>
    {
        auto btn = std::make_unique<jcmp::TextPushButton>();
        btn->setLabel(label);
        return btn;
    };

    auto saveBtn = makeBtn("Save Theme...");
    saveBtn->setOnCallback(
        [w = wp]()
        {
            if (!w)
                return;
            w->colorThemeFileChooser = std::make_unique<juce::FileChooser>(
                "Save Color Theme", juce::File(w->uiThemeManager->userThemesPath.u8string()),
                "*.sixtheme");
            w->colorThemeFileChooser->launchAsync(
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [w](const juce::FileChooser &fc)
                {
                    if (!w)
                        return;
                    auto f = fc.getResult();
                    if (f.getFullPathName().isEmpty())
                        return;
                    w->uiThemeManager->saveThemeToPath(w->currentSkin,
                                                       fs::path(f.getFullPathName().toStdString()));
                    w->uiThemeManager->rescanUserThemes();
                });
        });

    auto loadBtn = makeBtn("Load Theme...");
    loadBtn->setOnCallback(
        [w = wp]()
        {
            if (!w)
                return;
            w->colorThemeFileChooser = std::make_unique<juce::FileChooser>(
                "Load Color Theme", juce::File(w->uiThemeManager->userThemesPath.u8string()),
                "*.sixtheme");
            w->colorThemeFileChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [w](const juce::FileChooser &fc)
                {
                    if (!w)
                        return;
                    auto f = fc.getResult();
                    if (f.getFullPathName().isEmpty())
                        return;
                    auto themeFSPath = fs::path(f.getFullPathName().toStdString());
                    w->applyTheme(w->uiThemeManager->loadThemeFromPath(themeFSPath),
                                  themeFSPath.u8string());
                    w->colorEditorWindow.reset();
                    w->openColorEditor();
                });
        });

    auto factoryBtn = makeBtn("Saved Themes");
    // Capture a raw pointer to the button before unique_ptr ownership moves into
    // ColorEditorContent.  Used as the popup target so the menu attaches to the
    // colour editor's top-level window (not the host plugin window), and pops up
    // beneath the button itself.
    auto factoryBtnRaw = juce::Component::SafePointer<juce::Component>(factoryBtn.get());
    // Lifetime note for the menu-item lambdas below: each lambda captures only `w`
    // (a SafePointer to the SixSinesEditor) plus a copied skin/path.  When invoked
    // from showMenuAsync, the lambda is owned by the JUCE PopupMenu data structure,
    // not by factoryBtn — so calling colorEditorWindow.reset() (which destroys
    // factoryBtn) inside the lambda is safe: the lambda's captures and storage
    // outlive the deleted button.
    factoryBtn->setOnCallback(
        [w = wp, btn = factoryBtnRaw]()
        {
            if (!w || !w->uiThemeManager)
                return;
            auto menu = juce::PopupMenu();
            menu.addSectionHeader("Factory");
            for (auto &ft : w->uiThemeManager->factoryThemes)
            {
                menu.addItem(ft.name,
                             [w, skin = ft.skin, name = ft.name]()
                             {
                                 if (!w)
                                     return;
                                 w->applyTheme(skin, std::string(factoryThemeSentinel) + name);
                                 w->colorEditorWindow.reset();
                                 w->openColorEditor();
                             });
            }
            if (!w->uiThemeManager->userThemes.empty())
            {
                menu.addSectionHeader("User");
                for (auto &path : w->uiThemeManager->userThemes)
                {
                    auto name = path.stem().u8string();
                    menu.addItem(name,
                                 [w, path]()
                                 {
                                     if (!w)
                                         return;
                                     w->applyTheme(w->uiThemeManager->loadThemeFromPath(path),
                                                   path.u8string());
                                     w->colorEditorWindow.reset();
                                     w->openColorEditor();
                                 });
                }
            }
            auto opts = juce::PopupMenu::Options();
            if (btn)
                opts = opts.withTargetComponent(btn.getComponent());
            menu.showMenuAsync(opts);
        });

    // ---- Composite content: ColorEditor above, button strip below ----
    // Inherits WindowPanel so it paints a themed gradient background and participates
    // in style propagation. A single setStyle(ss) call in the constructor reaches every
    // StyleConsumer child — colorEditor, buttons, and (via parentHierarchyChanged) the
    // hex-field TextEditors inside each lazily-created ListView row.
    struct ColorEditorContent : jcmp::WindowPanel
    {
        std::unique_ptr<jscr::ColorEditor> colorEditor;
        std::unique_ptr<jcmp::TextPushButton> saveBtn, loadBtn, factoryBtn;

        ColorEditorContent(std::unique_ptr<jscr::ColorEditor> ce,
                           std::unique_ptr<jcmp::TextPushButton> sb,
                           std::unique_ptr<jcmp::TextPushButton> lb,
                           std::unique_ptr<jcmp::TextPushButton> fb,
                           sst::jucegui::style::StyleSheet::ptr_t ss)
            : jcmp::WindowPanel(), colorEditor(std::move(ce)), saveBtn(std::move(sb)),
              loadBtn(std::move(lb)), factoryBtn(std::move(fb))
        {
            addAndMakeVisible(*colorEditor);
            addAndMakeVisible(*saveBtn);
            addAndMakeVisible(*loadBtn);
            addAndMakeVisible(*factoryBtn);
            // setStyle propagates recursively to all StyleConsumer children.
            setStyle(ss);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            auto strip = b.removeFromBottom(34);
            colorEditor->setBounds(b);
            strip = strip.reduced(6, 3);
            saveBtn->setBounds(strip.removeFromLeft(120));
            strip.removeFromLeft(6);
            loadBtn->setBounds(strip.removeFromLeft(120));
            strip.removeFromLeft(6);
            factoryBtn->setBounds(strip.removeFromLeft(120));
        }
    };

    struct CEWindow : juce::DocumentWindow
    {
        CEWindow(std::unique_ptr<ColorEditorContent> content)
            : juce::DocumentWindow("Six Sines Color Editor", juce::Colours::darkgrey,
                                   juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, false);
            setContentOwned(content.release(), true);
            setSize(420, 560);
        }
        void closeButtonPressed() override { setVisible(false); }
    };

    auto content = std::make_unique<ColorEditorContent>(
        std::move(ce), std::move(saveBtn), std::move(loadBtn), std::move(factoryBtn), style());
    // Store a safe pointer to the content so onStyleChanged() can propagate style
    // updates to this separate top-level window.
    colorEditorContent = content.get();
    colorEditorWindow = std::make_unique<CEWindow>(std::move(content));
    colorEditorWindow->setVisible(true);
}

} // namespace baconpaul::six_sines::ui
