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

#include "main-sub-panel.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
MainSubPanel::MainSubPanel(SixSinesEditor &e)
    : HasEditor(e), DAHDSRComponents(), ModulationComponents()
{
    auto &on = editor.patchCopy.output;
    voiceTrigerAllowed = false;
    setupDAHDSR(e, on);
    setupModulation(e, on);

    createComponent(editor, *this, on.velSensitivity, velSen, velSenD);
    addAndMakeVisible(*velSen);
    velSenL = std::make_unique<jcmp::Label>();
    velSenL->setText("Amp");
    addAndMakeVisible(*velSenL);

    velTitle = std::make_unique<RuledLabel>();
    velTitle->setText("Sens");
    addAndMakeVisible(*velTitle);

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

    setEnabledState();
};

MainSubPanel::~MainSubPanel() {}

void MainSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto r = layoutDAHDSRAt(p.getX(), p.getY());

    auto depx = r.getX() + 2 * uicMargin;
    auto depy = r.getY();
    auto xtraW = 10;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, velTitle);

    depy += uicTitleLabelHeight;
    positionKnobAndLabel(depx + xtraW, depy, velSen, velSenL);

    depy += uicLabeledKnobHeight + uicMargin;
    auto pmw{14};
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, bendTitle);
    auto bbx = juce::Rectangle<int>(depx, depy + uicTitleLabelHeight, uicKnobSize + 2 * xtraW,
                                    uicLabelHeight);
    bUpL->setBounds(bbx.withWidth(pmw));
    bUp->setBounds(bbx.withTrimmedLeft(pmw + uicMargin));
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);

    bDnL->setBounds(bbx.withWidth(pmw));
    bDn->setBounds(bbx.withTrimmedLeft(pmw + uicMargin));

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
    bbx = bbx.translated(0, uicMargin + uicLabelHeight);
    positionTitleLabelAt(bbx.getX(), bbx.getY(), bbx.getWidth(), tsposeTitle);
    bbx = bbx.translated(0, uicMargin + uicLabelHeight);
    tsposeButton->setBounds(bbx.withHeight(uicLabelHeight));

    layoutModulation(p);
}

void MainSubPanel::setTriggerButtonLabel()
{
    auto v = (int)std::round(editor.patchCopy.output.defaultTrigger.value);
    switch (v)
    {
    case NEW_VOICE:
        triggerButton->setLabel("On Start");
        break;
    case KEY_PRESS:
        triggerButton->setLabel("On Key");
        break;
    default:
    case NEW_GATE:
        triggerButton->setLabel("On Str/Rel");
        break;
    }
}

void MainSubPanel::showTriggerButtonMenu()
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

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&this->editor));
}

void MainSubPanel::setEnabledState()
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

} // namespace baconpaul::six_sines::ui