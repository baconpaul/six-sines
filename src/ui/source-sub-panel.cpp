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
    SinTable st;
    WavPainter(const Param &w, const Param &p) : wf(w), ph(p) {}

    void paint(juce::Graphics &g)
    {
        st.setWaveForm((SinTable::WaveForm)std::round(wf.value));
        uint32_t phase{0};
        phase += (1 << 26) * ph.value;
        int nPixels{getWidth()};
        auto dPhase = (1 << 26) / (nPixels - 1);
        auto p = juce::Path();
        for (int i = 0; i < nPixels; ++i)
        {
            auto sv = st.at(phase);
            auto x = i;
            auto y = (1 - (sv + 1) * 0.5) * getHeight();
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
            phase += dPhase;
        }
        g.setColour(juce::Colours::white);
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

    createComponent(editor, *this, sn.envToRatio, envToRatio, envToRatioD);
    envToRatioL = std::make_unique<jcmp::Label>();
    envToRatioL->setText("Env");
    addAndMakeVisible(*envToRatioL);
    addAndMakeVisible(*envToRatio);

    createComponent(editor, *this, sn.lfoToRatio, lfoToRatio, lfoToRatioD);
    addAndMakeVisible(*lfoToRatio);
    lfoToRatioL = std::make_unique<jcmp::Label>();
    lfoToRatioL->setText("LFO");
    addAndMakeVisible(*lfoToRatioL);

    modTitle = std::make_unique<RuledLabel>();
    modTitle->setText("Depth");
    addAndMakeVisible(*modTitle);

    wavTitle = std::make_unique<RuledLabel>();
    wavTitle->setText("Wave");
    addAndMakeVisible(*wavTitle);

    createComponent(editor, *this, sn.waveForm, wavButton, wavButtonD);
    addAndMakeVisible(*wavButton);
    wavButtonD->onGuiSetValue = [this]()
    {
        wavPainter->repaint();
        wavButton->repaint();
    };

    wavPainter = std::make_unique<WavPainter>(sn.waveForm, sn.startingPhase);
    addAndMakeVisible(*wavPainter);

    keyTrackTitle = std::make_unique<RuledLabel>();
    keyTrackTitle->setText("KeyTrak");
    addAndMakeVisible(*keyTrackTitle);

    createComponent(editor, *this, sn.keyTrack, keyTrack, keyTrackD);
    keyTrack->setLabel("Active");
    addAndMakeVisible(*keyTrack);

    createComponent(editor, *this, sn.keyTrackValue, keyTrackValue, keyTrackValueD);
    addAndMakeVisible(*keyTrackValue);
    keyTrackValueLL = std::make_unique<jcmp::Label>();
    keyTrackValueLL->setText("f @ r=1");
    addAndMakeVisible(*keyTrackValueLL);

    createComponent(editor, *this, sn.startingPhase, startingPhase, startingPhaseD);
    addAndMakeVisible(*startingPhase);
    /*
     startingPhaseD->onGuiSetValue = [this]()
    {
        wavPainter->repaint();
        wavButton->repaint();
    };
    */
    auto op = [w = juce::Component::SafePointer(this)]()
    {
        if (w)
            w->setEnabledState();
    };
    keyTrackD->onGuiSetValue = op;
    editor.componentRefreshByID[sn.keyTrack.meta.id] = op;

    createComponent(editor, *this, sn.octTranspose, tsposeButton, tsposeButtonD);
    addAndMakeVisible(*tsposeButton);

    tsposeButtonL = std::make_unique<RuledLabel>();
    tsposeButtonL->setText("Octave");
    addAndMakeVisible(*tsposeButtonL);

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

    auto depx = r.getX() + 2 * uicMargin;
    auto depy = r.getY();
    positionTitleLabelAt(depx, depy, uicKnobSize * 2 + uicMargin, modTitle);

    depy += uicTitleLabelHeight;
    positionKnobAndLabel(depx, depy, envToRatio, envToRatioL);
    depx += uicKnobSize + uicMargin;
    positionKnobAndLabel(depx, depy, lfoToRatio, lfoToRatioL);

    depx = r.getX() + 2 * uicMargin;
    depy += uicLabeledKnobHeight + uicMargin;

    positionTitleLabelAt(depx, depy, uicKnobSize * 2 + uicMargin, wavTitle);
    depy += uicTitleLabelHeight;
    wavButton->setBounds(depx, depy, uicKnobSize * 2 + uicMargin, uicLabelHeight);
    depy += uicLabelHeight + uicMargin;
    startingPhase->setBounds(depx, depy, uicKnobSize * 2 + uicMargin, uicLabelHeight - uicMargin);
    depy += uicLabelHeight;
    wavPainter->setBounds(depx, depy, uicKnobSize * 2 + uicMargin, uicLabelHeight * 1.8);

    depx += 2 * uicKnobSize + 3 * uicMargin;
    depy = r.getY();
    auto xtraW = 8;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, keyTrackTitle);
    depy += uicLabelHeight + uicMargin;
    auto bbx = juce::Rectangle<int>(depx, depy, uicKnobSize + 2 * xtraW, uicLabelHeight);
    keyTrack->setBounds(bbx);
    bbx = bbx.translated(0, uicLabelHeight + 2 * uicMargin);
    positionKnobAndLabel(depx + xtraW, bbx.getY(), keyTrackValue, keyTrackValueLL);

    depy = bbx.getY() + uicLabeledKnobHeight + uicMargin;
    positionTitleLabelAt(depx, depy, uicKnobSize + 2 * xtraW, tsposeButtonL);
    depy += uicTitleLabelHeight;
    tsposeButton->setBounds(depx, depy, uicKnobSize + 2 * xtraW, uicLabelHeight);
    layoutModulation(p);
}

void SourceSubPanel::setEnabledState()
{
    auto &sn = editor.patchCopy.sourceNodes[index];
    auto ekt = sn.keyTrack.value < 0.5;
    keyTrackValue->setEnabled(ekt);
    keyTrackValueLL->setEnabled(ekt);
    tsposeButton->setEnabled(!ekt);
    repaint();
}

} // namespace baconpaul::six_sines::ui