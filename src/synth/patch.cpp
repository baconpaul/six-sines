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
#include "dsp/wavetable_io.h"

#include <algorithm>
#include <map>
#include <sstream>
namespace baconpaul::six_sines
{

void Patch::setupAdditionalState()
{
    onResetToInit = [](Patch &p)
    {
        p.clearWavetables();
        p.setAuthor(p.defaultAuthor);
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

        TiXmlElement wts("wavetables");
        for (size_t i = 0; i < wavetableBlobs.size(); ++i)
        {
            const auto &b = wavetableBlobs[i];
            auto packed = deflateBytes(b.sourceBytes.data(), b.sourceBytes.size());
            if (packed.empty())
                continue;
            TiXmlElement e("table");
            e.SetAttribute("idx", (int)i);
            e.SetAttribute("raw", (int)b.sourceBytes.size());
            e.SetAttribute("encoding", "deflate-base64");
            e.SetAttribute("name", b.name);
            // the XML boundary is one of the few places a path becomes a string
            if (!b.sourcePath.empty())
                e.SetAttribute("path", b.sourcePath.u8string());
            std::ostringstream hs;
            hs << std::hex << b.hash;
            e.SetAttribute("hash", hs.str());
            TiXmlText t(base64Encode(packed.data(), packed.size()));
            t.SetCDATA(false);
            e.InsertEndChild(t);
            wts.InsertEndChild(e);
        }
        root.InsertEndChild(wts);

        TiXmlElement srcs("sourceWavetables");
        for (int i = 0; i < numOps; ++i)
        {
            if (sourceNodes[i].wavetableBlobIndex < 0)
                continue;
            TiXmlElement e("source");
            e.SetAttribute("idx", i);
            e.SetAttribute("table", sourceNodes[i].wavetableBlobIndex);
            srcs.InsertEndChild(e);
        }
        root.InsertEndChild(srcs);
    };

    additionalFromState = [this](TiXmlElement *root, uint32_t /*ver*/)
    {
        readWavetablesFromState(root);

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

    // version 13 inserted USER_TABLE before AUDIO_IN, so AUDIO_IN moved 21 -> 22. Everything
    // below the insert point keeps its value.
    if (p->adhocFeatures & (uint64_t)Param::AdHocFeatureValues::WAVEFORM && version < 13)
    {
        if ((int)std::round(value) == 21)
            value = SinTable::AUDIO_IN;
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

    // zohPreFilter was added in version 11 and defaults on for new patches; pre-11
    // patches predate it (and its sound), so keep them as they were: filter off.
    if (version <= 10)
    {
        output.zohPreFilter.value = 0;
    }

    // v12 maps the LFO deform onto the StepLFO's full -2..2 smooth range (deform*2).
    // Pre-12 step-sequencer patches stored deform at half that scale, so halve it back
    // for any node currently in Step shape to preserve the original smoothing.
    if (version <= 11)
    {
        auto halveStepDeform = [](auto &n)
        {
            if ((int)std::round(n.lfoShape.value) == LFOMixin::Shape::Step)
                n.lfoDeform.value *= 0.5f;
        };
        for (auto &s : sourceNodes)
            halveStepDeform(s);
        for (auto &s : selfNodes)
            halveStepDeform(s);
        for (auto &s : matrixNodes)
            halveStepDeform(s);
        for (auto &s : mixerNodes)
            halveStepDeform(s);
        for (auto &s : macroNodes)
            halveStepDeform(s);
        halveStepDeform(output);
        halveStepDeform(fineTuneMod);
        halveStepDeform(mainPanMod);
    }
}

int Patch::addWavetableBlob(const std::vector<uint8_t> &bytes, const std::string &name,
                            const fs::path &sourcePath)
{
    if (bytes.empty())
        return -1;
    auto hash = hashBytes(bytes.data(), bytes.size());
    for (size_t i = 0; i < wavetableBlobs.size(); ++i)
        if (wavetableBlobs[i].hash == hash)
        {
            // same bytes from a path we did not know before: remember it, so jogging works
            if (wavetableBlobs[i].sourcePath.empty())
                wavetableBlobs[i].sourcePath = sourcePath;
            return (int)i;
        }
    wavetableBlobs.push_back({hash, name, sourcePath, bytes});
    return (int)wavetableBlobs.size() - 1;
}

void Patch::clearWavetables()
{
    wavetableBlobs.clear();
    for (auto &sn : sourceNodes)
    {
        sn.wavetableBlobIndex = -1;
        // dropping the last reference here would free on whichever thread cleared the patch;
        // only ever called on main, and the engine's retiring list holds its own reference
        sn.wavetable.reset();
    }
}

size_t Patch::encodedWavetableBytes() const
{
    size_t n{0};
    for (const auto &b : wavetableBlobs)
    {
        auto packed = deflateBytes(b.sourceBytes.data(), b.sourceBytes.size());
        n += (packed.size() + 2) / 3 * 4; // base64 expansion
    }
    return n;
}

void Patch::readWavetablesFromState(TiXmlElement *root)
{
    clearWavetables();

    auto *wts = root->FirstChildElement("wavetables");
    if (wts)
    {
        // idx is authoritative, so a blob that fails to decode leaves a hole rather than
        // shifting every later reference by one
        std::map<int, WavetableBlob> byIdx;
        for (auto *e = wts->FirstChildElement("table"); e; e = e->NextSiblingElement("table"))
        {
            int idx{-1}, raw{0};
            if (e->QueryIntAttribute("idx", &idx) != TIXML_SUCCESS || idx < 0)
                continue;
            if (e->QueryIntAttribute("raw", &raw) != TIXML_SUCCESS || raw <= 0)
                continue;
            /*
             * The entry is recorded whether or not its payload survives. An empty slot is how
             * the reconcile learns that a wavetable was in this patch and could not be read -
             * skipping it entirely would leave the operator looking like it never had one, and
             * it would quietly play something else instead.
             */
            WavetableBlob b;
            if (auto *nm = e->Attribute("name"))
                b.name = nm;
            if (auto *pa = e->Attribute("path"))
                b.sourcePath = fs::path(std::string(pa));

            auto *n = e->FirstChild();
            std::vector<uint8_t> packed;
            if (n && n->ToText() && n->ToText()->Value() &&
                base64Decode(n->ToText()->Value(), packed))
            {
                if (!inflateBytes(packed.data(), packed.size(), (size_t)raw, b.sourceBytes))
                    b.sourceBytes.clear();
            }
            b.hash = b.sourceBytes.empty()
                         ? 0
                         : hashBytes(b.sourceBytes.data(), b.sourceBytes.size());
            byIdx[idx] = std::move(b);
        }
        if (!byIdx.empty())
        {
            wavetableBlobs.resize(byIdx.rbegin()->first + 1);
            for (auto &[i, b] : byIdx)
                wavetableBlobs[i] = std::move(b);
        }
    }

    auto *srcs = root->FirstChildElement("sourceWavetables");
    if (!srcs)
        return;
    for (auto *e = srcs->FirstChildElement("source"); e; e = e->NextSiblingElement("source"))
    {
        int idx{-1}, table{-1};
        if (e->QueryIntAttribute("idx", &idx) != TIXML_SUCCESS || idx < 0 || idx >= numOps)
            continue;
        if (e->QueryIntAttribute("table", &table) != TIXML_SUCCESS)
            continue;
        /*
         * A reference past the blobs we recovered is dropped rather than left dangling - that
         * is a patch written before blobs existed, or one whose wavetables element is gone, and
         * the operator simply has no selection.
         *
         * An empty slot inside the list is different: the blob WAS in the file and did not
         * decode. Keep the reference so the reconcile reports it, because silently substituting
         * a different table is how someone loses a wavetable without being told.
         */
        if (table < 0 || table >= (int)wavetableBlobs.size())
            continue;
        sourceNodes[idx].wavetableBlobIndex = table;
    }
}

} // namespace baconpaul::six_sines
