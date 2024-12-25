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
    const Param &wf;
    SinTable st;
    WavPainter(const Param &w) : wf(w) {}

    void paint(juce::Graphics &g)
    {
        st.setWaveForm((SinTable::WaveForm)std::round(wf.value));
        uint32_t phase{0};
        int nPixels{getWidth()};
        auto dPhase = (1 << 27) / (nPixels - 1);
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

    createComponent(editor, *this, sn.envLfoSum, lfoMul, lfoMulD);
    addAndMakeVisible(*lfoMul);
    lfoMul->direction = jcmp::MultiSwitch::VERTICAL;

    modTitle = std::make_unique<RuledLabel>();
    modTitle->setText("Depth");
    addAndMakeVisible(*modTitle);

    wavTitle = std::make_unique<RuledLabel>();
    wavTitle->setText("Wave");
    addAndMakeVisible(*wavTitle);

    createComponent(editor, *this, sn.waveForm, wavButton, wavButtonD);
    addAndMakeVisible(*wavButton);
    wavButtonD->onGuiSetValue = [this]() { wavPainter->repaint(); };

    wavPainter = std::make_unique<WavPainter>(sn.waveForm);
    addAndMakeVisible(*wavPainter);

    resized();
}

void SourceSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    auto gh = (pn.getHeight() - 2 * uicPowerButtonSize) / 2;
    lfoMul->setBounds(pn.getX() + uicMargin, pn.getY() + gh, uicPowerButtonSize,
                      2 * uicPowerButtonSize);
    pn = pn.translated(2 * uicMargin + uicPowerButtonSize, 0);
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
    wavPainter->setBounds(depx, depy, uicKnobSize * 2 + uicMargin, uicLabelHeight * 2.5);
}
} // namespace baconpaul::six_sines::ui