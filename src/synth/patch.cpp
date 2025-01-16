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