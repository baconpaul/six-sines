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
#include "dsp/remap_functions.h"

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
        auto gridCol = labelCol.withAlpha(0.3f);
        auto wavCol = editor.style()->getColour(jcmp::base_styles::ValueBearing::styleClass,
                                                jcmp::base_styles::ValueBearing::value);
        auto bg = editor.style()->getColour(jcmp::base_styles::PushButton::styleClass,
                                            jcmp::base_styles::PushButton::fill);
        g.fillAll(bg);
        g.setColour(gridCol);
        g.drawRect(getLocalBounds(), 1.f);

        auto wfVal = (SinTable::WaveForm)std::round(wf.value);
        if (wfVal == SinTable::AUDIO_IN)
        {
            // Draw a simple "audio in" indicator — two horizontal lines suggesting a signal
            g.setColour(labelCol);
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(20.f)));
            g.drawFittedText("Audio In", getLocalBounds(), juce::Justification::centred, 1);
            return;
        }
        g.setColour(gridCol);
        g.drawHorizontalLine(getHeight() / 2, 0, getWidth());

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
            auto sv = st.at(phase) * 0.98;
            auto x = i;
            auto y = (1 - (sv + 1) * 0.5) * h + ho;
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
            phase += dPhase;
        }
        g.setColour(wavCol);
        g.strokePath(p, juce::PathStrokeType(1.5));
    }
};

/*
 * PDWavPainter shows two side-by-side plots: the active phase-remap function on the left,
 * and the current waveform read through that remap on the right. The right plot shares the
 * look-and-feel of WavPainter; the left plot is sized small and visually separated.
 */
struct PDWavPainter : juce::Component
{
    const Param &wf, &ph, &mp, &shp;
    SixSinesEditor &editor;
    SinTable st;

    PDWavPainter(const Param &w, const Param &p, const Param &mParam, const Param &sParam,
                 SixSinesEditor &e)
        : wf(w), ph(p), mp(mParam), shp(sParam), editor(e)
    {
    }

    // Default width devoted to the remap plot on the left. The wave plot takes the rest unless
    // innerWaveWidth is set explicitly (>0), in which case the wave region is fixed and the remap
    // plot fills the remainder.
    static constexpr int remapPlotW{60};
    static constexpr int gapW{6};
    int innerWaveWidth{-1};

    uint32_t doRemap(uint32_t inPhase) const
    {
        using PM = Patch::SourceNode::PhaseMapShape;
        auto pm = static_cast<PM>(static_cast<uint32_t>(std::round(shp.value)));
        auto mv = mp.value;
        auto masked = inPhase & phase::phaseMask;
        switch (pm)
        {
        case PM::SAW:
            return remap::remapSaw(masked, mv);
        case PM::SQUARE:
            return remap::remapSquare(masked, mv);
        case PM::PULSE:
            return remap::remapPulse(masked, mv);
        case PM::DOUBLE:
            return remap::remapDoubleSine(masked, mv);
        case PM::SIN_TO_SQUARE:
            return remap::remapSinToSquare(masked, mv);
        case PM::DOUBLE_SAW:
            return remap::remapDoubleSaw(masked, mv);
        }
        return masked;
    }

    void paint(juce::Graphics &g) override
    {
        auto labelCol = editor.style()->getColour(jcmp::base_styles::BaseLabel::styleClass,
                                                  jcmp::base_styles::BaseLabel::labelcolor);
        auto gridCol = labelCol.withAlpha(0.3f);
        auto wavCol = editor.style()->getColour(jcmp::base_styles::ValueBearing::styleClass,
                                                jcmp::base_styles::ValueBearing::value);
        auto bg = editor.style()->getColour(jcmp::base_styles::PushButton::styleClass,
                                            jcmp::base_styles::PushButton::fill);
        g.fillAll(bg);

        // If innerWaveWidth is set, the wave region is fixed to that width (matched to the upper
        // wave painter) and the remap plot fills whatever's left on the left.
        int waveW = (innerWaveWidth > 0) ? innerWaveWidth : (getWidth() - remapPlotW - gapW);
        int remapW = std::max(0, getWidth() - waveW - gapW);
        auto remapBox = juce::Rectangle<int>(0, 0, remapW, getHeight());
        auto waveBox = juce::Rectangle<int>(remapW + gapW, 0, waveW, getHeight());

        // ===== Left: phase-remap function =====
        g.setColour(gridCol);
        g.drawRect(remapBox, 1);
        // identity reference (diagonal, low-emphasis)
        g.setColour(gridCol.withAlpha(gridCol.getFloatAlpha() * 0.6f));
        g.drawLine(static_cast<float>(remapBox.getX()),
                   static_cast<float>(remapBox.getBottom() - 1),
                   static_cast<float>(remapBox.getRight() - 1),
                   static_cast<float>(remapBox.getY() + 1), 1.f);

        // remap curve y = remap(x, m)
        {
            auto p = juce::Path();
            int nPx = remapBox.getWidth();
            auto innerH = remapBox.getHeight() - 2;
            for (int i = 0; i < nPx - 1; ++i)
            {
                auto inPhase =
                    static_cast<uint32_t>(static_cast<float>(i) / (nPx - 1) * phase::phaseMaxF);
                auto outPhase = doRemap(inPhase);
                float yNorm = static_cast<float>(outPhase) / phase::phaseMaxF;
                auto x = static_cast<float>(remapBox.getX() + i);
                auto y = static_cast<float>(remapBox.getBottom() - 1) - yNorm * innerH;
                if (i == 0)
                    p.startNewSubPath(x, y);
                else
                    p.lineTo(x, y);
            }
            g.setColour(wavCol);
            g.strokePath(p, juce::PathStrokeType(1.5));
        }

        // ===== Right: waveform read through the remap =====
        g.setColour(gridCol);
        g.drawRect(waveBox, 1);

        auto wfVal = (SinTable::WaveForm)std::round(wf.value);
        if (wfVal == SinTable::AUDIO_IN)
        {
            g.setColour(labelCol);
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(20.f)));
            g.drawFittedText("Audio In", waveBox, juce::Justification::centred, 1);
            return;
        }
        g.setColour(gridCol);
        g.drawHorizontalLine(waveBox.getY() + waveBox.getHeight() / 2, waveBox.getX(),
                             waveBox.getRight());

        st.setWaveForm(wfVal);
        uint32_t phs{0};
        phs += (1 << 26) * ph.value;
        int nPixels = waveBox.getWidth();
        auto dPhase = (1 << 26) / (nPixels - 1);
        auto h = waveBox.getHeight() - 2;
        auto ho = waveBox.getY() + 1;
        auto p = juce::Path();
        for (int i = 0; i < nPixels; ++i)
        {
            auto sv = st.at(doRemap(phs)) * 0.98;
            auto x = waveBox.getX() + i;
            auto y = (1 - (sv + 1) * 0.5) * h + ho;
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
            phs += dPhase;
        }
        g.setColour(wavCol);
        g.strokePath(p, juce::PathStrokeType(1.5));
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

    isTargetAvailable = [w = juce::Component::SafePointer(this)](Patch::SourceNode::TargetID tid)
    {
        if (!w)
            return true;
        using EM = Patch::SourceNode::ExtendedMode;
        using TID = Patch::SourceNode::TargetID;
        auto em = static_cast<EM>(static_cast<int>(
            std::round(w->editor.patchCopy.sourceNodes[w->index].extendedModeMode.value)));
        if (tid == TID::EXTEND_M)
            return em == EM::PHASE_REMAP;
        if (tid == TID::EXTEND_N)
            return false;
        return true;
    };

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
    modTitle->setText(std::string() + "Env" + u8"\U00002192");
    addAndMakeVisible(*modTitle);

    lfoModTitle = std::make_unique<jcmp::RuledLabel>();
    lfoModTitle->setText(std::string() + "LFO" + u8"\U00002192");
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
        if (pdWavPainter)
            pdWavPainter->repaint();
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
        if (w->pdWavPainter)
            w->pdWavPainter->repaint();
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
    tsposeButtonL->setText("Oct");
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

    extModeRuleLeft = std::make_unique<jcmp::LineSegment>();
    extModeRuleRight = std::make_unique<jcmp::LineSegment>();
    addAndMakeVisible(*extModeRuleLeft);
    addAndMakeVisible(*extModeRuleRight);

    extModeLabel = std::make_unique<jcmp::Label>();
    extModeLabel->setText("Extended Mode");
    addAndMakeVisible(*extModeLabel);

    createComponent(editor, *this, sn.extendedModeMode, extModeButton, extModeButtonD);
    addAndMakeVisible(*extModeButton);
    traverse(extModeButton);
    auto extModeRefresh = [w = juce::Component::SafePointer(this)]()
    {
        if (!w)
            return;
        w->setExtendedModeVisibility();
        w->resized();
        w->repaint();
    };
    extModeButtonD->onGuiSetValue = extModeRefresh;
    // Also fire on programmatic param changes (e.g. clipboard paste of a full source node)
    editor.componentRefreshByID[sn.extendedModeMode.meta.id] = extModeRefresh;

    comingSoonLabel = std::make_unique<jcmp::Label>();
    comingSoonLabel->setText("Coming Soon");
    addChildComponent(*comingSoonLabel);

    createComponent(editor, *this, sn.phaseMapModeShape, phaseMapShape, phaseMapShapeD);
    addChildComponent(*phaseMapShape);
    traverse(phaseMapShape);

    createComponent(editor, *this, sn.extendedModeM, extM, extMD);
    addChildComponent(*extM);
    traverse(extM);
    extML = std::make_unique<jcmp::Label>();
    extML->setText("M");
    addChildComponent(*extML);

    createComponent(editor, *this, sn.envToExtendedModeM, envToExtM, envToExtMD);
    addChildComponent(*envToExtM);
    traverse(envToExtM);
    envToExtML = std::make_unique<jcmp::Label>();
    envToExtML->setText(std::string() + "Env" + u8"\U00002192" + "M");
    addChildComponent(*envToExtML);

    createComponent(editor, *this, sn.lfoToExtendedModeM, lfoToExtM, lfoToExtMD);
    addChildComponent(*lfoToExtM);
    traverse(lfoToExtM);
    lfoToExtML = std::make_unique<jcmp::Label>();
    lfoToExtML->setText(std::string() + "LFO" + u8"\U00002192" + "M");
    addChildComponent(*lfoToExtML);

    pdWavPainter = std::make_unique<PDWavPainter>(sn.waveForm, sn.startingPhase, sn.extendedModeM,
                                                  sn.phaseMapModeShape, editor);
    addChildComponent(*pdWavPainter);

    // Live-repaint the PD painter when any input it draws from changes.
    auto repaintPD = [w = juce::Component::SafePointer(this)]()
    {
        if (w && w->pdWavPainter)
            w->pdWavPainter->repaint();
    };
    extMD->onGuiSetValue = repaintPD;
    phaseMapShapeD->onGuiSetValue = repaintPD;
    editor.componentRefreshByID[sn.extendedModeM.meta.id] = repaintPD;
    editor.componentRefreshByID[sn.phaseMapModeShape.meta.id] = repaintPD;

    setExtendedModeVisibility();

    setEnabledState();

    resized();
}

void SourceSubPanel::resized()
{
    auto p = getLocalBounds().reduced(uicMargin, 0);
    auto pn = layoutDAHDSRAt(p.getX(), p.getY());
    pn = pn.translated(uicMargin, 0);
    auto r = layoutLFOAt(pn.getX(), p.getY());

    auto depx = p.getX();
    auto depy = std::min(pn.getBottom(), r.getBottom()) + uicMargin;

    namespace jlo = sst::jucegui::layouts;

    // Top level: [Env] | [LFO] | [Pitch] | [Wave]
    auto lo = jlo::HList().at(depx, depy).withAutoGap(uicMargin * 2);

    // Env depth column: title + coarse knob + fine knob stacked
    auto envCol = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    envCol.add(titleLabelGaplessLayout(modTitle));
    envCol.add(labelKnobLayout(envToRatio, envToRatioL).centerInParent());
    envCol.add(labelKnobLayout(envToRatioFine, envToRatioFineL).centerInParent());
    lo.add(envCol);

    // LFO depth column: title + coarse knob + fine knob stacked
    auto lfoCol = jlo::VList().withWidth(uicSubPanelColumnWidth).withAutoGap(uicMargin);
    lfoCol.add(titleLabelGaplessLayout(lfoModTitle));
    lfoCol.add(labelKnobLayout(lfoToRatio, lfoToRatioL).centerInParent());
    lfoCol.add(labelKnobLayout(lfoToRatioFine, lfoToRatioFineL).centerInParent());
    lo.add(lfoCol);

    // Pitch column — a bit wider, 3 vertical rows
    constexpr int pitchColW{(int)(uicSubPanelColumnWidth * 1.2)};
    constexpr int pitchRowH{uicLabelHeight};
    auto pitchCol = jlo::VList().withWidth(pitchColW).withAutoGap(uicMargin);
    pitchCol.add(titleLabelGaplessLayout(keyTrackTitle));

    // Row 1: [Octave label] [tspose jog]
    auto pRow1 = jlo::HList().withHeight(pitchRowH).withAutoGap(uicMargin);
    pRow1.add(jlo::Component(*tsposeButtonL).withWidth(20));
    pRow1.add(jlo::Component(*tsposeButton).expandToFill());
    pitchCol.add(pRow1);

    // Row 2: full-width KeyTrak toggle
    pitchCol.add(jlo::Component(*keyTrack).withHeight(pitchRowH));

    // Row 3: [Lo toggle] [keytrack value slider]
    auto pRow3 = jlo::HList().withHeight(pitchRowH);
    pRow3.add(jlo::Component(*keyTrackLow).withWidth(20));
    pRow3.addGap(1);
    pRow3.add(jlo::Component(*keyTrackValue).insetBy(0, 2).expandToFill());
    pitchCol.add(pRow3);

    // Row 4: full-width Unison button
    pitchCol.add(jlo::Component(*unisonBehaviorB).withHeight(pitchRowH));

    lo.add(pitchCol);

    // Wave column — fills the remaining horizontal space
    int waveColW = p.getRight() - depx - 3 * uicSubPanelColumnWidth - 9 * uicMargin;
    auto waveCol = jlo::VList().withWidth(waveColW).withAutoGap(uicMargin);
    waveCol.add(titleLabelGaplessLayout(wavTitle));
    // Painter fills the remaining height so it lines up with the Env/LFO column bottoms
    constexpr int depthColTotal{uicTitleLabelInnerBox + uicMargin + uicLabeledKnobHeight +
                                uicMargin + uicLabeledKnobHeight};
    constexpr int waveUsedAbove{uicTitleLabelInnerBox + uicMargin + uicLabelHeight + uicMargin +
                                uicLabelHeight + uicMargin};
    constexpr int painterH{depthColTotal - waveUsedAbove - uicMargin * 2};
    waveCol.add(jlo::Component(*wavPainter).withHeight(painterH));
    waveCol.add(jlo::Component(*wavButton).withHeight(uicLabelHeight));
    waveCol.add(sideLabelSlider(startingPhaseL, startingPhase));
    lo.add(waveCol);

    lo.doLayout();

    // The keytrack low can sub in for the keytrack
    keyTrackLowValue->setBounds(keyTrackValue->getBounds());

    // Extended Mode row — spans full width below the 4-column block
    auto extRowY = depy + depthColTotal + uicMargin * 2;
    auto extRow = jlo::HList().at(depx, extRowY).withWidth(p.getWidth()).withHeight(uicLabelHeight);
    extRow.add(jlo::Component(*extModeRuleLeft).expandToFill());
    extRow.addGap(uicMargin);
    extRow.add(jlo::Component(*extModeLabel).withWidth(90));
    extRow.add(jlo::Component(*extModeButton).withWidth(130));
    extRow.addGap(uicMargin);
    extRow.add(jlo::Component(*extModeRuleRight).expandToFill());
    extRow.doLayout();

    // LineSegment needs endpoints set explicitly after its bounds are placed
    auto setRule = [](auto &seg)
    {
        auto b = seg->getBounds();
        seg->setEndpointsAndBounds(b.getX(), b.getCentreY(), b.getRight(), b.getCentreY());
    };
    setRule(extModeRuleLeft);
    setRule(extModeRuleRight);

    // Extended-mode body row — sits between the Extended Mode row and the modulation footer.
    // For PHASE_REMAP we use a two-row block so the PD wave painter can span both rows
    // beside a 2x2 control grid. Other modes still use a single-row block.
    auto bodyY = extRowY + uicLabelHeight + uicMargin * 2;

    using EM = Patch::SourceNode::ExtendedMode;
    auto em = static_cast<EM>(static_cast<int>(std::round(extModeButtonD->getValue())));
    if (em == EM::PHASE_REMAP)
    {
        // Layout:
        //   [ multi ] [ pd wav painter spans                      ]
        //   [ multi ] [ M ] [ Env->M ] [ LFO->M ]
        // Multi switch occupies a single column on the left and spans the full body height.
        // The right side has the PD painter on top (matching the upper painter height) and a
        // row of three knob cells below.
        constexpr int pdPainterHeight = painterH - 15;
        constexpr int cellW = uicKnobSize;
        constexpr int row2H = uicLabeledKnobHeight;
        constexpr int bodyH = pdPainterHeight + uicMargin + row2H;
        auto bodyRect = juce::Rectangle<int>(depx, bodyY, p.getWidth(), bodyH);

        auto knobCell = [](auto &k, auto &l)
        {
            auto cell = jlo::VList().withWidth(cellW);
            cell.add(labelKnobLayout(k, l).centerInParent());
            return cell;
        };

        auto bodyLo = jlo::HList()
                          .at(bodyRect.getX(), bodyRect.getY())
                          .withWidth(bodyRect.getWidth())
                          .withHeight(bodyRect.getHeight())
                          .withAutoGap(uicMargin * 2);

        // Left: multiswitch spans the full body height
        auto leftCol = jlo::VList().withWidth(2 * cellW);
        leftCol.add(jlo::Component(*phaseMapShape).withHeight(bodyH));
        bodyLo.add(leftCol);

        // Right: PD painter on top, knob row beneath
        auto rightCol = jlo::VList().withAutoGap(uicMargin);
        // PD painter inner-wave width is fixed to waveColW (matches the upper painter); any
        // horizontal compression comes off the remap plot, not the wave.
        if (auto *pd = dynamic_cast<PDWavPainter *>(pdWavPainter.get()))
            pd->innerWaveWidth = waveColW;
        rightCol.add(jlo::Component(*pdWavPainter).withHeight(pdPainterHeight));

        auto knobRow = jlo::HList().withHeight(row2H).withAutoGap(uicMargin);
        knobRow.add(knobCell(extM, extML));
        knobRow.add(knobCell(envToExtM, envToExtML));
        knobRow.add(knobCell(lfoToExtM, lfoToExtML));
        rightCol.addGap(uicMargin);
        rightCol.add(knobRow);

        bodyLo.add(rightCol.expandToFill());
        bodyLo.doLayout();
    }
    else if (em == EM::RESONANT_SWEEP || em == EM::REDACTED_1)
    {
        auto bodyRect = juce::Rectangle<int>(depx, bodyY, p.getWidth(), uicLabeledKnobHeight);
        comingSoonLabel->setBounds(bodyRect);
    }

    layoutModulation(p);
}

void SourceSubPanel::setExtendedModeVisibility()
{
    using EM = Patch::SourceNode::ExtendedMode;
    auto em = static_cast<EM>(static_cast<int>(std::round(extModeButtonD->getValue())));

    auto isPhaseRemap = (em == EM::PHASE_REMAP);
    auto isComing = (em == EM::RESONANT_SWEEP || em == EM::REDACTED_1);

    comingSoonLabel->setVisible(isComing);
    phaseMapShape->setVisible(isPhaseRemap);
    extM->setVisible(isPhaseRemap);
    extML->setVisible(isPhaseRemap);
    envToExtM->setVisible(isPhaseRemap);
    envToExtML->setVisible(isPhaseRemap);
    lfoToExtM->setVisible(isPhaseRemap);
    lfoToExtML->setVisible(isPhaseRemap);
    if (pdWavPainter)
        pdWavPainter->setVisible(isPhaseRemap);
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
    lfoSetEnabledState(!isAudioIn);

    // Modulation slots
    for (int i = 0; i < numModsPer; ++i)
    {
        depthSlider[i]->setEnabled(!isAudioIn);
        sourceMenu[i]->setEnabled(!isAudioIn);
        targetMenu[i]->setEnabled(!isAudioIn);
    }

    // Extended-mode controls (disabled wholesale on AUDIO_IN)
    extModeButton->setEnabled(!isAudioIn);
    phaseMapShape->setEnabled(!isAudioIn);
    extM->setEnabled(!isAudioIn);
    envToExtM->setEnabled(!isAudioIn);
    lfoToExtM->setEnabled(!isAudioIn);

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