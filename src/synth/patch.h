/*
 * BaconPaul's FM Atrocity
 *
 * A mess, with FM.
 *
 * Copyright 2024, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work can also be
 * released under GPL3. You know the drill.
 */

#ifndef BACONPAUL_FMTHING_SYNTH_PATCH_H
#define BACONPAUL_FMTHING_SYNTH_PATCH_H

#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <clap/clap.h>
#include "configuration.h"
#include "sst/cpputils/constructors.h"
#include "sst/basic-blocks/params/ParamMetadata.h"
#include "sst/basic-blocks/modulators/DAHDSREnvelope.h"
#include "synth/matrix_index.h"

namespace baconpaul::fm
{
namespace scpu = sst::cpputils;
using md_t = sst::basic_blocks::params::ParamMetaData;
struct Param
{
    Param(const md_t &m) : value(m.defaultVal), meta(m) {}
    float value{0};
    const md_t meta{};

    Param &operator=(const float &val)
    {
        value = val;
        return *this;
    }

    operator const float &() const { return value; }
};

struct Patch
{
    std::vector<const Param *> params;
    std::unordered_map<uint32_t, Param *> paramMap;

    static constexpr uint32_t floatFlags{CLAP_PARAM_IS_AUTOMATABLE};
    static constexpr uint32_t boolFlags{CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED};

    static md_t floatMd() { return md_t().asFloat().withFlags(floatFlags); }
    static md_t floatEnvRateMd()
    {
        return md_t()
            .asFloat()
            .withFlags(floatFlags)
            .asLog2SecondsRange(sst::basic_blocks::modulators::TenSecondRange::etMin,
                                sst::basic_blocks::modulators::TenSecondRange::etMax);
    }
    static md_t boolMd() { return md_t().asBool().withFlags(boolFlags); }

    Patch()
        : sourceNodes(scpu::make_array_bind_first_index<SourceNode, numOps>()),
          selfNodes(scpu::make_array_bind_first_index<SelfNode, numOps>()),
          matrixNodes(scpu::make_array_bind_first_index<MatrixNode, matrixSize>()),
          mixerNodes(scpu::make_array_bind_first_index<MixerNode, numOps>())
    {
        auto pushParams = [this](auto &from)
        {
            auto m = from.params();
            params.insert(params.end(), m.begin(), m.end());
            for (auto &p : m)
            {
                if (paramMap.find(p->meta.id) != paramMap.end())
                {
                    FMLOG("Duplicate param id " << p->meta.id << " at " << p->meta.name);
                    FMLOG("Collision with " << paramMap[p->meta.id]->meta.name);
                    std::terminate();
                }
                paramMap.emplace(p->meta.id, p);
            }
        };

        pushParams(output);
        std::for_each(sourceNodes.begin(), sourceNodes.end(), pushParams);
        std::for_each(selfNodes.begin(), selfNodes.end(), pushParams);
        std::for_each(mixerNodes.begin(), mixerNodes.end(), pushParams);
        std::for_each(matrixNodes.begin(), matrixNodes.end(), pushParams);
    }

    struct DAHDSRMixin
    {
        using tsr = sst::basic_blocks::modulators::TenSecondRange;
        DAHDSRMixin(const std::string name, int id0, bool longAdsr)
            : delay(floatEnvRateMd()
                        .withName(name + " Delay")
                        .withGroupName(name)
                        .withDefault(tsr::etMin)
                        .withID(id0 + 0)),
              attack(floatEnvRateMd()
                         .withName(name + " Attack")
                         .withGroupName(name)
                         .withDefault(longAdsr ? -1.f : tsr::etMin)
                         .withID(id0 + 1)),
              hold(floatEnvRateMd()
                       .withName(name + " Hold")
                       .withGroupName(name)
                       .withDefault(tsr::etMin)
                       .withID(id0 + 2)),
              decay(floatEnvRateMd()
                        .withName(name + " Decay")
                        .withGroupName(name)
                        .withDefault(longAdsr ? 1.f : tsr::etMin)
                        .withID(id0 + 3)),
              sustain(floatMd()
                          .asPercent()
                          .withName(name + " Sustain")
                          .withGroupName(name)
                          .withDefault(longAdsr ? 0.7f : 1.f)
                          .withID(id0 + 4)),
              release(floatEnvRateMd()
                          .withName(name + " Release")
                          .withGroupName(name)
                          .withDefault(longAdsr ? 0.5f : tsr::etMin)
                          .withID(id0 + 5))
        {
        }

        Param delay, attack, hold, decay, sustain, release;
    };

    struct SourceNode
    {
        static constexpr uint32_t idBase{1500}, idStride{250};
        int index;
        SourceNode(size_t idx)
            : index(idx), ratio(floatMd()
                                    .withRange(-4, 4)
                                    .withATwoToTheBFormatting(1, 1, "x")
                                    .withName(name() + " Ratio")
                                    .withGroupName(name())
                                    .withDefault(0.0)
                                    .withID(id(0))),
              active(boolMd()
                         .withGroupName(name())
                         .withName(name() + " Active")
                         .withDefault(1.0)
                         .withID(id(1)))
        {
        }

        std::string name() const { return "Op " + std::to_string(index + 1); }
        uint32_t id(int f) const { return idBase + idStride * index + f; }

        Param ratio;
        Param active;

        std::vector<Param *> params() { return {&ratio, &active}; }
    };
    struct SelfNode : public DAHDSRMixin
    {
        // Once streamed you cant change these bases or the individual ids inside
        static constexpr uint32_t idBase{10000}, idStride{100};
        int index;
        SelfNode(size_t idx)
            : index(idx), fbLevel(floatMd()
                                      .asPercent()
                                      .withName(name() + " Feedback Level")
                                      .withGroupName(name())
                                      .withDefault(0.0)
                                      .withID(id(0))),
              active(boolMd()
                         .withName(name() + " Feedback Active")
                         .withGroupName(name())
                         .withDefault(false)
                         .withID(id(1))),
              DAHDSRMixin(name(idx) + " Feedback", id(2), false)
        {
        }

        std::string name(int idx = -1) const
        {
            if (idx < 0)
                idx = index;
            return "Op " + std::to_string(idx + 1);
        }
        uint32_t id(int f) const { return idBase + idStride * index + f; }

        Param fbLevel;
        Param active;

        std::vector<Param *> params() { return {&fbLevel, &active}; }
    };

    struct MatrixNode : public DAHDSRMixin
    {
        static constexpr uint32_t idBase{30000}, idStride{200};
        int index;
        MatrixNode(size_t idx)
            : index(idx), level(floatMd()
                                    .asPercent()
                                    .withName(name() + " FM Depth")
                                    .withGroupName(name())
                                    .withDefault(0.f)
                                    .withID(id(0))),
              active(boolMd()
                         .withName(name() + " Active")
                         .withGroupName(name())
                         .withFlags(CLAP_PARAM_IS_STEPPED)
                         .withDefault(false)
                         .withID(id(1))),
              DAHDSRMixin(name(idx), id(2), false)
        {
        }

        Param level;
        Param active;

        std::string name(int idx = -1) const
        {
            if (idx < 0)
                idx = index;
            return "Op " + std::to_string(MatrixIndex::sourceIndexAt(idx) + 1) + " to Op " +
                   std::to_string(MatrixIndex::targetIndexAt(idx) + 1);
            ;
        }
        uint32_t id(int f) const { return idBase + idStride * index + f; }
        std::vector<Param *> params() { return {&level, &active}; }
    };

    struct MixerNode : public DAHDSRMixin
    {
        static constexpr uint32_t idBase{20000}, idStride{100};
        int index;
        MixerNode(size_t idx)
            : index(idx), level(floatMd()
                                    .asPercent()
                                    .withName(name() + " Mixer Level")
                                    .withGroupName(name())
                                    .withDefault(idx == 0 ? 1.f : 0.f)
                                    .withID(id(0))),
              active(boolMd()
                         .withName(name() + " Mixer Active")
                         .withGroupName(name())
                         .withFlags(CLAP_PARAM_IS_STEPPED)
                         .withDefault(idx == 0 ? true : false)
                         .withID(id(1))),
              DAHDSRMixin(name(idx), id(2), false)
        {
        }

        std::string name(int idx = -1) const
        {
            if (idx < 0)
                idx = index;
            return "Op " + std::to_string(idx + 1);
        }
        uint32_t id(int f) const { return idBase + idStride * index + f; }

        Param level;
        Param active;

        std::vector<Param *> params() { return {&level, &active}; }
    };

    struct OutputNode : public DAHDSRMixin
    {
        static constexpr uint32_t idBase{500};
        OutputNode()
            : DAHDSRMixin(name(), id(2), true), level(floatMd()
                                                          .asPercent()
                                                          .withName(name() + " Level")
                                                          .withGroupName(name())
                                                          .withDefault(1.0)
                                                          .withID(id(0))),
              pan(floatMd()
                      .asPercentBipolar()
                      .withName(name() + " Pan")
                      .withGroupName(name())
                      .withDefault(0.f)
                      .withID(id(1)))
        {
        }

        std::string name() const { return "Main"; }
        uint32_t id(int f) const { return idBase + f; }

        Param level, pan;

        std::vector<Param *> params()
        {
            return {&level, &pan, &delay, &attack, &hold, &decay, &sustain, &release};
        }
    };

    std::array<SourceNode, numOps> sourceNodes;
    std::array<SelfNode, numOps> selfNodes;
    std::array<MatrixNode, matrixSize> matrixNodes;
    std::array<MixerNode, numOps> mixerNodes;
    OutputNode output;
};
} // namespace baconpaul::fm
#endif // PATCH_H
