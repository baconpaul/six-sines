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

#include "about-screen.h"
#include "configuration.h"

#include <fmt/core.h>
#include <cmrc/cmrc.hpp>

#include <sst/plugininfra/version_information.h>
#include <sst/plugininfra/paths.h>

CMRC_DECLARE(sixsines_fonts);

namespace baconpaul::six_sines::ui
{

AboutScreen::AboutScreen(SixSinesEditor &e) : HasEditor(e)
{
    // Icon and acknowledgements text ride along in the fonts cmrc filesystem.
    try
    {
        auto fs = cmrc::sixsines_fonts::get_filesystem();

        auto iconFile = fs.open("resources/icon/SixSinesIcon.png");
        iconImage = juce::ImageFileFormat::loadFrom(iconFile.begin(), iconFile.size());

        auto ackFile = fs.open("doc/ack.md");
        std::string ackText(ackFile.begin(), ackFile.end());

        ackLabel = std::make_unique<jcmp::Label>();
        ackLabel->setText(ackText);
        ackLabel->setJustification(juce::Justification::topLeft);
        ackLabel->setIsMultiline(true);
        // Let clicks on the text fall through so they dismiss like the rest of
        // the backdrop; only the buttons swallow clicks.
        ackLabel->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*ackLabel);
    }
    catch (const std::exception &err)
    {
        SXSNLOG("AboutScreen resource load failed: " << err.what());
    }

    manualButton = std::make_unique<jcmp::TextPushButton>();
    manualButton->setLabel("Read the Manual");
    manualButton->setOnCallback([]() { juce::URL(manualURL).launchInDefaultBrowser(); });
    addAndMakeVisible(*manualButton);

    sourceButton = std::make_unique<jcmp::TextPushButton>();
    sourceButton->setLabel("Get the Source");
    sourceButton->setOnCallback([]() { juce::URL(sourceURL).launchInDefaultBrowser(); });
    addAndMakeVisible(*sourceButton);

    copyButton = std::make_unique<jcmp::TextPushButton>();
    copyButton->setLabel("Copy Info");
    copyButton->setOnCallback([this]()
                              { juce::SystemClipboard::copyTextToClipboard(infoClipboardText); });
    addAndMakeVisible(*copyButton);

    buildInfo();
}

AboutScreen::~AboutScreen() = default;

void AboutScreen::buildInfo()
{
    namespace vi = sst::plugininfra;
    using VI = vi::VersionInformation;

    auto add = [this](const std::string &k, const std::string &v)
    {
        infoRows.emplace_back(k, v);
        infoClipboardText += k + ": " + v + "\n";
    };

    add("Version", fmt::format("Six Sines {} ({}/{})", VI::project_version, VI::git_branch,
                               VI::git_commit_hash));
    add("Build", fmt::format("{} @ {} on '{}' with '{}' using JUCE {}", VI::build_date,
                             VI::build_time, VI::build_host, VI::cmake_compiler,
                             juce::SystemStats::getJUCEVersion().toStdString()));

    auto cpu = juce::SystemStats::getCpuModel().toStdString();
    if (cpu.empty())
        cpu = juce::SystemStats::getCpuVendor().toStdString();
    add("System", fmt::format("{} {} on {} ({} cores), {} GB RAM",
                              juce::SystemStats::getOperatingSystemName().toStdString(),
                              juce::SystemStats::isOperatingSystem64Bit() ? "64-bit" : "32-bit",
                              cpu, juce::SystemStats::getNumPhysicalCpus(),
                              (juce::SystemStats::getMemorySizeInMegabytes() + 512) / 1024));

    if (editor.clapHost)
        add("Host", fmt::format("{} {}", editor.clapHost->name, editor.clapHost->version));

    if (editor.hostSR > 0)
    {
        if (editor.engineSR > 0 && editor.engineSR != editor.hostSR)
            add("Sample Rate",
                fmt::format("{:.0f} Hz (engine {:.0f} Hz)", editor.hostSR, editor.engineSR));
        else
            add("Sample Rate", fmt::format("{:.0f} Hz", editor.hostSR));
    }
    if (editor.presetManager)
        add("User Data", editor.presetManager->userPath.u8string());
}

void AboutScreen::mouseDown(const juce::MouseEvent &event) { setVisible(false); }

void AboutScreen::resized()
{
    ModalBase::resized();

    auto ca = getContentArea().reduced(14);

    static constexpr int iconSz{132}, btnH{26}, btnGap{6}, sectionGap{18};

    iconArea = juce::Rectangle<int>(ca.getX(), ca.getY(), iconSz, iconSz);

    // Left column under the logo: Manual, Source, then Copy Info.
    auto btnY = iconArea.getBottom() + 10;
    if (manualButton)
        manualButton->setBounds(ca.getX(), btnY, iconSz, btnH);
    if (sourceButton)
        sourceButton->setBounds(ca.getX(), btnY + btnH + btnGap, iconSz, btnH);
    if (copyButton)
        copyButton->setBounds(ca.getX(), btnY + 2 * (btnH + btnGap), iconSz, btnH);

    // Header (title + subtitle) sits to the right of the logo, top aligned with
    // it, and is painted in paintContents.
    auto headerX = ca.getX() + iconSz + sectionGap;
    headerArea =
        juce::Rectangle<int>(headerX, iconArea.getY(), ca.getRight() - headerX, titleBoxHeight);

    // Info stack anchored to the bottom of the content box.
    auto infoHeight = static_cast<int>(infoRows.size()) * infoRowHeight;
    auto infoTop = ca.getBottom() - infoHeight;
    infoArea = juce::Rectangle<int>(ca.getX(), infoTop, ca.getWidth(), infoHeight);

    // Acknowledgements: right of the logo, squeezed up under the header and
    // running down to just above the info stack.
    auto ackTop = headerArea.getBottom() + 6;
    auto ackBottom = infoTop - 12;
    if (ackLabel)
        ackLabel->setBounds(headerX, ackTop, ca.getRight() - headerX, ackBottom - ackTop);
}

void AboutScreen::paintContents(juce::Graphics &g)
{
    if (iconImage.isValid())
        g.drawImageWithin(iconImage, iconArea.getX(), iconArea.getY(), iconArea.getWidth(),
                          iconArea.getHeight(), juce::RectanglePlacement::centred);

    // Title + subtitle next to the logo, top aligned with it.
    auto titleFont = getFont(Styles::labelfont).withHeight(titleFontHeight);
    auto subFont = getFont(Styles::labelfont).withHeight(subtitleFontHeight);
    g.setColour(getColour(Styles::labelcolor));
    g.setFont(titleFont);
    g.drawText("Six Sines", headerArea.getX(), headerArea.getY(), headerArea.getWidth(),
               titleBoxHeight, juce::Justification::topLeft);
    g.setColour(getColour(Styles::labelcolor).withAlpha(0.7f));
    g.setFont(subFont);
    g.drawText("An Audio-rate Modulation Synth", headerArea.getX(), headerArea.getY(),
               headerArea.getWidth(), titleBoxHeight, juce::Justification::topRight);
    g.drawText("by BaconPaul", headerArea.getX(), headerArea.getY() + subtitleFontHeight,
               headerArea.getWidth(), titleBoxHeight, juce::Justification::topRight);

    auto font = getFont(Styles::labelfont);
    g.setFont(font);

    auto labelCol = getColour(Styles::brightoutline);
    auto valueCol = getColour(Styles::labelcolor);

    static constexpr int labelColW{110}, colGap{8};
    auto y = infoArea.getY();
    for (const auto &[k, v] : infoRows)
    {
        g.setColour(labelCol);
        g.drawText(k, infoArea.getX(), y, labelColW, infoRowHeight, juce::Justification::topLeft);
        g.setColour(valueCol);
        g.drawText(v, infoArea.getX() + labelColW + colGap, y,
                   infoArea.getWidth() - labelColW - colGap, infoRowHeight,
                   juce::Justification::topLeft);
        y += infoRowHeight;
    }
}
} // namespace baconpaul::six_sines::ui
