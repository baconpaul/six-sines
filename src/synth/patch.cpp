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

#include "patch.h"
namespace baconpaul::six_sines
{

void Patch::setupAdditionalState()
{
    onResetToInit = [](Patch &p)
    {
        for (int i = 0; i < numMacros; ++i)
        {
            auto s = "Macro " + std::to_string(i + 1);
            strncpy(p.macroNames[i].data(), s.c_str(), 63);
            p.macroNames[i][63] = '\0';
        }
    };

    additionalToState = [this](TiXmlElement &root)
    {
        TiXmlElement mn("macroNames");
        for (int i = 0; i < numMacros; ++i)
        {
            TiXmlElement entry("macroName");
            entry.SetAttribute("idx", i);
            TiXmlText t(macroNames[i].data());
            t.SetCDATA(true);
            entry.InsertEndChild(t);
            mn.InsertEndChild(entry);
        }
        root.InsertEndChild(mn);
    };

    additionalFromState = [this](TiXmlElement *root, uint32_t /*ver*/)
    {
        auto *mn = root->FirstChildElement("macroNames");
        if (!mn)
            return;
        auto *entry = mn->FirstChildElement("macroName");
        while (entry)
        {
            int idx = -1;
            if (entry->QueryIntAttribute("idx", &idx) == TIXML_SUCCESS && idx >= 0 &&
                idx < numMacros)
            {
                auto *n = entry->FirstChild();
                if (n)
                {
                    auto *txt = n->ToText();
                    if (txt && txt->Value())
                    {
                        strncpy(macroNames[idx].data(), txt->Value(), 63);
                        macroNames[idx][63] = '\0';
                    }
                }
            }
            entry = entry->NextSiblingElement("macroName");
        }
    };
}

float Patch::migrateParamValueFromVersion(Param *p, float value, uint32_t version)
{
    if ((p->adhocFeatures & Param::AdHocFeatureValues::ENVTIME) && version <= 2)
    {
        /*
         * This is a gross way to do this but really its just to not break
         * Jacky's patches from the very first weekend, so...
         */
        static auto oldStyle = md_t().asFloat().asLog2SecondsRange(
            sst::basic_blocks::modulators::TenSecondRange::etMin,
            sst::basic_blocks::modulators::TenSecondRange::etMax);
        if (value < oldStyle.minVal + 0.0001)
            return 0.f;
        if (value > oldStyle.maxVal - 0.0001)
            return 1.f;
        auto osv = oldStyle.valueToString(value);
        if (osv.has_value())
        {
            std::string em;
            auto nsv = p->meta.valueFromString(*osv, em);
            if (nsv.has_value())
            {
                SXSNLOG("Converting version " << version << " node '" << p->meta.name
                                              << "' val=" << value << " -> " << *nsv);
                value = *nsv;
            }
            else
            {
                value = 0.f;
            }
        }
        else
        {
            value = 0.f;
        }
    }

    if (p == &output.playMode && version <= 3)
    {
        if (value > 0)
            return 1;
    }

    if (p == &output.defaultTrigger && version == 6)
    {
        if (value == 1 /* that's NEW_VOICE */)
        {
            return 0; /* that's NEW_GATE */
        }
    }

    if ((p->adhocFeatures & (uint64_t)Param::AdHocFeatureValues::TRIGGERMODE))
    {
        if (version == 5)
        {
            if (value == 2)
                return value + 1;
        }
    }

    if (p->adhocFeatures & (uint64_t)Param::AdHocFeatureValues::WAVEFORM && version < 9)
    {
        /* version 8
        *   SIN = 0, // these stream so you know....
        SIN_FIFTH,
        SQUARISH,
        SAWISH,
        SIN_OF_CUBED,

        TX2,
        TX3,
        TX4,
        TX5,
        TX6,
        TX7,
        TX8,

        version 9 and later

        SIN = 0, // these stream so you know....
        SIN_FIFTH,
        SQUARISH,
        SAWISH,
        TRIANGLE,
        SIN_OF_CUBED,

        TX2,
        TX3,
        TX4,
        TX5,
        TX6,
        TX7,
        TX8,

        SPIKY_TX2,
        SPIKY_TX4,
        SPIKY_TX6,
        SPIKY_TX8,
        */

        // work around triangle
        if (value > 3)
            value = value + 1;

        auto ivalue = (int)std::round(value);
        if (ivalue == SinTable::TX2)
            value = SinTable::SPIKY_TX2;
        if (ivalue == SinTable::TX4)
            value = SinTable::SPIKY_TX4;
        if (ivalue == SinTable::TX6)
            value = SinTable::SPIKY_TX6;
        if (ivalue == SinTable::TX8)
            value = SinTable::SPIKY_TX8;
    }
    return value;
}

void Patch::migratePatchFromVersion(uint32_t version)
{
    if (version == 7)
    {
        auto fixTrigMod = [](auto &a)
        {
            if (a.triggerMode.value == 5)
            {
                a.triggerMode.value = 3; // PATCH_DEFAULT
                a.envIsOneShot.value = true;
            }
        };
        for (auto &s : sourceNodes)
            fixTrigMod(s);
        for (auto &s : selfNodes)
            fixTrigMod(s);
        for (auto &s : matrixNodes)
            fixTrigMod(s);
        fixTrigMod(output);
        fixTrigMod(fineTuneMod);
        fixTrigMod(mainPanMod);
    }
}

} // namespace baconpaul::six_sines