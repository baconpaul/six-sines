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

#ifndef BACONPAUL_SIX_SINES_PRESETS_UI_THEME_MANAGER_H
#define BACONPAUL_SIX_SINES_PRESETS_UI_THEME_MANAGER_H

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "filesystem/import.h"
#include "sst/plugininfra/paths.h"
#include "ui/six-sines-skin.h"
#include "configuration.h"

namespace baconpaul::six_sines::presets
{

struct UIThemeManager
{
    static constexpr const char *factoryThemesPath{"resources/factory_themes"};

    fs::path userPath;
    fs::path userThemesPath;

    struct FactoryTheme
    {
        std::string name;
        ui::SixSinesSkin skin;
    };
    std::vector<FactoryTheme> factoryThemes;

    std::vector<fs::path> userThemes;

    // Constructor is implemented in ui-theme-manager.cpp so that cmrc usage
    // (CMRC_DECLARE / get_filesystem) is confined to a single translation unit.
    UIThemeManager();

    void rescanUserThemes()
    {
        userThemes.clear();
        try
        {
            for (auto &entry : fs::directory_iterator(userThemesPath))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".sixtheme")
                    userThemes.push_back(entry.path());
            }
            std::sort(userThemes.begin(), userThemes.end(),
                      [](const auto &a, const auto &b) { return a.filename() < b.filename(); });
        }
        catch (fs::filesystem_error &e)
        {
            SXSNLOG("Error scanning themes dir: " << e.what());
        }
    }

    // Save the skin to an arbitrary path.
    void saveThemeToPath(const ui::SixSinesSkin &skin, const fs::path &path)
    {
        try
        {
            std::ofstream f(path);
            if (f.is_open())
                f << skin.toXmlString();
        }
        catch (std::exception &e)
        {
            SXSNLOG("Error saving theme: " << e.what());
        }
    }

    // Save to userThemesPath / <name>.sixtheme, then rescan.
    void saveUserTheme(const ui::SixSinesSkin &skin, const std::string &name)
    {
        auto path = userThemesPath / (name + ".sixtheme");
        saveThemeToPath(skin, path);
        rescanUserThemes();
    }

    // Load a skin from an arbitrary path.  Returns darkDefault() on failure.
    ui::SixSinesSkin loadThemeFromPath(const fs::path &path)
    {
        try
        {
            std::ifstream f(path);
            if (!f.is_open())
                return ui::SixSinesSkin::darkDefault();
            std::ostringstream ss;
            ss << f.rdbuf();
            return ui::SixSinesSkin::fromXmlString(ss.str());
        }
        catch (std::exception &e)
        {
            SXSNLOG("Error loading theme: " << e.what());
            return ui::SixSinesSkin::darkDefault();
        }
    }
};

} // namespace baconpaul::six_sines::presets

#endif // BACONPAUL_SIX_SINES_PRESETS_UI_THEME_MANAGER_H
