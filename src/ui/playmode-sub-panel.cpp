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

#include "playmode-sub-panel.h"
#include <array>
#include <sstream>
#include <sst/jucegui/layouts/ListLayout.h>
#include "libMTSClient.h"

namespace baconpaul::six_sines::ui
{
PlayModeSubPanel::~PlayModeSubPanel() = default;

PlayModeSubPanel::PlayModeSubPanel(SixSinesEditor &e) : HasEditor(e)
{
    auto &on = editor.patchCopy.output;

    createComponent(editor, *this, on.bendUp, bUp, bUpD);
    createComponent(editor, *this, on.bendDown, bDn, bDnD);
    bUpL = std::make_unique<jcmp::Label>();
    bUpL->setText("+");
    bDnL = std::make_unique<jcmp::Label>();
    bDnL->setText("-");
    bendTitle = std::make_unique<jcmp::RuledLabel>();
    bendTitle->setText("Bend");

    addAndMakeVisible(*bendTitle);
    addAndMakeVisible(*bUp);
    addAndMakeVisible(*bUpL);
    addAndMakeVisible(*bDn);
    addAndMakeVisible(*bDnL);

    playTitle = std::make_unique<jcmp::RuledLabel>();
    playTitle->setText("Play");
    addAndMakeVisible(*playTitle);

    createComponent(editor, *this, on.playMode, playMode, playModeD);
    playMode->direction = sst::jucegui::components::MultiSwitch::VERTICAL;
    addAndMakeVisible(*playMode);

    createComponent(editor, *this, on.portaTime, portaTime, portaTimeD);
    addAndMakeVisible(*portaTime);
    portaL = std::make_unique<jcmp::Label>();
    portaL->setText("Porta");
    addAndMakeVisible(*portaL);
    portaTime->setEnabled(false);
    portaL->setEnabled(false);

    portaContinuationButton = std::make_unique<jcmp::TextPushButton>();
    portaContinuationButton->setOnCallback(
        [w = juce::Component::SafePointer(this)]()
        {
            if (w)
            {
                w->showPortaContinuationMenu();
            }
        });
    setPortaContinuationLabel();

    editor.componentRefreshByID[on.portaContMode.meta.id] =
        [w = juce::Component::SafePointer(this)]()
    {
        if (w)
        {
            w->setPortaContinuationLabel();
        }
    };
    addAndMakeVisible(*portaContinuationButton);

    auto op = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setEnabledState();
    };

    editor.componentRefreshByID[on.playMode.meta.id] = op;
    playModeD->onGuiSetValue = op;

    triggerButton = std::make_unique<jcmp::TextPushButton>();
    triggerButton->setOnCallback(
        [w = juce::Component::SafePointer(this)]()
        {
            if (w)
            {
                w->showTriggerButtonMenu();
            }
        });
    setTriggerButtonLabel();
    editor.componentRefreshByID[on.defaultTrigger.meta.id] =
        [w = juce::Component::SafePointer(this)]()
    {
        if (w)
        {
            w->setTriggerButtonLabel();
        }
    };
    addAndMakeVisible(*triggerButton);

    createComponent(editor, *this, on.pianoModeActive, pianoModeButton, pianoModeButtonD);
    pianoModeButton->setLabel("Piano Mode");
    addAndMakeVisible(*pianoModeButton);

    createComponent(editor, *this, on.unisonCount, uniCt, uniCtD);
    addAndMakeVisible(*uniCt);
    uniCtD->onGuiSetValue = op;

    uniCtL = std::make_unique<jcmp::Label>();
    uniCtL->setText("Voice");
    addAndMakeVisible(*uniCtL);

    createComponent(editor, *this, on.uniPhaseRand, uniRPhase, uniRPhaseDD);
    uniRPhase->setLabel("Rand Phase");
    addAndMakeVisible(*uniRPhase);

    createRescaledComponent(editor, *this, on.unisonSpread, uniSpread, uniSpreadD);
    addAndMakeVisible(*uniSpread);
    uniSpreadG = std::make_unique<jcmp::GlyphPainter>(jcmp::GlyphPainter::TUNING);
    addAndMakeVisible(*uniSpreadG);

    createComponent(editor, *this, on.unisonPan, uniPan, uniPanD);
    addAndMakeVisible(*uniPan);
    uniPanG = std::make_unique<jcmp::GlyphPainter>(jcmp::GlyphPainter::STEREO);
    addAndMakeVisible(*uniPanG);

    uniTitle = std::make_unique<jcmp::RuledLabel>();
    uniTitle->setText("Unison");
    addAndMakeVisible(*uniTitle);
    editor.componentRefreshByID[on.unisonCount.meta.id] = op;

    smoothingSectionTitle = std::make_unique<jcmp::RuledLabel>();
    smoothingSectionTitle->setText("MPE + Smoothing");
    addAndMakeVisible(*smoothingSectionTitle);

    mpeRowLabel = std::make_unique<jcmp::Label>();
    mpeRowLabel->setText("MPE");
    mpeRowLabel->setJustification(juce::Justification::centredRight);
    addAndMakeVisible(*mpeRowLabel);

    mpeActiveButtonD = std::make_unique<decltype(mpeActiveButtonD)::element_type>(
        editor.editorDawExtraState.mpeActive);
    mpeActiveButtonD->setLabel("MPE Active");
    mpeActiveButtonD->widget->setLabel("Active");
    mpeActiveButtonD->onValueChanged = [w = juce::Component::SafePointer(this), op](bool v)
    {
        if (!w)
            return;
        Synth::MainToAudioMsg m{Synth::MainToAudioMsg::SET_MPE_ACTIVE};
        m.value = v ? 1.f : 0.f;
        w->editor.mainToAudio.push(m);
        op();
    };
    addAndMakeVisible(*mpeActiveButtonD->widget);

    mpeRangeEditor = std::make_unique<jcmp::TextEditor>();
    mpeRangeEditor->setMultiLine(false);
    mpeRangeEditor->setReturnKeyStartsNewLine(false);
    mpeRangeEditor->setJustification(juce::Justification::centred);
    mpeRangeEditor->setIndents(3, 1);
    mpeRangeEditor->setInputRestrictions(3, "0123456789");
    mpeRangeEditor->setTitle("MPE Bend Range");
    mpeRangeEditor->onReturnKey = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->commitMpeRangeEditor();
    };
    mpeRangeEditor->onFocusLost = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->commitMpeRangeEditor();
    };
    refreshMpeRangeEditor();
    addAndMakeVisible(*mpeRangeEditor);

    // Refresh widget visuals + dependent enabled-state when the dawExtraState changes
    // (e.g. CLAP host stateLoad echoes a SET_DAW_EXTRA_STATE back to the UI).
    editor.dawExtraStateRefreshListeners.push_back(
        [w = juce::Component::SafePointer(this)]()
        {
            if (!w)
                return;
            if (w->mpeActiveButtonD && w->mpeActiveButtonD->widget)
                w->mpeActiveButtonD->widget->repaint();
            w->refreshMpeRangeEditor();
            w->refreshMidiSmoothingButton();
            w->refreshParamSmoothingButton();
            w->setEnabledState();
        });

    mpeRangeL = std::make_unique<jcmp::Label>();
    mpeRangeL->setText("Bend");
    mpeRangeL->setJustification(juce::Justification::centredRight);
    addAndMakeVisible(*mpeRangeL);

    smoothingRowLabel = std::make_unique<jcmp::Label>();
    smoothingRowLabel->setText("MIDI Smoothing");
    smoothingRowLabel->setJustification(juce::Justification::centredRight);
    addAndMakeVisible(*smoothingRowLabel);

    midiSmoothingButton = std::make_unique<jcmp::MenuButton>();
    midiSmoothingButton->setOnCallback(
        [w = juce::Component::SafePointer(this)]()
        {
            if (w)
                w->showMidiSmoothingMenu();
        });
    refreshMidiSmoothingButton();
    addAndMakeVisible(*midiSmoothingButton);

    paramSmoothingRowLabel = std::make_unique<jcmp::Label>();
    paramSmoothingRowLabel->setText("Param Smoothing");
    paramSmoothingRowLabel->setJustification(juce::Justification::centredRight);
    addAndMakeVisible(*paramSmoothingRowLabel);

    paramSmoothingButton = std::make_unique<jcmp::MenuButton>();
    paramSmoothingButton->setOnCallback(
        [w = juce::Component::SafePointer(this)]()
        {
            if (w)
                w->showParamSmoothingMenu();
        });
    refreshParamSmoothingButton();
    addAndMakeVisible(*paramSmoothingButton);

    tsposeTitle = std::make_unique<jcmp::RuledLabel>();
    tsposeTitle->setText("Octave");
    addAndMakeVisible(*tsposeTitle);

    createComponent(editor, *this, on.octTranspose, tsposeButton, tsposeButtonD);
    addAndMakeVisible(*tsposeButton);

    voiceLimitL = std::make_unique<jcmp::RuledLabel>();
    voiceLimitL->setText("Voices");
    addAndMakeVisible(*voiceLimitL);
    voiceLimit = std::make_unique<jcmp::MenuButton>();
    voiceLimit->setLabelAndTitle(std::to_string(getPolyLimit()),
                                 "Voice Limit " + std::to_string(getPolyLimit()));
    voiceLimit->setOnCallback([this]() { showPolyLimitMenu(); });
    editor.componentRefreshByID[editor.patchCopy.output.polyLimit.meta.id] =
        [w = juce::Component::SafePointer(this)]()
    {
        if (!w)
            return;
        w->voiceLimit->setLabelAndTitle(std::to_string(w->getPolyLimit()),
                                        "Voice Limit " + std::to_string(w->getPolyLimit()));
    };
    addAndMakeVisible(*voiceLimit);

    mtsTitle = std::make_unique<jcmp::RuledLabel>();
    mtsTitle->setText("MTS");
    addAndMakeVisible(*mtsTitle);
    mtsStatusLabel = std::make_unique<jcmp::Label>();
    mtsStatusLabel->setText("No MTS");
    mtsStatusLabel->setJustification(juce::Justification::centred);
    mtsStatusLabel->setEnabled(false);
    addAndMakeVisible(*mtsStatusLabel);

    panicTitle = std::make_unique<jcmp::RuledLabel>();
    panicTitle->setText("Panic");
    panicButton = std::make_unique<jcmp::TextPushButton>();
    panicButton->setLabel("Sound Off");
    panicButton->setOnCallback(
        [this]() { editor.mainToAudio.push({Synth::MainToAudioMsg::PANIC_STOP_VOICES}); });
    addAndMakeVisible(*panicButton);
    addAndMakeVisible(*panicTitle);

    outputControlTitle = std::make_unique<jcmp::RuledLabel>();
    outputControlTitle->setText("Output Stage (Main)");
    addAndMakeVisible(*outputControlTitle);

    auto &out = editor.patchCopy.output;
    createComponent(editor, *this, out.sampleRateStrategy, srStrat, srStratD);
    addAndMakeVisible(*srStrat);
    createComponent(editor, *this, out.resampleEngine, rsEng, rsEngD);
    addAndMakeVisible(*rsEng);

    createComponent(editor, *this, out.saturationType, satType, satTypeD);
    addAndMakeVisible(*satType);
    createComponent(editor, *this, out.saturationDrive, satDrive, satDriveD);
    addAndMakeVisible(*satDrive);
    createComponent(editor, *this, out.lowpass, lowpass, lowpassD);
    addAndMakeVisible(*lowpass);
    createComponent(editor, *this, out.bitRateAdjust, bitRate, bitRateD);
    addAndMakeVisible(*bitRate);
    createComponent(editor, *this, out.bitDepthAdjust, bitDepth, bitDepthD);
    addAndMakeVisible(*bitDepth);
    createComponent(editor, *this, out.highpass, highpass, highpassD);
    addAndMakeVisible(*highpass);
    createComponent(editor, *this, out.outputGain, outGain, outGainD);
    addAndMakeVisible(*outGain);

    auto mkLabel = [this](auto &slot, const std::string &t,
                          juce::Justification j = juce::Justification::centredRight)
    {
        slot = std::make_unique<jcmp::Label>();
        slot->setText(t);
        slot->setJustification(j);
        addAndMakeVisible(*slot);
    };
    mkLabel(sampleRateLabel, "Sample Rate:");
    mkLabel(saturationLabel, "Saturation:");
    mkLabel(lowpassLabel, "Low Pass:");
    mkLabel(bitRateLabel, "Bit Rate:");
    mkLabel(bitDepthLabel, "Bit Depth:");
    mkLabel(highpassLabel, "High Pass:");
    mkLabel(outGainLabel, "Output:");
    mkLabel(downsamplerLabel, "Downsampler:");

    setEnabledState();
}

void PlayModeSubPanel::resized()
{
    namespace jlo = sst::jucegui::layouts;
    auto skinny = uicKnobSize + 30;

    // Top section: 4 columns, each split into a top sub-section and a bottom sub-section
    //   col1: Voices / Unison
    //   col2: Play (mode + triggers + porta)
    //   col3: Bend / Octave
    //   col4: MTS / Panic
    // The two tallest columns (1 and 2) drive the row height.
    auto topRowHeight = uicTitleLabelInnerBox + 7 * uicLabelHeight + 7 * uicMargin;

    auto outer = jlo::VList().at(uicMargin, 0).withAutoGap(2 * uicMargin);

    auto topRow = jlo::HList().withHeight(topRowHeight).withAutoGap(2 * uicMargin);

    auto col1 = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);
    col1.add(titleLabelGaplessLayout(voiceLimitL));
    col1.add(jlo::Component(*voiceLimit).withHeight(uicLabelHeight));
    col1.add(titleLabelGaplessLayout(uniTitle));
    col1.add(jlo::Component(*uniCt).withHeight(uicLabelHeight));
    col1.add(jlo::Component(*uniCtL).withHeight(uicLabelHeight));
    col1.add(jlo::Component(*uniRPhase).withHeight(uicLabelHeight));
    col1.add(sideLabelSlider(uniSpreadG, uniSpread));
    col1.add(sideLabelSlider(uniPanG, uniPan));
    topRow.add(col1);

    auto col2 = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);
    col2.add(titleLabelGaplessLayout(playTitle));
    col2.add(jlo::Component(*playMode).withHeight(2 * uicLabelHeight + uicMargin));
    col2.add(jlo::Component(*triggerButton).withHeight(uicLabelHeight));
    col2.add(jlo::Component(*pianoModeButton).withHeight(uicLabelHeight));
    col2.add(jlo::Component(*portaL).withHeight(uicLabelHeight));
    col2.add(jlo::Component(*portaTime).withHeight(uicLabelHeight).insetBy(0, 2));
    col2.add(jlo::Component(*portaContinuationButton).withHeight(uicLabelHeight));
    topRow.add(col2);

    auto col3 = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);
    col3.add(titleLabelGaplessLayout(bendTitle));
    col3.add(sideLabel(bUpL, bUp));
    col3.add(sideLabel(bDnL, bDn));
    col3.add(titleLabelGaplessLayout(tsposeTitle));
    col3.add(jlo::Component(*tsposeButton).withHeight(uicLabelHeight));
    topRow.add(col3);

    auto col4 = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);
    col4.add(titleLabelGaplessLayout(mtsTitle));
    col4.add(jlo::Component(*mtsStatusLabel).withHeight(uicLabelHeight));
    col4.add(titleLabelGaplessLayout(panicTitle));
    col4.add(jlo::Component(*panicButton).withHeight(uicLabelHeight));
    topRow.add(col4);

    outer.add(topRow);

    auto fullWidth = getWidth() - 2 * uicMargin;

    // "MPE + Smoothing" section: full-width title with three rows underneath.
    outer.add(titleLabelGaplessLayout(smoothingSectionTitle).withWidth(fullWidth));

    auto smoothingColHeight = 3 * uicLabelHeight + 2 * uicMargin;
    auto smoothingCol =
        jlo::VList().withWidth(fullWidth).withHeight(smoothingColHeight).withAutoGap(uicMargin);
    int labelW = fullWidth / 3;

    auto mpeRow = jlo::HList().withHeight(uicLabelHeight).withAutoGap(uicMargin);
    mpeRow.add(jlo::Component(*mpeRowLabel).withWidth(labelW));
    mpeRow.add(jlo::Component(*mpeActiveButtonD->widget).withWidth(uicSubPanelColumnWidth * 1.5));
    mpeRow.add(jlo::Component(*mpeRangeL).withWidth(uicLabelHeight * 2));
    mpeRow.add(jlo::Component(*mpeRangeEditor).expandToFill());
    smoothingCol.add(mpeRow);

    auto smRow = jlo::HList().withHeight(uicLabelHeight).withAutoGap(uicMargin);
    smRow.add(jlo::Component(*smoothingRowLabel).withWidth(labelW));
    smRow.add(jlo::Component(*midiSmoothingButton).expandToFill());
    smoothingCol.add(smRow);

    auto psRow = jlo::HList().withHeight(uicLabelHeight).withAutoGap(uicMargin);
    psRow.add(jlo::Component(*paramSmoothingRowLabel).withWidth(labelW));
    psRow.add(jlo::Component(*paramSmoothingButton).expandToFill());
    smoothingCol.add(psRow);

    outer.add(smoothingCol);

    outer.add(titleLabelGaplessLayout(outputControlTitle).withWidth(fullWidth));

    // Output signal path. Each row is "Label : [control(s)]". Saturation row also
    // gets a drive HSlider following the type jog. Future revision will add
    // arrow connectors between stages.
    auto pathCol = jlo::VList().withWidth(fullWidth).withAutoGap(uicMargin);

    auto stageRow = [&](auto &lbl, auto &c1, juce::Component *c2 = nullptr)
    {
        auto row = jlo::HList().withHeight(uicLabelHeight).withAutoGap(uicMargin);
        row.add(jlo::Component(*lbl).withWidth(labelW));
        if (c2)
        {
            // Split the right side: jog on the left (skinny), slider fills the rest
            row.add(jlo::Component(*c1).withWidth(uicSubPanelColumnWidth * 1.5));
            row.add(jlo::Component(*c2).expandToFill());
        }
        else
        {
            row.add(jlo::Component(*c1).expandToFill());
        }
        return row;
    };

    pathCol.add(stageRow(sampleRateLabel, srStrat));
    pathCol.add(stageRow(saturationLabel, satType, satDrive.get()));
    pathCol.add(stageRow(bitRateLabel, bitRate));
    pathCol.add(stageRow(bitDepthLabel, bitDepth));
    pathCol.add(stageRow(lowpassLabel, lowpass));
    pathCol.add(stageRow(highpassLabel, highpass));
    pathCol.add(stageRow(outGainLabel, outGain));
    pathCol.add(stageRow(downsamplerLabel, rsEng));

    outer.add(pathCol);

    outer.doLayout();
}

void PlayModeSubPanel::setTriggerButtonLabel()
{
    auto v = (int)std::round(editor.patchCopy.output.defaultTrigger.value);
    switch (v)
    {
    case KEY_PRESS:
        triggerButton->setLabel("On Key");
        break;
    default:
    case NEW_GATE:
        triggerButton->setLabel("On Str/Rel");
        break;
    }
}

void PlayModeSubPanel::showTriggerButtonMenu()
{
    auto tmv = (int)std::round(editor.patchCopy.output.defaultTrigger.value);

    auto genSet = [w = juce::Component::SafePointer(this)](int nv)
    {
        auto that = w;
        return [nv, that]()
        {
            auto &p = that->editor.patchCopy.output.defaultTrigger;
            that->editor.setAndSendParamValue(p, nv);
            that->setTriggerButtonLabel();
        };
    };
    auto p = juce::PopupMenu();
    p.addSectionHeader("Default Trigger Mode");
    p.addSeparator();
    for (int g : {(int)TriggerMode::KEY_PRESS, (int)TriggerMode::NEW_GATE})
    {
        p.addItem(TriggerModeName[g], true, tmv == g, genSet(g));
    }
    p.addSeparator();
    auto rpo = editor.patchCopy.output.rephaseOnRetrigger;
    auto rnp = editor.patchCopy.output.uniPhaseRand;
    auto uc = (int)editor.patchCopy.output.unisonCount.value;
    p.addItem("Reset Phase on Retrigger", !rnp || (uc == 1), rpo,
              [rpo, w = juce::Component::SafePointer(this)]()
              {
                  if (!w)
                      return;
                  w->editor.setAndSendParamValue(w->editor.patchCopy.output.rephaseOnRetrigger,
                                                 !rpo);
              });

    p.addSeparator();
    auto atfl = editor.patchCopy.output.attackFloorOnRetrig;
    p.addItem("Attack Floored on Retrigger", true, atfl,
              [atfl, w = juce::Component::SafePointer(this)]()
              {
                  if (!w)
                      return;
                  w->editor.setAndSendParamValue(w->editor.patchCopy.output.attackFloorOnRetrig,
                                                 !atfl);
              });

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&this->editor),
                    makeMenuAccessibleButtonCB(triggerButton.get()));
}

void PlayModeSubPanel::setPortaContinuationLabel()
{
    auto v = (int)std::round(editor.patchCopy.output.portaContMode.value);
    switch (v)
    {
    case 0:
        portaContinuationButton->setLabel("Reset");
        break;
    case 1:
        portaContinuationButton->setLabel("Pause");
        break;
    case 2:
        portaContinuationButton->setLabel("Continue");
        break;
    }
}

void PlayModeSubPanel::showPortaContinuationMenu()
{
    auto tmv = (int)std::round(editor.patchCopy.output.portaContMode.value);

    auto genSet = [w = juce::Component::SafePointer(this)](int nv)
    {
        auto that = w;
        return [nv, that]()
        {
            auto &p = that->editor.patchCopy.output.portaContMode;
            that->editor.setAndSendParamValue(p, nv);
            that->setPortaContinuationLabel();
        };
    };
    auto p = juce::PopupMenu();
    p.addSectionHeader("Portamento Continuation");
    p.addSeparator();
    p.addItem("Reset after Release", true, tmv == 0, genSet(0));
    p.addItem("Pause after Release", true, tmv == 1, genSet(1));
    p.addItem("Continue after Release", true, tmv == 2, genSet(2));

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&this->editor),
                    makeMenuAccessibleButtonCB(portaContinuationButton.get()));
}

void PlayModeSubPanel::setEnabledState()
{
    auto vm = editor.patchCopy.output.playMode.value;
    auto en = vm > 0.5;
    portaL->setEnabled(en);
    portaTime->setEnabled(en);
    portaContinuationButton->setEnabled(en);
    pianoModeButton->setEnabled(!en);

    auto uc = editor.patchCopy.output.unisonCount.value;
    uniSpread->setEnabled(uc > 1.5);
    uniSpreadG->setEnabled(uc > 1.5);
    uniPan->setEnabled(uc > 1.5);
    uniPanG->setEnabled(uc > 1.5);
    uniRPhase->setEnabled(uc > 1.5);
    if (uc < 1.5)
        uniCtL->setText("Voice");
    else
        uniCtL->setText("Voices");

    auto me = editor.editorDawExtraState.mpeActive;
    mpeRangeEditor->setEnabled(me);
    mpeRangeL->setEnabled(me);

    repaint();
}

void PlayModeSubPanel::showPolyLimitMenu()
{
    auto p = juce::PopupMenu();
    p.addSectionHeader("Voice Limit");
    p.addSeparator();
    auto currentLimit = getPolyLimit();
    for (auto lim : {4, 6, 8, 12, 16, 24, 32, 48, 64})
    {
        p.addItem(std::to_string(lim), true, lim == currentLimit,
                  [w = juce::Component::SafePointer(this), lim]()
                  {
                      if (!w)
                          return;
                      w->setPolyLimit(lim);
                  });
    }
    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor));
}

int PlayModeSubPanel::getPolyLimit()
{
    return (int)std::round(editor.patchCopy.output.polyLimit.value);
}
void PlayModeSubPanel::setPolyLimit(int plVal)
{
    SXSNLOG("Setting val to " << plVal);
    auto &pl = editor.patchCopy.output.polyLimit;
    editor.setAndSendParamValue(pl, plVal);

    voiceLimit->setLabelAndTitle(std::to_string(getPolyLimit()),
                                 "Voice Limit " + std::to_string(getPolyLimit()));
    voiceLimit->repaint();
}

void PlayModeSubPanel::updateMTSStatus()
{
    if (++mtsIdleCount < 50)
        return;
    mtsIdleCount = 0;

    auto *cli = editor.mtsClient;
    bool connected = cli && MTS_HasMaster(cli);
    std::string text;
    if (connected)
    {
        const char *name = MTS_GetScaleName(cli);
        text = (name && *name) ? std::string(name) : std::string("Connected");
    }
    else
    {
        text = "No MTS";
    }

    if (connected != mtsConnected || text != mtsStatusLabel->getText())
    {
        mtsConnected = connected;
        mtsStatusLabel->setEnabled(connected);
        mtsStatusLabel->setText(text);
    }
}

void PlayModeSubPanel::refreshMpeRangeEditor()
{
    auto v = editor.editorDawExtraState.mpeBendRange;
    mpeRangeEditor->setAllText(std::to_string(v));
}

void PlayModeSubPanel::commitMpeRangeEditor()
{
    auto txt = mpeRangeEditor->getText().trim();
    int v = txt.isEmpty() ? editor.editorDawExtraState.mpeBendRange : txt.getIntValue();
    v = std::clamp(v, 1, 96);
    editor.editorDawExtraState.mpeBendRange = v;
    Synth::MainToAudioMsg m{Synth::MainToAudioMsg::SET_MPE_BEND_RANGE};
    m.value = (float)v;
    editor.mainToAudio.push(m);
    refreshMpeRangeEditor();
}

// User-selectable smoothing values, in ms. Engine accepts any float; these are
// just convenient presets surfaced in the menus.
static constexpr std::array<float, 5> midiSmoothingChoicesMs{5.f, 10.f, 25.f, 50.f, 100.f};
static constexpr std::array<float, 4> paramSmoothingChoicesMs{2.f, 5.f, 10.f, 25.f};

static std::string formatSmoothingMs(float v)
{
    std::ostringstream oss;
    if (v == (int)v)
        oss << (int)v << "ms";
    else
        oss << v << "ms";
    return oss.str();
}

void PlayModeSubPanel::refreshMidiSmoothingButton()
{
    auto text = formatSmoothingMs(editor.editorDawExtraState.midiCCSmoothingTimeMs);
    midiSmoothingButton->setLabelAndTitle(text, "MIDI Smoothing " + text);
    midiSmoothingButton->repaint();
}

void PlayModeSubPanel::showMidiSmoothingMenu()
{
    auto current = editor.editorDawExtraState.midiCCSmoothingTimeMs;
    auto p = juce::PopupMenu();
    p.addSectionHeader("MIDI Smoothing");
    p.addSeparator();
    for (auto ms : midiSmoothingChoicesMs)
    {
        bool ticked = std::abs(current - ms) < 0.001f;
        p.addItem(formatSmoothingMs(ms), true, ticked,
                  [w = juce::Component::SafePointer(this), ms]()
                  {
                      if (!w)
                          return;
                      w->editor.editorDawExtraState.midiCCSmoothingTimeMs = ms;
                      Synth::MainToAudioMsg m{Synth::MainToAudioMsg::SET_MIDI_CC_SMOOTHING_TIME_MS};
                      m.value = ms;
                      w->editor.mainToAudio.push(m);
                      w->refreshMidiSmoothingButton();
                  });
    }
    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor),
                    makeMenuAccessibleButtonCB(midiSmoothingButton.get()));
}

void PlayModeSubPanel::refreshParamSmoothingButton()
{
    auto text = formatSmoothingMs(editor.editorDawExtraState.paramAutomationSmoothingTimeMs);
    paramSmoothingButton->setLabelAndTitle(text, "Param Smoothing " + text);
    paramSmoothingButton->repaint();
}

void PlayModeSubPanel::showParamSmoothingMenu()
{
    auto current = editor.editorDawExtraState.paramAutomationSmoothingTimeMs;
    auto p = juce::PopupMenu();
    p.addSectionHeader("Param Smoothing");
    p.addSeparator();
    for (auto ms : paramSmoothingChoicesMs)
    {
        bool ticked = std::abs(current - ms) < 0.001f;
        p.addItem(formatSmoothingMs(ms), true, ticked,
                  [w = juce::Component::SafePointer(this), ms]()
                  {
                      if (!w)
                          return;
                      w->editor.editorDawExtraState.paramAutomationSmoothingTimeMs = ms;
                      Synth::MainToAudioMsg m{
                          Synth::MainToAudioMsg::SET_PARAM_AUTOMATION_SMOOTHING_TIME_MS};
                      m.value = ms;
                      w->editor.mainToAudio.push(m);
                      w->refreshParamSmoothingButton();
                  });
    }
    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor),
                    makeMenuAccessibleButtonCB(paramSmoothingButton.get()));
}

} // namespace baconpaul::six_sines::ui
