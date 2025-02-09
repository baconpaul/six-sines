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
#include <sst/jucegui/layouts/ListLayout.h>

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

    editor.componentRefreshByID[on.portaContinuation.meta.id] =
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

    createComponent(editor, *this, on.unisonSpread, uniSpread, uniSpreadD);
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

    mpeTitle = std::make_unique<jcmp::RuledLabel>();
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

    panicTitle = std::make_unique<jcmp::RuledLabel>();
    panicTitle->setText("Panic");
    panicButton = std::make_unique<jcmp::TextPushButton>();
    panicButton->setLabel("Sound Off");
    panicButton->setOnCallback(
        [this]() { editor.mainToAudio.push({Synth::MainToAudioMsg::PANIC_STOP_VOICES}); });
    addAndMakeVisible(*panicButton);
    addAndMakeVisible(*panicTitle);

    createComponent(editor, *this, editor.patchCopy.output.sampleRateStrategy, srStrat, srStratD);
    addAndMakeVisible(*srStrat);
    createComponent(editor, *this, editor.patchCopy.output.resampleEngine, rsEng, rsEngD);
    addAndMakeVisible(*rsEng);
    srStratLab = std::make_unique<jcmp::RuledLabel>();
    srStratLab->setText("Oversampling");
    addAndMakeVisible(*srStratLab);

    setEnabledState();
}

void PlayModeSubPanel::resized()
{
    namespace jlo = sst::jucegui::layouts;
    auto lo = jlo::HList().at(uicMargin, 0).withAutoGap(2 * uicMargin);

    // Voice Bend and Octave
    auto skinny = uicKnobSize + 30;
    auto vbol = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);

    vbol.add(titleLabelGaplessLayout(voiceLimitL));
    vbol.add(jlo::Component(*voiceLimit).withHeight(uicLabelHeight));
    vbol.add(titleLabelGaplessLayout(bendTitle));

    vbol.add(sideLabel(bUpL, bUp));
    vbol.add(sideLabel(bDnL, bDn));

    vbol.add(titleLabelGaplessLayout(tsposeTitle));
    vbol.add(jlo::Component(*tsposeButton).withHeight(uicLabelHeight));

    lo.add(vbol);

    // Play mode
    auto pml = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);
    pml.add(titleLabelGaplessLayout(playTitle));
    pml.add(jlo::Component(*playMode).withHeight(2 * uicLabelHeight + uicMargin));
    pml.add(jlo::Component(*triggerButton).withHeight(uicLabelHeight));
    pml.add(jlo::Component(*pianoModeButton).withHeight(uicLabelHeight));
    pml.add(jlo::Component(*portaL).withHeight(uicLabelHeight));
    pml.add(jlo::Component(*portaTime).withHeight(uicLabelHeight).insetBy(0, 2));
    pml.add(jlo::Component(*portaContinuationButton).withHeight(uicLabelHeight));
    lo.add(pml);

    // Unison Controls
    auto uml = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);
    uml.add(titleLabelGaplessLayout(uniTitle));
    uml.add(jlo::Component(*uniCt).withHeight(uicLabelHeight));
    uml.add(jlo::Component(*uniCtL).withHeight(uicLabelHeight));
    uml.add(jlo::Component(*uniRPhase).withHeight(uicLabelHeight));
    uml.add(sideLabelSlider(uniSpreadG, uniSpread));
    uml.add(sideLabelSlider(uniPanG, uniPan));

    lo.add(uml);

    auto mpl = jlo::VList().withWidth(skinny).withAutoGap(uicMargin);
    mpl.add(titleLabelGaplessLayout(mpeTitle));
    mpl.add(jlo::Component(*mpeActiveButton).withHeight(uicLabelHeight));
    mpl.add(jlo::Component(*mpeRange).withHeight(uicLabelHeight));
    mpl.add(jlo::Component(*mpeRangeL).withHeight(uicLabelHeight));

    mpl.add(titleLabelGaplessLayout(panicTitle));
    mpl.add(jlo::Component(*panicButton).withHeight(uicLabelHeight));
    lo.add(mpl);

    auto rsl = jlo::VList().withWidth(2 * skinny).withAutoGap(uicMargin);
    rsl.add(titleLabelGaplessLayout(srStratLab));
    rsl.add(jlo::Component(*srStrat).withHeight(uicLabelHeight));
    rsl.add(jlo::Component(*rsEng).withHeight(uicLabelHeight));
    lo.add(rsl);

    lo.doLayout();
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
    auto v = (int)std::round(editor.patchCopy.output.portaContinuation.value);
    switch (v)
    {
    case 0:
        portaContinuationButton->setLabel("OnVoice");
        break;
    case 1:
        portaContinuationButton->setLabel("FromLast");
        break;
    }
}

void PlayModeSubPanel::showPortaContinuationMenu()
{
    auto tmv = (int)std::round(editor.patchCopy.output.portaContinuation.value);

    auto genSet = [w = juce::Component::SafePointer(this)](int nv)
    {
        auto that = w;
        return [nv, that]()
        {
            auto &p = that->editor.patchCopy.output.portaContinuation;
            that->editor.setAndSendParamValue(p, nv);
            that->setPortaContinuationLabel();
        };
    };
    auto p = juce::PopupMenu();
    p.addSectionHeader("Portamento Continuation");
    p.addSectionHeader("Experimental; May change before 1.1");
    p.addSeparator();
    p.addItem("Reset Porta on New Voice", true, tmv == 0, genSet(0));
    p.addItem("Start Porta from Last Release", true, tmv == 1, genSet(1));

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
    editor.setAndSendParamValue(pl, plVal);

    voiceLimit->setLabelAndTitle(std::to_string(getPolyLimit()),
                                 "Voice Limit " + std::to_string(getPolyLimit()));
    voiceLimit->repaint();
}

} // namespace baconpaul::six_sines::ui