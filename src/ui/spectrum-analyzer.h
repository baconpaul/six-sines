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

#ifndef BACONPAUL_SIX_SINES_UI_SPECTRUM_ANALYZER_H
#define BACONPAUL_SIX_SINES_UI_SPECTRUM_ANALYZER_H

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "synth/synth.h"
#include "ui-defaults.h"

namespace baconpaul::six_sines::ui
{

struct SpectrumAnalyzerComponent : juce::Component, private juce::AsyncUpdater
{
    static constexpr int spectrogramColumns = 256;

    struct Mode
    {
        int fftOrder;
        int hopSize;
        const char *label;
    };
    static constexpr std::array<Mode, 4> modes{{
        {11, 512, "2048 / 512"},
        {12, 512, "4096 / 512"},
        {13, 512, "8192 / 512"},
        {14, 1024, "16384 / 1024"},
    }};
    static constexpr int defaultModeIdx = 2;

    static constexpr std::array<int, 3> scopeScales{1, 2, 3};
    static constexpr int defaultScopeScale = 1;

    SpectrumAnalyzerComponent(Synth::audioOutputQueue_t &ring, float hostSampleRate,
                              defaultsProvder_t *defaults = nullptr);
    ~SpectrumAnalyzerComponent() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;

    // Pushed by the editor when the host sample rate changes; rebuilds buffers if needed.
    void setHostSampleRate(float sr);

  private:
    void handleAsyncUpdate() override;
    void analysisThreadMain();

    // Tear down the worker, resize buffers, rebuild FFT/window for the given mode,
    // then restart the worker. Called from the UI thread.
    void applyMode(int modeIdx);
    void setScopeScale(int s);

    float magInRange(const float *col, float b0, float b1) const;

    Synth::audioOutputQueue_t &ring;
    float hostSampleRate{48000.f};
    defaultsProvder_t *defaults{nullptr};
    int scopeScale{defaultScopeScale};

    int modeIdx{defaultModeIdx};
    int lastSavedModeIdx{-1};
    int fftOrder{};
    int fftSize{};
    int hopSize{};
    int numBins{};

    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> hann;

    // owned by the analysis thread
    std::vector<float> windowBuffer;
    int windowFill{0};
    std::vector<float> fftScratch;
    std::vector<float> col; // reused per-frame magnitude scratch

    // Scope (raw oscilloscope), 20 ms window with positive zero-crossing retrigger.
    int scopeFrameLen{};
    std::vector<float> scopeBuilding; // analysis-thread only
    int scopeFill{0};
    bool scopeWaitingForTrigger{false};
    float scopePrev{0.f};

    // shared between analysis thread (writer) and UI thread (reader)
    std::mutex snapshotMutex;
    std::vector<float> columns; // flat: spectrogramColumns * numBins
    int nextColumn{0};
    std::vector<float> latestLine;
    std::vector<float> peakLine;
    std::vector<float> scopeFrame; // latest complete frame
    // Bumped under the lock when columns/latestLine/peakLine change. Paint compares this
    // against lastPaintedSpectrumVersion to skip the spectrogram-image rebuild when
    // nothing new has arrived (e.g. a paint triggered by hover or resize).
    std::atomic<uint32_t> spectrumVersion{0};

    // UI-thread paint scratch — sized in applyMode so paint never allocates.
    std::vector<float> paintCols;
    std::vector<float> paintLine;
    std::vector<float> paintPeak;
    std::vector<float> paintScope;
    int paintNextColumn{0};
    juce::Image cachedSpecImage;
    int cachedSpecImageH{0};
    uint32_t lastPaintedSpectrumVersion{(uint32_t)-1};

    // Peak-hold decay per analysis frame; depends on hop period and target half-life.
    float peakDecay{0.995f};

    std::thread analysisThread;
    std::atomic<bool> running{false};
};

struct SpectrumAnalyzerWindow : juce::DocumentWindow
{
    explicit SpectrumAnalyzerWindow(std::unique_ptr<SpectrumAnalyzerComponent> content);
    void closeButtonPressed() override;

    std::function<void()> onCloseRequested;
};

} // namespace baconpaul::six_sines::ui

#endif // BACONPAUL_SIX_SINES_UI_SPECTRUM_ANALYZER_H
