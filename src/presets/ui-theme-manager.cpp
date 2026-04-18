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

#include "ui-theme-manager.h"
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(sixsines_themes);

namespace baconpaul::six_sines::presets
{

UIThemeManager::UIThemeManager()
{
    try
    {
        userPath = sst::plugininfra::paths::bestDocumentsFolderPathFor("SixSines");
        userThemesPath = userPath / "Themes";
        fs::create_directories(userThemesPath);
    }
    catch (fs::filesystem_error &e)
    {
        SXSNLOG("Unable to create themes dir: " << e.what());
    }

    try
    {
        auto themefs = cmrc::sixsines_themes::get_filesystem();
        std::vector<std::string> names;
        for (const auto &f : themefs.iterate_directory(factoryThemesPath))
        {
            if (!f.is_directory())
                names.push_back(f.filename());
        }
        std::sort(names.begin(), names.end());
        for (const auto &fname : names)
        {
            auto path = std::string(factoryThemesPath) + "/" + fname;
            auto file = themefs.open(path);
            std::string xml(file.begin(), file.end());
            auto skin = ui::SixSinesSkin::fromXmlString(xml);
            // Strip the .sixtheme extension to get the theme name
            auto name = fname;
            if (name.size() > 9 && name.substr(name.size() - 9) == ".sixtheme")
                name = name.substr(0, name.size() - 9);
            factoryThemes.push_back({name, skin});
        }
    }
    catch (const std::exception &e)
    {
        SXSNLOG("Error loading factory themes from resources: " << e.what());
        factoryThemes.push_back({"Dark", ui::SixSinesSkin::darkDefault()});
    }

    rescanUserThemes();
}

} // namespace baconpaul::six_sines::presets
