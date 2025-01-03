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

namespace baconpaul::six_sines::ui
{
PlayModeSubPanel::PlayModeSubPanel(SixSinesEditor &e) : HasEditor(e)
{
    auto &on = editor.patchCopy.output;

    createComponent(editor, *this, on.bendUp, bUp, bUpD);
    createComponent(editor, *this, on.bendDown, bDn, bDnD);
    bUpL = std::make_unique<jcmp::Label>();
    bUpL->setText("+");
    bDnL = std::make_unique<jcmp::Label>();
    bDnL->setText("-");
    bendTitle = std::make_unique<RuledLabel>();
    bendTitle->setText("Bend");

    addAndMakeVisible(*bendTitle);
    addAndMakeVisible(*bUp);
    addAndMakeVisible(*bUpL);
    addAndMakeVisible(*bDn);
    addAndMakeVisible(*bDnL);

    playTitle = std::make_unique<RuledLabel>();
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

    createComponent(editor, *this, on.unisonSpread, uniSpread, uniSpreadD);
    addAndMakeVisible(*uniSpread);
    uniSpreadL = std::make_unique<jcmp::Label>();
    uniSpreadL->setText("Spread");
    addAndMakeVisible(*uniSpreadL);

    uniTitle = std::make_unique<RuledLabel>();
    uniTitle->setText("Unison");
    addAndMakeVisible(*uniTitle);
    editor.componentRefreshByID[on.unisonCount.meta.id] = op;

    mpeTitle = std::make_unique<RuledLabel>();
    mpeTitle->setText("MPE");
    addAndMakeVisible(*mpeTitle);

    createComponent(editor, *this, on.mpeActive, mpeActiveButton, mpeActiveButtonD);
    addAndMakeVisible(*mpeActiveButton);
    mpeActiveButton->setLabel("MPE Active");
    mpeActiveButtonD->onGuiSetValue = op;
    editor.componentRefreshByID[on.mpeActive.meta.id] = op;

    createComponent(editor, *this, on.mpeBendRange, mpeRange, mpeRangeD);
    addAndMakeVisible(*mpeRange);

    mpeRangeL = std::make_unique<jcmp::Label>();
    mpeRangeL->setText("Bend");
    addAndMakeVisible(*mpeRangeL);

    tsposeTitle = std::make_unique<RuledLabel>();
    tsposeTitle->setText("Octave");
    addAndMakeVisible(*tsposeTitle);

    createComponent(editor, *this, on.octTranspose, tsposeButton, tsposeButtonD);
    addAndMakeVisible(*tsposeButton);

    voiceLimitL = std::make_unique<RuledLabel>();
    voiceLimitL->setText("Voices");
    addAndMakeVisible(*voiceLimitL);
    voiceLimit = std::make_unique<jcmp::MenuButton>();
    voiceLimit->setLabel(std::to_string(getPolyLimit()));
    voiceLimit->setOnCallback([this]() { showPolyLimitMenu(); });
    editor.componentRefreshByID[editor.patchCopy.output.polyLimit.meta.id] =
        [w = juce::Component::SafePointer(this)]()
    {
        if (!w)
            return;
        w->voiceLimit->setLabel(std::to_string(w->getPolyLimit()));
    };
    addAndMakeVisible(*voiceLimit);

    panicTitle = std::make_unique<RuledLabel>();
    panicTitle->setText("Panic");
    panicButton = std::make_unique<jcmp::TextPushButton>();
    panicButton->setLabel("Sound Off");
    panicButton->setOnCallback(
        [this]() { editor.uiToAudio.push({Synth::UIToAudioMsg::PANIC_STOP_VOICES}); });
    addAndMakeVisible(*panicButton);
    addAndMakeVisible(*panicTitle);

    setEnabledState();
}

void PlayModeSubPanel::resized()
{
    auto r = getLocalBounds().reduced(uicMargin, 0);
    auto depx = r.getX();
    auto depy = getLocalBounds().getY();
    auto xtraW = 15;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, voiceLimitL);
    depy += uicTitleLabelHeight;
    voiceLimit->setBounds(
        juce::Rectangle<int>(depx, depy, uicKnobSize + 2 * xtraW, uicLabelHeight));
    depy += uicLabelHeight + uicMargin;

    auto pmw{14};
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, bendTitle);
    auto bbx = juce::Rectangle<int>(depx, depy + uicTitleLabelHeight, uicKnobSize + 2 * xtraW,
                                    uicLabelHeight);
    bUpL->setBounds(bbx.withWidth(pmw));
    bUp->setBounds(bbx.withTrimmedLeft(pmw + uicMargin));
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);

    bDnL->setBounds(bbx.withWidth(pmw));
    bDn->setBounds(bbx.withTrimmedLeft(pmw + uicMargin));
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);

    positionTitleLabelAt(bbx.getX(), bbx.getY(), bbx.getWidth(), tsposeTitle);
    bbx = bbx.translated(0, uicMargin + uicLabelHeight);
    tsposeButton->setBounds(bbx.withHeight(uicLabelHeight));

    depx += bbx.getWidth() + uicMargin;
    depy = r.getY();
    positionTitleLabelAt(depx, depy, bbx.getWidth(), playTitle);
    bbx = juce::Rectangle<int>(depx, depy + uicTitleLabelHeight, bbx.getWidth(), uicLabelHeight);
    playMode->setBounds(bbx.withHeight(2 * uicLabelHeight + uicMargin));
    bbx = bbx.translated(0, 2 * uicLabelHeight + 2 * uicMargin);
    triggerButton->setBounds(bbx.withHeight(uicLabelHeight));
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);
    pianoModeButton->setBounds(bbx.withHeight(uicLabelHeight));
    pianoModeButton->setLabel("Piano Mode");
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);

    positionKnobAndLabel(bbx.getX() + xtraW, bbx.getY() + uicMargin, portaTime, portaL);

    depx += bbx.getWidth() + uicMargin;
    depy = r.getY();
    positionTitleLabelAt(depx, depy, bbx.getWidth(), uniTitle);
    bbx = juce::Rectangle<int>(depx, depy + uicTitleLabelHeight, bbx.getWidth(), uicLabelHeight);
    uniCt->setBounds(bbx.withHeight(uicLabelHeight));
    bbx = bbx.translated(0, uicMargin + uicLabelHeight);
    uniCtL->setBounds(bbx.withHeight(uicLabelHeight));
    bbx = bbx.translated(0, uicMargin + uicLabelHeight);
    uniRPhase->setBounds(bbx.withHeight(uicLabelHeight));
    positionKnobAndLabel(bbx.getX() + xtraW, portaTime->getY(), uniSpread, uniSpreadL);

    depx += bbx.getWidth() + uicMargin;
    depy = r.getY();
    positionTitleLabelAt(depx, depy, bbx.getWidth(), mpeTitle);
    bbx = juce::Rectangle<int>(depx, depy + uicTitleLabelHeight, bbx.getWidth(), uicLabelHeight);
    mpeActiveButton->setBounds(bbx.withHeight(uicLabelHeight));
    bbx = bbx.translated(0, uicMargin + uicLabelHeight);
    mpeRange->setBounds(bbx.withHeight(uicLabelHeight));
    bbx = bbx.translated(0, uicMargin + uicLabelHeight);
    mpeRangeL->setBounds(bbx.withHeight(uicLabelHeight));
    bbx = bbx.translated(0, 2 * uicMargin + uicLabelHeight);

    positionTitleLabelAt(depx, bbx.getY(), bbx.getWidth(), panicTitle);
    bbx = bbx.translated(0, uicTitleLabelHeight);
    panicButton->setBounds(bbx.withHeight(uicLabelHeight));
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
            auto pid = that->editor.patchCopy.output.defaultTrigger.meta.id;
            that->editor.patchCopy.paramMap.at(pid)->value = nv;
            that->setTriggerButtonLabel();

            that->editor.uiToAudio.push({Synth::UIToAudioMsg::Action::SET_PARAM, pid, (float)nv});
            that->editor.flushOperator();
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
    p.addItem("Reset Phase on Retrigger", !rnp, rpo,
              [rpo, w = juce::Component::SafePointer(this)]()
              {
                  if (!w)
                      return;
                  w->editor.patchCopy.output.rephaseOnRetrigger = !rpo;
                  w->editor.uiToAudio.push({Synth::UIToAudioMsg::SET_PARAM,
                                            w->editor.patchCopy.output.rephaseOnRetrigger.meta.id,
                                            (float)(!rpo)});
              });

    p.addSeparator();
    auto atfl = editor.patchCopy.output.attackFloorOnRetrig;
    p.addItem("Attack Floored on Retrigger", true, atfl,
              [atfl, w = juce::Component::SafePointer(this)]()
              {
                  if (!w)
                      return;
                  w->editor.patchCopy.output.attackFloorOnRetrig = !atfl;
                  w->editor.uiToAudio.push({Synth::UIToAudioMsg::SET_PARAM,
                                            w->editor.patchCopy.output.attackFloorOnRetrig.meta.id,
                                            (float)(!atfl)});
              });

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&this->editor));
}

void PlayModeSubPanel::setEnabledState()
{
    auto vm = editor.patchCopy.output.playMode.value;
    auto en = vm > 0.5;
    portaL->setEnabled(en);
    portaTime->setEnabled(en);
    pianoModeButton->setEnabled(!en);

    auto uc = editor.patchCopy.output.unisonCount.value;
    uniSpread->setEnabled(uc > 1.5);
    uniSpreadL->setEnabled(uc > 1.5);
    uniRPhase->setEnabled(uc > 1.5);
    if (uc < 1.5)
        uniCtL->setText("Voice");
    else
        uniCtL->setText("Voices");

    auto me = editor.patchCopy.output.mpeActive.value > 0.5;
    mpeRange->setEnabled(me);
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
    pl.value = plVal;
    editor.uiToAudio.push({Synth::UIToAudioMsg::Action::SET_PARAM, pl.meta.id, (float)plVal});
    editor.flushOperator();

    voiceLimit->setLabel(std::to_string(getPolyLimit()));
    voiceLimit->repaint();
}

} // namespace baconpaul::six_sines::ui