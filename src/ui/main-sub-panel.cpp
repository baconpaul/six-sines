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

#include "main-sub-panel.h"
#include "ui-constants.h"

namespace baconpaul::six_sines::ui
{
MainSubPanel::MainSubPanel(SixSinesEditor &e) : HasEditor(e), DAHDSRComponents()
{
    setupDAHDSR(e, e.patchCopy.output);

    createComponent(editor, *this, e.patchCopy.output.velSensitivity, velSen, velSenD);
    addAndMakeVisible(*velSen);
    velSenL = std::make_unique<jcmp::Label>();
    velSenL->setText("Amp");
    addAndMakeVisible(*velSenL);

    velTitle = std::make_unique<RuledLabel>();
    velTitle->setText("Sens");
    addAndMakeVisible(*velTitle);

    createComponent(editor, *this, e.patchCopy.output.bendUp, bUp, bUpD);
    createComponent(editor, *this, e.patchCopy.output.bendDown, bDn, bDnD);
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

    createComponent(editor, *this, e.patchCopy.output.playMode, playMode, playModeD);
    playMode->direction = sst::jucegui::components::MultiSwitch::VERTICAL;
    addAndMakeVisible(*playMode);

    createComponent(editor, *this, e.patchCopy.output.portaTime, portaTime, portaTimeD);
    addAndMakeVisible(*portaTime);
    portaL = std::make_unique<jcmp::Label>();
    portaL->setText("Porta");
    addAndMakeVisible(*portaL);
    portaTime->setEnabled(false);
    portaL->setEnabled(false);
    setPortaEnable();

    auto op = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setPortaEnable();
    };

    editor.componentRefreshByID[e.patchCopy.output.playMode.meta.id] = op;
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
    editor.componentRefreshByID[e.patchCopy.output.defaultTrigger.meta.id] =
        [w = juce::Component::SafePointer(this)]()
    {
        if (w)
        {
            w->setTriggerButtonLabel();
        }
    };
    addAndMakeVisible(*triggerButton);
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
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, bendTitle);
    auto bbx = juce::Rectangle<int>(depx, depy + uicTitleLabelHeight, uicKnobSize + 2 * xtraW,
                                    uicLabelHeight);
    bUpL->setBounds(bbx.withWidth(18));
    bUp->setBounds(bbx.withTrimmedLeft(18 + uicMargin));
    bbx = bbx.translated(0, uicLabelHeight + uicMargin);

    auto pmw{14};
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

    positionKnobAndLabel(bbx.getX() + xtraW, bbx.getY() + uicMargin, portaTime, portaL);
}

void MainSubPanel::setTriggerButtonLabel()
{
    auto v = (int)std::round(editor.patchCopy.output.defaultTrigger.value);
    switch (v)
    {
    case 1:
        triggerButton->setLabel("On Voice");
        break;
    case 2:
        triggerButton->setLabel("On Key");
        break;
    default:
    case 0:
        triggerButton->setLabel("On Gated");
        break;
    }
}

void MainSubPanel::showTriggerButtonMenu()
{
    auto tmv = (int)std::round(editor.patchCopy.output.defaultTrigger.value);

    auto genSet = [w = juce::Component::SafePointer(asComp())](int nv)
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
    p.addItem("On Gated", true, tmv == 0, genSet(0));
    p.addItem("On New Voice", true, tmv == 1, genSet(1));
    p.addItem("On Key Press", true, tmv == 2, genSet(2));

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&asComp()->editor));
}

void MainSubPanel::setPortaEnable()
{
    auto vm = editor.patchCopy.output.playMode.value;
    auto en = vm > 0.5;
    portaL->setEnabled(en);
    portaTime->setEnabled(en);
    repaint();
}

} // namespace baconpaul::six_sines::ui