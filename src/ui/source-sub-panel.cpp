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

#include "source-sub-panel.h"
#include "source-panel.h"
#include "matrix-panel.h"
#include "self-sub-panel.h"
#include <sst/jucegui/layouts/ListLayout.h>
#include "patch-data-bindings.h"
#include "ui-constants.h"
#include "dsp/sintable.h" // for drawing

namespace baconpaul::six_sines::ui
{
SourceSubPanel::SourceSubPanel(SixSinesEditor &e) : HasEditor(e) { setSelectedIndex(0); };
SourceSubPanel::~SourceSubPanel() {}

struct WavPainter : juce::Component
{
    const Param &wf, &ph;
    SixSinesEditor &editor;
    SinTable st;
    WavPainter(const Param &w, const Param &p, SixSinesEditor &e) : wf(w), ph(p), editor(e) {}

    void paint(juce::Graphics &g)
    {
        auto labelCol = editor.style()->getColour(jcmp::base_styles::BaseLabel::styleClass,
                                                  jcmp::base_styles::BaseLabel::labelcolor);
        auto wfVal = (SinTable::WaveForm)std::round(wf.value);
        if (wfVal == SinTable::AUDIO_IN)
        {
            // Draw a simple "audio in" indicator — two horizontal lines suggesting a signal
            g.setColour(labelCol.withAlpha(0.5f));
            auto h = getHeight();
            auto w = getWidth();
            auto cy = h / 2;
            g.drawLine(0, cy, w, cy, 1.f);
            g.setColour(labelCol);
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.f)));
            g.drawFittedText("Audio In", getLocalBounds(), juce::Justification::centred, 1);
            return;
        }
        st.setWaveForm(wfVal);
        uint32_t phase{0};
        phase += (1 << 26) * ph.value;
        int nPixels{getWidth()};
        auto dPhase = (1 << 26) / (nPixels - 1);
        auto p = juce::Path();
        auto h = getHeight() - 2;
        auto ho = 1;
        for (int i = 0; i < nPixels; ++i)
        {
            auto sv = st.at(phase);
            auto x = i;
            auto y = (1 - (sv + 1) * 0.5) * h + ho;
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
            phase += dPhase;
        }
        g.setColour(labelCol);
        g.strokePath(p, juce::PathStrokeType(1));
    }
};

void SourceSubPanel::setSelectedIndex(size_t idx)
{
    index = idx;
    repaint();

    removeAllChildren();

    auto &sn = editor.patchCopy.sourceNodes[idx];
    setupDAHDSR(editor, sn);
    setupLFO(editor, sn);
    setupModulation(editor, sn);

    auto travidx{400};
    auto traverse = [&travidx](auto &c)
    { sst::jucegui::component_adapters::setTraversalId(c.get(), travidx++); };

    createComponent(editor, *this, sn.envToRatio, envToRatio, envToRatioD);
    createComponent(editor, *this, sn.envToRatioFine, envToRatioFine, envToRatioFineD);
    envToRatioL = std::make_unique<jcmp::Label>();
    envToRatioL->setText("Coarse");
    envToRatioFineL = std::make_unique<jcmp::Label>();
    envToRatioFineL->setText("Fine");
    addAndMakeVisible(*envToRatioL);
    addAndMakeVisible(*envToRatio);
    addAndMakeVisible(*envToRatioFine);
    addAndMakeVisible(*envToRatioFineL);
    traverse(envToRatio);
    traverse(envToRatioFine);

    createComponent(editor, *this, sn.lfoToRatio, lfoToRatio, lfoToRatioD);
    createComponent(editor, *this, sn.lfoToRatioFine, lfoToRatioFine, lfoToRatioFineD);
    lfoToRatioL = std::make_unique<jcmp::Label>();
    lfoToRatioL->setText("Coarse");
    lfoToRatioFineL = std::make_unique<jcmp::Label>();
    lfoToRatioFineL->setText("Fine");
    addAndMakeVisible(*lfoToRatioL);
    addAndMakeVisible(*lfoToRatio);
    addAndMakeVisible(*lfoToRatioFine);
    addAndMakeVisible(*lfoToRatioFineL);
    traverse(lfoToRatio);
    traverse(lfoToRatioFine);

    modTitle = std::make_unique<jcmp::RuledLabel>();
    modTitle->setText("Env Depth");
    addAndMakeVisible(*modTitle);

    lfoModTitle = std::make_unique<jcmp::RuledLabel>();
    lfoModTitle->setText("LFO Depth");
    addAndMakeVisible(*lfoModTitle);

    wavTitle = std::make_unique<jcmp::RuledLabel>();
    wavTitle->setText("Wave");
    addAndMakeVisible(*wavTitle);

    wavPainter = std::make_unique<WavPainter>(sn.waveForm, sn.startingPhase, editor);
    addAndMakeVisible(*wavPainter);

    createComponent(editor, *this, sn.waveForm, wavButton, wavButtonD);
    wavButtonD->includeAudioIn = (idx == 0);
    addAndMakeVisible(*wavButton);
    wavButtonD->onGuiSetValue = [this]()
    {
        wavPainter->repaint();
        wavButton->repaint();
        setEnabledState();
    };
    wavButton->onPopupMenu = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->showWaveformPopup();
    };
    traverse(wavButton);

    createComponent(editor, *this, sn.startingPhase, startingPhase, startingPhaseD);
    addAndMakeVisible(*startingPhase);
    traverse(startingPhase);
    startingPhaseD->onGuiSetValue = [w = juce::Component::SafePointer(this)]()
    {
        if (!w)
            return;
        w->wavPainter->repaint();
    };

    startingPhaseL = std::make_unique<jcmp::Label>();
    startingPhaseL->setText(std::string() + u8"\U000003C6");
    addAndMakeVisible(*startingPhaseL);

    keyTrackTitle = std::make_unique<jcmp::RuledLabel>();
    keyTrackTitle->setText("Pitch");
    addAndMakeVisible(*keyTrackTitle);

    createComponent(editor, *this, sn.keyTrack, keyTrack, keyTrackD);
    keyTrack->setLabel("KeyTrak");
    addAndMakeVisible(*keyTrack);
    traverse(keyTrack);

    createComponent(editor, *this, sn.keyTrackValueIsLow, keyTrackLow, keyTrackLowD);
    keyTrackLow->setLabel("Lo");
    addAndMakeVisible(*keyTrackLow);
    traverse(keyTrackLow);

    createComponent(editor, *this, sn.keyTrackValue, keyTrackValue, keyTrackValueD);
    addAndMakeVisible(*keyTrackValue);
    traverse(keyTrackValue);

    createRescaledComponent(editor, *this, sn.keyTrackLowFrequencyValue, keyTrackLowValue,
                            keyTrackLowValueD);
    addChildComponent(*keyTrackLowValue);

    auto op = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setEnabledState();
    };

    keyTrackD->onGuiSetValue = op;
    keyTrackLowD->onGuiSetValue = op;
    editor.componentRefreshByID[sn.keyTrack.meta.id] = op;

    createComponent(editor, *this, sn.octTranspose, tsposeButton, tsposeButtonD);
    addAndMakeVisible(*tsposeButton);
    traverse(tsposeButton);

    tsposeButtonL = std::make_unique<jcmp::Label>();
    tsposeButtonL->setText("Octave");
    addAndMakeVisible(*tsposeButtonL);

    unisonBehaviorB = std::make_unique<jcmp::TextPushButton>();
    unisonBehaviorB->setLabel("Unison");
    unisonBehaviorB->setOnCallback(
        [w = juce::Component::SafePointer(this)]()
        {
            if (w)
                w->showUnisonFeaturesMenu();
        });
    addAndMakeVisible(*unisonBehaviorB);
    traverse(unisonBehaviorB);

    setEnabledState();

    resized();
}

void SourceSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    pn = pn.translated(uicMargin, 0);
    auto r = layoutLFOAt(pn.getX(), p.getY());

    namespace jlo = sst::jucegui::layouts;
    auto lo = jlo::HList().at(r.getX() + uicMargin, r.getY()).withAutoGap(uicMargin * 2);

    auto kl = jlo::VList().withWidth(uicKnobSize * 2 + uicMargin * 2).withAutoGap(uicMargin);
    kl.add(titleLabelGaplessLayout(modTitle));

    auto hl = jlo::HList().withAutoGap(uicMargin).withHeight(uicLabeledKnobHeight);
    hl.add(labelKnobLayout(envToRatio, envToRatioL));
    hl.add(labelKnobLayout(envToRatioFine, envToRatioFineL));
    kl.add(hl);

    kl.add(titleLabelGaplessLayout(lfoModTitle));

    hl = jlo::HList().withAutoGap(uicMargin).withHeight(uicLabeledKnobHeight);
    hl.add(labelKnobLayout(lfoToRatio, lfoToRatioL));
    hl.add(labelKnobLayout(lfoToRatioFine, lfoToRatioFineL));
    kl.add(hl);

    lo.add(kl);

    auto ktl = jlo::VList().withWidth(uicKnobSize * 2 + uicMargin + 36).withAutoGap(uicMargin);
    ktl.add(titleLabelGaplessLayout(keyTrackTitle));

    auto sktcol = jlo::HList().withAutoGap(uicMargin).withHeight(uicLabeledKnobHeight);

    auto c1 = jlo::VList().withWidth(uicKnobSize + 18).withAutoGap(uicMargin);
    c1.add(jlo::Component(*tsposeButton).withHeight(uicLabelHeight));
    c1.add(jlo::Component(*tsposeButtonL).withHeight(uicLabelHeight));
    c1.add(jlo::Component(*unisonBehaviorB).withHeight(uicLabelHeight));
    sktcol.add(c1);

    auto c2 = jlo::VList().withWidth(uicKnobSize + 18).withAutoGap(uicMargin);
    c2.add(jlo::Component(*keyTrack).withHeight(uicLabelHeight));

    // We have to get a bit crunched to get this in
    namespace jlo = sst::jucegui::layouts;
    auto ul = jlo::HList().withHeight(uicLabelHeight);
    ul.add(jlo::Component(*keyTrackLow).withWidth(20));
    ul.addGap(1);
    ul.add(jlo::Component(*keyTrackValue).insetBy(0, 2).expandToFill());
    c2.add(ul);
    sktcol.add(c2);

    ktl.add(sktcol);

    ktl.add(titleLabelGaplessLayout(wavTitle));
    ktl.add(jlo::Component(*wavButton).withHeight(uicLabelHeight));
    ktl.add(sideLabelSlider(startingPhaseL, startingPhase));
    ktl.add(jlo::Component(*wavPainter).withHeight(uicLabelHeight * 1.8));

    lo.add(ktl);

    lo.doLayout();

    // The keytrack low can sub in for the keytrack
    keyTrackLowValue->setBounds(keyTrackValue->getBounds());

    layoutModulation(p);
}

void SourceSubPanel::setEnabledState()
{
    auto &sn = editor.patchCopy.sourceNodes[index];
    auto isAudioIn = ((int)std::round(sn.waveForm.value) == SinTable::AUDIO_IN);

    auto ekt = sn.keyTrack.value < 0.5;
    auto ktl = sn.keyTrackValueIsLow > 0.5;
    keyTrackValue->setEnabled(ekt && !ktl && !isAudioIn);
    keyTrackValue->setVisible(!ktl);

    keyTrackLowValue->setEnabled(ekt && ktl && !isAudioIn);
    keyTrackLowValue->setVisible(ktl);

    keyTrackLow->setEnabled(ekt && !isAudioIn);
    tsposeButton->setEnabled(!ekt && !isAudioIn);

    // When AUDIO_IN: env/LFO depth knobs and ratio sliders are inactive
    envToRatio->setEnabled(!isAudioIn);
    envToRatioFine->setEnabled(!isAudioIn);
    lfoToRatio->setEnabled(!isAudioIn);
    lfoToRatioFine->setEnabled(!isAudioIn);
    startingPhase->setEnabled(!isAudioIn);

    // DAHDSR envelope controls
    for (int i = 0; i < DAHDSRComponents::nels; ++i)
        slider[i]->setEnabled(!isAudioIn);
    for (int i = 0; i < DAHDSRComponents::nShape; ++i)
        shapes[i]->setEnabled(!isAudioIn);
    triggerButton->setEnabled(!isAudioIn);

    // LFO controls
    rate->setEnabled(!isAudioIn);
    deform->setEnabled(!isAudioIn);
    phase->setEnabled(!isAudioIn);
    shape->setEnabled(!isAudioIn);
    tempoSync->setEnabled(!isAudioIn);
    bipolar->setEnabled(!isAudioIn);
    isEnv->setEnabled(!isAudioIn);

    // Modulation slots
    for (int i = 0; i < numModsPer; ++i)
    {
        depthSlider[i]->setEnabled(!isAudioIn);
        sourceMenu[i]->setEnabled(!isAudioIn);
        targetMenu[i]->setEnabled(!isAudioIn);
    }

    // Notify source/matrix panels to grey out ratio knob and feedback knob
    editor.sourcePanel->updateOpEnabledState(index);
    editor.matrixPanel->updateSelfKnobState(index);

    // If op1's feedback sub-panel is currently open, update its enabled state too
    if (index == 0 && editor.selfSubPanel->isVisible())
        editor.selfSubPanel->setEnabledState();

    unisonBehaviorB->setEnabled(editor.patchCopy.output.unisonCount > 1);
    repaint();
    editor.repaint();
}

void SourceSubPanel::showWaveformPopup()
{
    auto &sn = editor.patchCopy.sourceNodes[index];
    auto wfid = sn.waveForm.meta.id;
    auto currentVal = (int)std::round(sn.waveForm.value);

    auto p = juce::PopupMenu();
    p.addSectionHeader("Op " + std::to_string(index + 1) + " Waveform");
    p.addSeparator();

    auto addEntry = [&](const WaveformMenuEntry &entry)
    {
        if (entry.isSeparator)
        {
            p.addSeparator();
        }
        else
        {
            auto val = entry.waveformValue;
            p.addItem(entry.label, true, currentVal == val,
                      [val, wfid, w = juce::Component::SafePointer(this)]()
                      {
                          if (!w)
                              return;
                          w->editor.setAndSendParamValue(wfid, val);
                          w->wavButtonD->onGuiSetValue();
                      });
        }
    };

    for (int i = 0; i < waveformMenuBaseCount; ++i)
        addEntry(waveformMenuBase[i]);

    if (wavButtonD->includeAudioIn)
        for (int i = 0; i < waveformMenuAudioInCount; ++i)
            addEntry(waveformMenuAudioIn[i]);

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor),
                    makeMenuAccessibleButtonCB(wavButton.get()));
}

void SourceSubPanel::showUnisonFeaturesMenu()
{
    if (editor.patchCopy.output.unisonCount < 1.5)
        return;

    auto p = juce::PopupMenu();
    auto canCenter = (int)(editor.patchCopy.output.unisonCount.value) % 2;
    auto upart = (int)editor.patchCopy.sourceNodes[index].unisonParticipation.value;
    auto upid = editor.patchCopy.sourceNodes[index].unisonParticipation.meta.id;
    p.addSectionHeader("Op " + std::to_string(index + 1) + " Unison Behavior");
    p.addSeparator();
    p.addItem("Participates in Tuning", true, (upart & 1),
              [upid, upart, w = juce::Component::SafePointer(this)]()
              {
                  if (!w)
                      return;

                  auto onoff = upart & 1;
                  auto nv = (upart & (~1)) | (onoff ? 0 : 1);
                  w->editor.setAndSendParamValue(upid, nv);
              });
    p.addItem("Participates in Pan", true, (upart & 2),
              [upid, upart, w = juce::Component::SafePointer(this)]()
              {
                  if (!w)
                      return;

                  auto onoff = upart & 2;
                  auto nv = (upart & (~2)) | (onoff ? 0 : 2);
                  w->editor.setAndSendParamValue(upid, nv);
              });
    p.addSeparator();
    auto u2m = (int)editor.patchCopy.sourceNodes[index].unisonToMain.value;
    auto u2mid = editor.patchCopy.sourceNodes[index].unisonToMain.meta.id;

    p.addItem("All Voices to Main", true, u2m == 0,
              [w = juce::Component::SafePointer(this), u2mid]()
              {
                  if (w)
                      w->editor.setAndSendParamValue(u2mid, 0);
              });
    p.addItem("Center Voice Only to Main", canCenter, u2m == 1,
              [w = juce::Component::SafePointer(this), u2mid]()
              {
                  if (w)
                      w->editor.setAndSendParamValue(u2mid, 1);
              });
    p.addItem("Side Voices Only to Main", canCenter, u2m == 2,
              [w = juce::Component::SafePointer(this), u2mid]()
              {
                  if (w)
                      w->editor.setAndSendParamValue(u2mid, 2);
              });
    p.addItem("No Voices to Main", true, u2m == 3,
              [w = juce::Component::SafePointer(this), u2mid]()
              {
                  if (w)
                      w->editor.setAndSendParamValue(u2mid, 3);
              });
    p.addSeparator();
    auto u2o = (int)editor.patchCopy.sourceNodes[index].unisonToOpOut.value;
    auto u2oid = editor.patchCopy.sourceNodes[index].unisonToOpOut.meta.id;
    p.addItem("All Voices to Op " + std::to_string(index + 1) + "Out", true, u2o == 0,
              [w = juce::Component::SafePointer(this), u2oid]()
              {
                  if (w)
                      w->editor.setAndSendParamValue(u2oid, 0);
              });
    p.addItem("Center Voice Only to Op " + std::to_string(index + 1) + " Out", canCenter, u2o == 1,
              [w = juce::Component::SafePointer(this), u2oid]()
              {
                  if (w)
                      w->editor.setAndSendParamValue(u2oid, 1);
              });
    p.addItem("Side Voices Only to Op " + std::to_string(index + 1) + " Out", canCenter, u2o == 2,
              [w = juce::Component::SafePointer(this), u2oid]()
              {
                  if (w)
                      w->editor.setAndSendParamValue(u2oid, 2);
              });

    p.showMenuAsync(juce::PopupMenu::Options().withParentComponent(&editor),
                    makeMenuAccessibleButtonCB(unisonBehaviorB.get()));
}

IMPLEMENTS_CLIPBOARD_SUPPORT(SourceSubPanel, sourceNodes[index], Clipboard::SOURCE_FULLNODE)

} // namespace baconpaul::six_sines::ui