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

#include "ui/spectrum-analyzer.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <sst/jucegui/components/GlyphPainter.h>
#include <sst/jucegui/style/StyleSheet.h>

namespace baconpaul::six_sines::ui
{

namespace jcmp = sst::jucegui::components;

namespace
{
// dB range for both spectrogram colormap and line plot
constexpr float dbMin = -96.f;
constexpr float dbMax = 0.f;

// Magma-ish 5-stop gradient, lerped at paint time
constexpr std::array<std::array<uint8_t, 3>, 5> colormap = {{
    {0, 0, 4},       // black
    {40, 11, 84},    // deep purple
    {140, 41, 129},  // magenta
    {241, 96, 93},   // coral
    {252, 253, 191}, // pale yellow
}};

juce::Colour colorFor(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    constexpr int n = (int)colormap.size();
    auto x = t * (n - 1);
    auto i = std::min((int)x, n - 2);
    auto f = x - i;
    auto lerp = [f](uint8_t a, uint8_t b) { return (uint8_t)std::round(a + (b - a) * f); };
    return juce::Colour(lerp(colormap[i][0], colormap[i + 1][0]),
                        lerp(colormap[i][1], colormap[i + 1][1]),
                        lerp(colormap[i][2], colormap[i + 1][2]));
}

float magToNorm(float mag)
{
    auto db = 20.f * std::log10(std::max(mag, 1e-12f));
    return (db - dbMin) / (dbMax - dbMin);
}
} // namespace

SpectrumAnalyzerComponent::SpectrumAnalyzerComponent(Synth::audioOutputQueue_t &r, float hostSr,
                                                     defaultsProvder_t *d)
    : ring(r), defaults(d)
{
    if (hostSr > 0.f)
        hostSampleRate = hostSr;
    setOpaque(true);

    alwaysOnTopAdapter = std::make_unique<
        sst::jucegui::component_adapters::DiscreteToValueReference<jcmp::ToggleButton, bool>>(
        alwaysOnTopState);
    auto *btn = alwaysOnTopAdapter->widget.get();
    btn->setGlyph(jcmp::GlyphPainter::PIN);
    btn->setOffGlyph(jcmp::GlyphPainter::UNPIN);
    btn->setTitle("Always on Top");
    addAndMakeVisible(*btn);
    alwaysOnTopAdapter->onValueChanged = [this](bool b)
    {
        if (onAlwaysOnTopChanged)
            onAlwaysOnTopChanged(b);
    };

    setStyle(sst::jucegui::style::StyleSheet::getBuiltInStyleSheet(
        sst::jucegui::style::StyleSheet::DARK));

    // setSize last so the resulting resized() positions the toggle.
    setSize(720, 690);

    int savedMode = defaultModeIdx;
    int savedScale = defaultScopeScale;
    if (defaults)
    {
        savedMode = defaults->getUserDefaultValue(Defaults::spectrumAnalysisMode, defaultModeIdx);
        savedScale = defaults->getUserDefaultValue(Defaults::spectrumScopeScale, defaultScopeScale);
    }
    if (savedMode < 0 || savedMode >= (int)modes.size())
        savedMode = defaultModeIdx;
    if (savedScale < 1 || savedScale > (int)scopeScales.back())
        savedScale = defaultScopeScale;
    scopeScale = savedScale;
    lastSavedModeIdx = savedMode;

    applyMode(savedMode);
}

void SpectrumAnalyzerComponent::setHostSampleRate(float sr)
{
    if (sr <= 0.f)
        return;
    if (std::abs(sr - hostSampleRate) < 1.f)
        return;
    hostSampleRate = sr;
    applyMode(modeIdx);
}

SpectrumAnalyzerComponent::~SpectrumAnalyzerComponent()
{
    running = false;
    if (analysisThread.joinable())
        analysisThread.join();

    ring.unsubscribe();
    ring.clear();
}

void SpectrumAnalyzerComponent::resized()
{
    if (alwaysOnTopAdapter)
    {
        constexpr int sz = 20;
        constexpr int pad = 4;
        alwaysOnTopAdapter->widget->setBounds(getWidth() - sz - pad, pad, sz, sz);
    }
}

void SpectrumAnalyzerComponent::mouseDown(const juce::MouseEvent &e)
{
    if (!e.mods.isPopupMenu())
        return;
    juce::PopupMenu m;
    m.addSectionHeader("Resolution");
    for (int i = 0; i < (int)modes.size(); ++i)
    {
        m.addItem(modes[i].label, true, i == modeIdx,
                  [w = juce::Component::SafePointer(this), i]()
                  {
                      if (w)
                          w->applyMode(i);
                  });
    }
    m.addSeparator();
    m.addSectionHeader("Waveform Scale");
    for (auto s : scopeScales)
    {
        m.addItem(juce::String(s) + "x", true, s == scopeScale,
                  [w = juce::Component::SafePointer(this), s]()
                  {
                      if (w)
                          w->setScopeScale(s);
                  });
    }
    m.showMenuAsync(juce::PopupMenu::Options());
}

void SpectrumAnalyzerComponent::handleAsyncUpdate() { repaint(); }

void SpectrumAnalyzerComponent::applyMode(int newIdx)
{
    bool wasRunning = running.exchange(false);
    if (analysisThread.joinable())
        analysisThread.join();

    ring.unsubscribe();
    ring.clear();

    modeIdx = newIdx;
    fftOrder = modes[modeIdx].fftOrder;
    fftSize = 1 << fftOrder;
    hopSize = modes[modeIdx].hopSize;
    numBins = fftSize / 2;

    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    hann = std::make_unique<juce::dsp::WindowingFunction<float>>(
        (size_t)fftSize, juce::dsp::WindowingFunction<float>::hann);

    windowBuffer.assign(fftSize, 0.f);
    windowFill = 0;
    fftScratch.assign(2 * fftSize, 0.f);
    col.assign(numBins, 0.f);

    // Scope window: 20 ms regardless of mode.
    scopeFrameLen = std::max(64, (int)std::round(0.02f * hostSampleRate));
    scopeBuilding.assign(scopeFrameLen, 0.f);
    scopeFill = 0;
    scopeWaitingForTrigger = false;
    scopePrev = 0.f;

    {
        std::lock_guard<std::mutex> lk(snapshotMutex);
        columns.assign((size_t)spectrogramColumns * numBins, 0.f);
        nextColumn = 0;
        latestLine.assign(numBins, 0.f);
        peakLine.assign(numBins, 0.f);
        scopeFrame.assign(scopeFrameLen, 0.f);
        spectrumVersion.fetch_add(1, std::memory_order_relaxed);
    }

    // Paint-side scratch resized once per mode change so paint never allocates.
    paintCols.assign((size_t)spectrogramColumns * numBins, 0.f);
    paintLine.assign(numBins, 0.f);
    paintPeak.assign(numBins, 0.f);
    paintScope.assign(scopeFrameLen, 0.f);
    paintNextColumn = 0;
    cachedSpecImage = juce::Image();
    cachedSpecImageW = 0;
    lastPaintedSpectrumVersion = (uint32_t)-1;

    auto framePeriod = (float)hopSize / hostSampleRate;
    constexpr float halfLife = 1.2f;
    peakDecay = std::pow(0.5f, framePeriod / halfLife);

    if (wasRunning || !analysisThread.joinable())
    {
        ring.subscribe();
        running = true;
        analysisThread = std::thread(&SpectrumAnalyzerComponent::analysisThreadMain, this);
    }
    if (defaults && modeIdx != lastSavedModeIdx)
    {
        defaults->updateUserDefaultValue(Defaults::spectrumAnalysisMode, modeIdx);
        lastSavedModeIdx = modeIdx;
    }
    repaint();
}

void SpectrumAnalyzerComponent::setScopeScale(int s)
{
    scopeScale = s;
    if (defaults)
        defaults->updateUserDefaultValue(Defaults::spectrumScopeScale, s);
    repaint();
}

void SpectrumAnalyzerComponent::analysisThreadMain()
{
    int newSamples = 0;

    while (running.load(std::memory_order_relaxed))
    {
        // Drain whatever audio is available, mono-summed into windowBuffer with wrap.
        bool drained = false;
        while (auto p = ring.pop())
        {
            auto m = 0.5f * (p->first + p->second);
            windowBuffer[windowFill % fftSize] = m;
            windowFill++;
            newSamples++;
            drained = true;

            // Scope: collect a frame, then wait for next positive zero-crossing.
            if (scopeWaitingForTrigger)
            {
                if (scopePrev <= 0.f && m > 0.f)
                {
                    scopeWaitingForTrigger = false;
                    scopeFill = 0;
                    scopeBuilding[scopeFill++] = m;
                }
            }
            else
            {
                scopeBuilding[scopeFill++] = m;
                if (scopeFill >= scopeFrameLen)
                {
                    {
                        std::lock_guard<std::mutex> lk(snapshotMutex);
                        std::copy(scopeBuilding.begin(), scopeBuilding.end(), scopeFrame.begin());
                    }
                    triggerAsyncUpdate();
                    scopeWaitingForTrigger = true;
                }
            }
            scopePrev = m;

            // Cap per drain pass to keep things responsive.
            if (newSamples >= hopSize && windowFill >= fftSize)
                break;
        }

        if (!drained || windowFill < fftSize || newSamples < hopSize)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // Compose the latest fftSize samples in time order from the wrap-around buffer.
        auto start = (windowFill - fftSize) % fftSize;
        if (start < 0)
            start += fftSize;
        for (int i = 0; i < fftSize; ++i)
            fftScratch[i] = windowBuffer[(start + i) % fftSize];

        std::fill(fftScratch.begin() + fftSize, fftScratch.end(), 0.f);
        hann->multiplyWithWindowingTable(fftScratch.data(), (size_t)fftSize);
        fft->performFrequencyOnlyForwardTransform(fftScratch.data());

        // Normalize: 1/N for FFT, ~2 for Hann coherent gain
        auto scale = 2.f / (float)fftSize;

        for (int b = 0; b < numBins; ++b)
            col[b] = magToNorm(fftScratch[b] * scale);

        {
            std::lock_guard<std::mutex> lk(snapshotMutex);
            auto *dst = columns.data() + (size_t)nextColumn * numBins;
            std::copy(col.begin(), col.end(), dst);
            nextColumn = (nextColumn + 1) % spectrogramColumns;
            std::copy(col.begin(), col.end(), latestLine.begin());
            for (int b = 0; b < numBins; ++b)
                peakLine[b] = std::max(col[b], peakLine[b] * peakDecay);
            spectrumVersion.fetch_add(1, std::memory_order_relaxed);
        }
        triggerAsyncUpdate();

        newSamples = 0;
    }
}

float SpectrumAnalyzerComponent::magInRange(const float *c, float b0, float b1) const
{
    int ib0 = std::max(0, (int)std::floor(b0));
    int ib1 = std::min(numBins - 1, (int)std::ceil(b1));
    if (b1 - b0 < 1.f && ib0 + 1 < numBins)
    {
        // Sub-bin: linear interp between adjacent bins.
        auto bf = 0.5f * (b0 + b1);
        int bi = std::clamp((int)std::floor(bf), 0, numBins - 2);
        auto f = bf - bi;
        return c[bi] * (1.f - f) + c[bi + 1] * f;
    }
    float v = 0.f;
    for (int b = ib0; b <= ib1; ++b)
        v = std::max(v, c[b]);
    return v;
}

void SpectrumAnalyzerComponent::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black);

    constexpr int leftMargin = 38;
    constexpr int bottomMargin = 14;
    constexpr int splitGap = 4;
    constexpr int scopeHeight = 132;
    constexpr int lineHeight = 186;

    auto bounds = getLocalBounds();
    auto leftLabels = bounds.removeFromLeft(leftMargin);
    auto scopeArea = bounds.removeFromBottom(scopeHeight);
    bounds.removeFromBottom(splitGap);
    auto freqLabels = bounds.removeFromBottom(bottomMargin);
    auto lineArea = bounds.removeFromBottom(lineHeight);
    bounds.removeFromBottom(splitGap);
    auto specArea = bounds;

    auto specRect = specArea.toFloat();
    auto lineRect = lineArea.toFloat();
    auto scopeRect = scopeArea.toFloat();

    int snapNumBins = 0;
    int snapFftSize = 0;
    uint32_t snapVersion = 0;
    {
        std::lock_guard<std::mutex> lk(snapshotMutex);
        snapNumBins = numBins;
        snapFftSize = fftSize;
        snapVersion = spectrumVersion.load(std::memory_order_relaxed);
        // Always copy scope (changes asynchronously from spectrum); copy spectrum data
        // only when something new arrived since the last paint.
        if (paintScope.size() == scopeFrame.size())
            std::copy(scopeFrame.begin(), scopeFrame.end(), paintScope.begin());
        if (snapVersion != lastPaintedSpectrumVersion)
        {
            if (paintCols.size() == columns.size())
                std::copy(columns.begin(), columns.end(), paintCols.begin());
            if (paintLine.size() == latestLine.size())
                std::copy(latestLine.begin(), latestLine.end(), paintLine.begin());
            if (paintPeak.size() == peakLine.size())
                std::copy(peakLine.begin(), peakLine.end(), paintPeak.begin());
            paintNextColumn = nextColumn;
        }
    }

    if (snapNumBins == 0)
        return;

    auto colDataAt = [&](int idx) { return paintCols.data() + (size_t)idx * snapNumBins; };

    // Log-frequency mapping shared by spectrogram (y) and line plot (x).
    constexpr float fmin = 20.f;
    auto fmax = std::max(hostSampleRate * 0.5f, fmin * 2.f);
    auto logRatio = std::log(fmax / fmin);
    auto freqAt = [&](float t) { return fmin * std::exp(t * logRatio); };
    auto tAt = [&](float f) { return std::log(f / fmin) / logRatio; };
    auto binAt = [&](float f) { return f * (float)snapFftSize / hostSampleRate; };

    // Spectrogram: rotated so frequency runs along X (matching the line plot below) and
    // time runs along Y, with the newest row at the top. Image is (specPxW × columns),
    // cached as a member and rebuilt only on geometry or data changes.
    auto specPxW = specArea.getWidth();
    if (specPxW > 0)
    {
        bool dimsChanged = (specPxW != cachedSpecImageW || !cachedSpecImage.isValid());
        if (dimsChanged)
        {
            cachedSpecImage = juce::Image(juce::Image::ARGB, specPxW, spectrogramColumns, false);
            cachedSpecImageW = specPxW;
        }
        if (dimsChanged || snapVersion != lastPaintedSpectrumVersion)
        {
            std::vector<float> b0s(specPxW), b1s(specPxW);
            for (int pcol = 0; pcol < specPxW; ++pcol)
            {
                auto t0 = pcol / (float)specPxW;
                auto t1 = (pcol + 1) / (float)specPxW;
                b0s[pcol] = binAt(freqAt(t0));
                b1s[pcol] = binAt(freqAt(t1));
            }
            juce::Image::BitmapData bd(cachedSpecImage, juce::Image::BitmapData::writeOnly);
            for (int prow = 0; prow < spectrogramColumns; ++prow)
            {
                // prow=columns-1 (bottom) = newest column; prow=0 (top) = oldest
                auto idx = (paintNextColumn + prow) % spectrogramColumns;
                auto *cdata = colDataAt(idx);
                auto *line = (juce::PixelARGB *)bd.getLinePointer(prow);
                for (int pcol = 0; pcol < specPxW; ++pcol)
                {
                    auto v = magInRange(cdata, b0s[pcol], b1s[pcol]);
                    auto col = colorFor(v);
                    line[pcol] = juce::PixelARGB(255, col.getRed(), col.getGreen(), col.getBlue());
                }
            }
            lastPaintedSpectrumVersion = snapVersion;
        }
        g.drawImage(cachedSpecImage, specRect);
    }

    auto dbToY = [&](float db)
    {
        auto v = (db - dbMin) / (dbMax - dbMin);
        return lineRect.getBottom() - v * lineRect.getHeight();
    };
    static constexpr std::array<float, 9> ticks{50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};

    // Grid + line-plot drawing is clipped to lineArea: magToNorm can yield negative
    // values (signals below dbMin), and stroked paths straddle getBottom() by half their
    // width, so without a clip those bits leak into the labels strip.
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(lineArea);

        g.setColour(juce::Colour(0xff202020));
        g.drawRect(lineRect, 1.f);
        for (int db = -20; db >= -80; db -= 20)
        {
            auto y = dbToY((float)db);
            g.drawHorizontalLine((int)y, lineRect.getX(), lineRect.getRight());
        }
        for (auto f : ticks)
        {
            if (f < fmin || f > fmax)
                continue;
            auto t = tAt(f);
            auto x = lineRect.getX() + t * lineRect.getWidth();
            g.drawVerticalLine((int)x, lineRect.getY(), lineRect.getBottom());
        }

        // Peak-hold trace (drawn first so live spectrum sits on top)
        juce::Path peakPath;
        {
            bool first = true;
            auto lineW = lineRect.getWidth();
            auto pxW = std::max(1, (int)std::round(lineW));
            for (int px = 0; px < pxW; ++px)
            {
                auto t0 = px / (float)pxW;
                auto t1 = (px + 1) / (float)pxW;
                auto v = magInRange(paintPeak.data(), binAt(freqAt(t0)), binAt(freqAt(t1)));
                auto x = lineRect.getX() + t0 * lineW;
                auto y = lineRect.getBottom() - v * lineRect.getHeight();
                if (first)
                {
                    peakPath.startNewSubPath(x, y);
                    first = false;
                }
                else
                    peakPath.lineTo(x, y);
            }
        }
        g.setColour(juce::Colour(0xff806030));
        g.strokePath(peakPath, juce::PathStrokeType(1.f));

        // Live line spectrum
        juce::Path linePath;
        {
            bool first = true;
            auto lineW = lineRect.getWidth();
            auto pxW = std::max(1, (int)std::round(lineW));
            for (int px = 0; px < pxW; ++px)
            {
                auto t0 = px / (float)pxW;
                auto t1 = (px + 1) / (float)pxW;
                auto v = magInRange(paintLine.data(), binAt(freqAt(t0)), binAt(freqAt(t1)));
                auto x = lineRect.getX() + t0 * lineW;
                auto y = lineRect.getBottom() - v * lineRect.getHeight();
                if (first)
                {
                    linePath.startNewSubPath(x, y);
                    first = false;
                }
                else
                    linePath.lineTo(x, y);
            }
        }
        g.setColour(juce::Colour(0xfff0c060));
        g.strokePath(linePath, juce::PathStrokeType(1.5f));
    }

    // Labels (drawn outside the line-area clip)
    g.setColour(juce::Colour(0xff808080));
    g.setFont(10.f);

    auto freqLabel = [](float f)
    {
        if (f >= 1000.f)
        {
            float k = f / 1000.f;
            if (std::abs(k - std::round(k)) < 0.05f)
                return juce::String((int)std::round(k)) + "k";
            return juce::String(k, 1) + "k";
        }
        return juce::String((int)f);
    };
    for (auto f : ticks)
    {
        if (f < fmin || f > fmax)
            continue;
        auto x = lineRect.getX() + tAt(f) * lineRect.getWidth();
        g.drawText(freqLabel(f), (int)x - 24, freqLabels.getY(), 48, freqLabels.getHeight(),
                   juce::Justification::centred);
    }
    for (int db = 0; db >= -80; db -= 20)
    {
        auto y = dbToY((float)db);
        g.drawText(juce::String(db), leftLabels.getX(), (int)y - 6, leftMargin - 4, 12,
                   juce::Justification::centredRight);
    }

    // Scope: 20 ms left-to-right, retriggered at the next positive zero crossing.
    // Samples beyond ±1 are clipped by the scope rect.
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRect(scopeRect);
    g.setColour(juce::Colour(0xff202020));
    g.drawRect(scopeRect, 1.f);
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(scopeArea);

        auto cy = scopeRect.getCentreY();
        auto halfH = scopeRect.getHeight() * 0.5f;
        g.setColour(juce::Colour(0xff303030));
        g.drawHorizontalLine((int)cy, scopeRect.getX(), scopeRect.getRight());
        g.setColour(juce::Colour(0xff202020));
        g.drawHorizontalLine((int)(cy - 0.5f * halfH), scopeRect.getX(), scopeRect.getRight());
        g.drawHorizontalLine((int)(cy + 0.5f * halfH), scopeRect.getX(), scopeRect.getRight());

        if (!paintScope.empty())
        {
            auto N = (int)paintScope.size();
            auto W = scopeRect.getWidth();
            auto X0 = scopeRect.getX();
            juce::Path tracePath;
            for (int i = 0; i < N; ++i)
            {
                auto x = X0 + (i / (float)(N - 1)) * W;
                auto y = cy - paintScope[i] * (float)scopeScale * halfH;
                if (i == 0)
                    tracePath.startNewSubPath(x, y);
                else
                    tracePath.lineTo(x, y);
            }
            g.setColour(juce::Colour(0xfff0c060));
            g.strokePath(tracePath, juce::PathStrokeType(1.f));
        }
    }

    // Info overlays in the upper-left of each plot.
    auto drawInfo = [&](juce::Rectangle<int> area, const juce::String &txt)
    {
        auto pad = 4;
        auto box = juce::Rectangle<int>(area.getX() + pad, area.getY() + pad, 200, 14);
        g.setColour(juce::Colour(0xa0000000));
        g.fillRect(box);
        g.setColour(juce::Colour(0xffc0c0c0));
        g.setFont(10.f);
        g.drawText(txt, box.reduced(4, 0), juce::Justification::centredLeft);
    };
    drawInfo(specArea, juce::String(fftSize) + "/" + juce::String(hopSize) + " - RMB to change");
    drawInfo(scopeArea, juce::String(scopeScale) + "x - RMB to change");
}

SpectrumAnalyzerWindow::SpectrumAnalyzerWindow(std::unique_ptr<SpectrumAnalyzerComponent> content)
    : juce::DocumentWindow("Six Sines Analyzer", juce::Colours::black,
                           juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(true);
    setResizable(true, false);
    // Default on-top so the window isn't hidden by hosts (e.g. Reaper) that pin the
    // plugin window above all peers. The toggle in the component's upper-right lets
    // the user opt out per session.
    setAlwaysOnTop(true);
    content->onAlwaysOnTopChanged = [this](bool b) { setAlwaysOnTop(b); };
    setContentOwned(content.release(), true);
    setSize(720, 690);
}

void SpectrumAnalyzerWindow::closeButtonPressed()
{
    // Defer: deleting the window inside its own callback is fragile in JUCE.
    juce::MessageManager::callAsync(
        [cb = onCloseRequested]()
        {
            if (cb)
                cb();
        });
}

} // namespace baconpaul::six_sines::ui
