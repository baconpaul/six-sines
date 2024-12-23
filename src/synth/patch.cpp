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

#include "patch.h"
#include <sstream>
#include "tinyxml/tinyxml.h"

namespace baconpaul::six_sines
{

void Patch::resetToInit()
{
    // Sweep any new param since this stream to default
    for (auto [id, p] : paramMap)
    {
        p->value = p->meta.defaultVal;
    }
}
std::string Patch::toState() const
{
    TiXmlDocument doc;
    TiXmlElement rootNode("patch");
    rootNode.SetAttribute("id", "org.baconpaul.six-sines");
    rootNode.SetAttribute("version", "2");

    TiXmlElement paramsNode("params");

    for (auto p : params)
    {
        TiXmlElement param("p");
        param.SetAttribute("id", p->meta.id);
        param.SetDoubleAttribute("v", p->value);
        paramsNode.InsertEndChild(param);
    }

    rootNode.InsertEndChild(paramsNode);
    doc.InsertEndChild(rootNode);

    std::ostringstream oss;
    oss << doc;
    return oss.str();
}
bool Patch::fromState(const std::string &idata)
{
    resetToInit();
    if (idata.substr(0, 12) == "SIXSINES;V=1")
    {
        return fromStateV1(idata);
    }

    TiXmlDocument doc;
    doc.Parse(idata.c_str());

    auto rn = doc.FirstChildElement("patch");
    if (!rn)
    {
        SXSNLOG("No Patch");
        return false;
    }
    if (strcmp(rn->Attribute("id"), "org.baconpaul.six-sines") != 0)
    {
        SXSNLOG("Wrong ID");
        return false;
    }
    int ver;
    if (rn->QueryIntAttribute("version", &ver) != TIXML_SUCCESS)
    {
        SXSNLOG("No Version");
        return false;
    }
    if (ver != 2)
    {
        SXSNLOG("Unknown version " << ver);
        return false;
    }

    auto pars = rn->FirstChildElement("params");
    if (!pars)
    {
        SXSNLOG("No Params");
        return false;
    }

    auto *par = pars->FirstChildElement("p");
    while (par)
    {
        int id;
        double value;
        if (par->QueryIntAttribute("id", &id) == TIXML_SUCCESS &&
            par->QueryDoubleAttribute("v", &value) == TIXML_SUCCESS)
        {
            auto it = paramMap.find(id);
            if (it == paramMap.end())
            {
                // SXSNLOG("  -  vestigal param " << id);
            }
            else
            {
                it->second->value = value;
            }
        }
        else
        {
            SXSNLOG("Par missing id or value");
        }
        par = par->NextSiblingElement("p");
    }

    return true;
}

bool Patch::fromStateV1(const std::string &idata)
{
    std::string data = idata;
    auto p = data.find('\n');
    bool first{true};
    while (p != std::string::npos && !data.empty())
    {
        auto ss = data.substr(0, p);
        if (first)
        {
            if (ss != "SIXSINES;V=1")
            {
                SXSNLOG("Bad version string [" << ss << "]");
                return false;
            }
            first = false;
        }
        else
        {
            auto sl = ss.find('|');
            auto id = ss.substr(0, sl);
            auto v = ss.substr(sl + 1);

            auto idv = std::atoi(id.c_str());
            auto vv = std::atof(v.c_str());

            auto it = paramMap.find(idv);
            if (it == paramMap.end())
            {
                // SXSNLOG("Ignoring vestigal param " << idv);
            }
            else
            {
                it->second->value = vv;
            }
        }
        data = data.substr(p + 1);
        p = data.find('\n');
    }

    return true;
}

} // namespace baconpaul::six_sines