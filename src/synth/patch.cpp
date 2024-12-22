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

namespace baconpaul::six_sines
{

std::string Patch::toState() const
{
    std::ostringstream oss;
    oss << "SIXSINES;V=1\n";
    for (auto p : params)
    {
        oss << p->meta.id << "|" << p->value << "\n";
    }
    return oss.str();
}
bool Patch::fromState(const std::string &idata)
{
    // Sweep any new param since this stream to default
    for (auto [id, p] : paramMap)
    {
        p->value = p->meta.defaultVal;
    }

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
                SXSNLOG("Ignoring vestigal param " << idv);
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