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

#ifndef BACONPAUL_SIX_SINES_SYNTH_PATCH_H
#define BACONPAUL_SIX_SINES_SYNTH_PATCH_H

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

namespace baconpaul::six_sines
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
    static md_t intMd() { return md_t().asInt().withFlags(boolFlags); }

    Patch()
        : sourceNodes(scpu::make_array_bind_first_index<SourceNode, numOps>()),
          selfNodes(scpu::make_array_bind_first_index<SelfNode, numOps>()),
          matrixNodes(scpu::make_array_bind_first_index<MatrixNode, matrixSize>()),
          mixerNodes(scpu::make_array_bind_first_index<MixerNode, numOps>()),
          macroNodes(scpu::make_array_bind_first_index<MacroNode, numMacros>())
    {
        auto pushParams = [this](auto &from)
        {
            auto m = from.params();
            params.insert(params.end(), m.begin(), m.end());
            for (auto &p : m)
            {
                if (paramMap.find(p->meta.id) != paramMap.end())
                {
                    SXSNLOG("Duplicate param id " << p->meta.id << " at " << p->meta.name);
                    SXSNLOG("Collision with " << paramMap[p->meta.id]->meta.name);
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
        std::for_each(macroNodes.begin(), macroNodes.end(), pushParams);

        std::sort(params.begin(), params.end(),
                  [](const Param *a, const Param *b)
                  {
                      auto ga = a->meta.groupName;
                      auto gb = b->meta.groupName;
                      if (ga != gb)
                      {
                          if (ga == "Main")
                              return true;
                          if (gb == "Main")
                              return false;

                          return ga < gb;
                      }

                      auto an = a->meta.name;
                      auto bn = b->meta.name;
                      auto ane = an.find("Env ") != std::string::npos;
                      auto bne = bn.find("Env ") != std::string::npos;

                      if (ane != bne)
                      {
                          if (ane)
                              return false;

                          return true;
                      }
                      if (ane && bne)
                          return a->meta.id < b->meta.id;

                      return a->meta.name < b->meta.name;
                  });
    }

    struct LFOMixin
    {
        LFOMixin(const std::string name, int id0)
            : lfoRate(floatMd()
                          .asLfoRate()
                          .withName(name + " LFO Rate")
                          .withGroupName(name)
                          .withDefault(0)
                          .withID(id0 + 1)),
              lfoDeform(floatMd()
                            .asPercentBipolar()
                            .withName(name + " LFO Deform")
                            .withGroupName(name)
                            .withDefault(0)
                            .withID(id0 + 2)),
              lfoShape(intMd()
                           .withName(name + " LFO Shape")
                           .withGroupName(name)
                           .withRange(0, 6)
                           .withDefault(0)
                           .withID(id0 + 3)
                           .withUnorderedMapFormatting({{0, "Sine"},
                                                        {1, "Ramp"},
                                                        {2, "RampNeg"},
                                                        {3, "Tri"},
                                                        {4, "Pulse"},
                                                        {5, "Noise"},
                                                        {6, "SnH"}})),
              lfoActive(boolMd()
                            .withDefault(true)
                            .withID(id0 + 4)
                            .withName(name + " LFO Active")
                            .withGroupName(name))
        {
        }

        Param lfoRate, lfoDeform, lfoShape, lfoActive;

        void appendLFOParams(std::vector<Param *> &res)
        {
            res.push_back(&lfoRate);
            res.push_back(&lfoDeform);
            res.push_back(&lfoShape);
        }
    };

    struct DAHDSRMixin
    {
        using tsr = sst::basic_blocks::modulators::TenSecondRange;
        DAHDSRMixin(const std::string name, int id0, bool longAdsr)
            : delay(floatEnvRateMd()
                        .withName(name + " Env Delay")
                        .withGroupName(name)
                        .withDefault(tsr::etMin)
                        .withID(id0 + 0)),
              attack(floatEnvRateMd()
                         .withName(name + " Env Attack")
                         .withGroupName(name)
                         .withDefault(longAdsr ? -7.f : tsr::etMin)
                         .withID(id0 + 1)),
              hold(floatEnvRateMd()
                       .withName(name + " Env Hold")
                       .withGroupName(name)
                       .withDefault(tsr::etMin)
                       .withID(id0 + 2)),
              decay(floatEnvRateMd()
                        .withName(name + " Env Decay")
                        .withGroupName(name)
                        .withDefault(longAdsr ? 0.f : tsr::etMin)
                        .withID(id0 + 3)),
              sustain(floatMd()
                          .asPercent()
                          .withName(name + " Env Sustain")
                          .withGroupName(name)
                          .withDefault(longAdsr ? 0.7f : 1.f)
                          .withID(id0 + 4)),
              release(floatEnvRateMd()
                          .withName(name + " Env Release")
                          .withGroupName(name)
                          .withDefault(longAdsr ? -2.f : tsr::etMax)
                          .withID(id0 + 5)),
              envPower(boolMd()
                           .withName(name + " Env Power")
                           .withGroupName(name)
                           .withDefault(true)
                           .withID(id0 + 6))
        {
        }

        Param delay, attack, hold, decay, sustain, release, envPower;

        void appendDAHDSRParams(std::vector<Param *> &res)
        {
            res.push_back(&delay);
            res.push_back(&attack);
            res.push_back(&hold);
            res.push_back(&decay);
            res.push_back(&sustain);
            res.push_back(&release);
            res.push_back(&envPower);
        }
    };

    struct SourceNode : public DAHDSRMixin, public LFOMixin
    {
        static constexpr uint32_t idBase{1500}, idStride{250};
        SourceNode(size_t idx)
            : ratio(floatMd()
                        .withRange(-5, 5)
                        .withATwoToTheBFormatting(1, 1, "x")
                        .withName(name(idx) + " Ratio")
                        .withGroupName(name(idx))
                        .withDecimalPlaces(4)
                        .withDefault(0.0)
                        .withID(id(0, idx))),
              active(boolMd()
                         .withGroupName(name(idx))
                         .withName(name(idx) + " Active")
                         .withDefault(idx == 0)
                         .withID(id(1, idx))),
              envToRatio(floatMd()
                             .withRange(-2, 2)
                             .withLinearScaleFormatting("offset")
                             .withName(name(idx) + " Env to Ratio")
                             .withGroupName(name(idx))
                             .withDecimalPlaces(4)
                             .withDefault(0.f)
                             .withID(id(2, idx))),
              lfoToRatio(floatMd()
                             .withRange(-2, 2)
                             .withLinearScaleFormatting("offset")
                             .withName(name(idx) + " LFO to Ratio")
                             .withGroupName(name(idx))
                             .withDecimalPlaces(4)
                             .withDefault(0.f)
                             .withID(id(3, idx))),
              envLfoSum(intMd()
                            .withName(name(idx) + " LFO Env Sum")
                            .withGroupName(name(idx))
                            .withID(id(4, idx))
                            .withRange(0, 1)
                            .withDefault(0)
                            .withUnorderedMapFormatting({{0, "+"}, {1, "x"}})),

              DAHDSRMixin(name(idx), id(100, idx), false), LFOMixin(name(idx), id(45, idx))
        {
        }

        std::string name(int idx) const { return "Op " + std::to_string(idx + 1) + " Source"; }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        Param ratio;
        Param active;

        Param envToRatio;
        Param lfoToRatio;
        Param envLfoSum;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&ratio, &active, &envToRatio, &lfoToRatio, &envLfoSum};
            appendDAHDSRParams(res);
            appendLFOParams(res);
            return res;
        }
    };
    struct SelfNode : public DAHDSRMixin, LFOMixin
    {
        // Once streamed you cant change these bases or the individual ids inside
        static constexpr uint32_t idBase{10000}, idStride{100};
        SelfNode(size_t idx)
            : fbLevel(floatMd()
                          .asPercent()
                          .withName(name(idx) + " Level")
                          .withGroupName(name(idx))
                          .withDefault(0.0)
                          .withID(id(0, idx))),
              active(boolMd()
                         .withName(name(idx) + " Active")
                         .withGroupName(name(idx))
                         .withDefault(false)
                         .withID(id(1, idx))),
              DAHDSRMixin(name(idx), id(2, idx), false), LFOMixin(name(idx), id(15, idx)),
              lfoToFB(floatMd()
                          .asPercentBipolar()
                          .withName(name(idx) + " LFO to FB Amplitude")
                          .withGroupName(name(idx))
                          .withDefault(0.0)
                          .withID(id(25, idx))),
              envLfoSum(intMd()
                            .withName(name(idx) + " LFO Env Sum")
                            .withGroupName(name(idx))
                            .withID(id(26, idx))
                            .withRange(0, 1)
                            .withDefault(0)
                            .withUnorderedMapFormatting({{0, "+"}, {1, "x"}}))

        {
        }

        std::string name(int idx) const { return "Op " + std::to_string(idx + 1) + " Feedback"; }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        Param fbLevel, lfoToFB, envLfoSum;
        Param active;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&fbLevel, &active, &lfoToFB, &envLfoSum};
            appendDAHDSRParams(res);
            appendLFOParams(res);
            return res;
        }
    };

    struct MatrixNode : public DAHDSRMixin, public LFOMixin
    {
        static constexpr uint32_t idBase{30000}, idStride{200};
        MatrixNode(size_t idx)
            : level(floatMd()
                        .asPercent()
                        .withName(name(idx) + " Depth")
                        .withGroupName(name(idx))
                        .withDefault(0.f)
                        .withID(id(0, idx))),
              active(boolMd()
                         .withName(name(idx) + " Active")
                         .withGroupName(name(idx))
                         .withFlags(CLAP_PARAM_IS_STEPPED)
                         .withDefault(false)
                         .withID(id(1, idx))),
              pmOrRM(
                  intMd()
                      .withRange(0, 1)
                      .withDefault(0)
                      .withName(name(idx) + " PM or RM")
                      .withGroupName(name(idx))
                      .withID(id(35, idx))
                      .withUnorderedMapFormatting({{0, std::string() + u8"\U000003C6"}, {1, "A"}})),
              DAHDSRMixin(name(idx), id(2, idx), false), LFOMixin(name(idx), id(14, idx)),
              lfoToDepth(floatMd()
                             .asPercentBipolar()
                             .withName(name(idx) + " LFO to Depth")
                             .withGroupName(name(idx))
                             .withDefault(0.0)
                             .withID(id(25, idx))),
              envLfoSum(intMd()
                            .withName(name(idx) + " LFO Env Sum")
                            .withGroupName(name(idx))
                            .withID(id(26, idx))
                            .withRange(0, 1)
                            .withDefault(0)
                            .withUnorderedMapFormatting({{0, "+"}, {1, "x"}}))
        {
        }

        Param level;
        Param active;
        Param pmOrRM;
        Param lfoToDepth;
        Param envLfoSum;

        std::string name(int idx) const
        {
            return "Op " + std::to_string(MatrixIndex::sourceIndexAt(idx) + 1) + " to Op " +
                   std::to_string(MatrixIndex::targetIndexAt(idx) + 1);
        }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }
        std::vector<Param *> params()
        {
            std::vector<Param *> res{&level, &active, &pmOrRM, &lfoToDepth, &envLfoSum};
            appendDAHDSRParams(res);
            appendLFOParams(res);
            return res;
        }
    };

    struct MixerNode : public DAHDSRMixin, public LFOMixin
    {
        static constexpr uint32_t idBase{20000}, idStride{100};
        MixerNode(size_t idx)
            : level(floatMd()
                        .asPercent()
                        .withName(name(idx) + " Level")
                        .withGroupName(name(idx))
                        .withDefault(idx == 0 ? 1.f : 0.f)
                        .withID(id(0, idx))),
              active(boolMd()
                         .withName(name(idx) + " Active")
                         .withGroupName(name(idx))
                         .withFlags(CLAP_PARAM_IS_STEPPED)
                         .withDefault(idx == 0 ? true : false)
                         .withID(id(1, idx))),
              DAHDSRMixin(name(idx), id(2, idx), false), pan(floatMd()
                                                                 .asPercentBipolar()
                                                                 .withName(name(idx) + " Pan")
                                                                 .withGroupName(name(idx))
                                                                 .withDefault(0.f)
                                                                 .withID(id(15, idx))),
              LFOMixin(name(idx), id(30, idx)),
              lfoToLevel(floatMd()
                             .asPercent()
                             .withName(name(idx) + " LFO to Level")
                             .withGroupName(name(idx))
                             .withID(id(40, idx))
                             .withDefault(0)),
              lfoToPan(floatMd()
                           .asPercent()
                           .withName(name(idx) + " LFO to Pan")
                           .withGroupName(name(idx))
                           .withID(id(41, idx))
                           .withDefault(0))
        {
        }

        std::string name(int idx) const { return "Op " + std::to_string(idx + 1) + " Mixer"; }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        Param level, pan, lfoToLevel, lfoToPan;
        Param active;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&level, &active, &pan, &lfoToLevel, &lfoToPan};
            appendDAHDSRParams(res);
            appendLFOParams(res);
            return res;
        }
    };

    struct MacroNode
    {
        static constexpr uint32_t idBase{40000}, idStride{250};

        MacroNode(size_t idx)
            : level(floatMd()
                        .asPercentBipolar()
                        .withGroupName(name(idx))
                        .withName(name(idx) + " Level")
                        .withID(id(0, idx))
                        .withDefault(0))
        {
        }

        std::string name(int idx) const { return "Macro " + std::to_string(idx + 1); }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        Param level;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&level};
            return res;
        }
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
              velSensitivity(floatMd()
                                 .asPercent()
                                 .withName(name() + " Velocity Sensitivity")
                                 .withGroupName(name())
                                 .withDefault(0.2)
                                 .withID(id(12))),
              playMode(md_t()
                           .asInt()
                           .withRange(0, 2)
                           .withName(name() + " Play Mode")
                           .withGroupName(name())
                           .withDefault(0)
                           .withID(id(13))
                           .withUnorderedMapFormatting({{0, "Poly"}, {1, "Mono"}, {2, "Legato"}}))
        {
        }

        std::string name() const { return "Main"; }
        uint32_t id(int f) const { return idBase + f; }

        Param level, velSensitivity, playMode;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&level, &velSensitivity, &playMode};
            appendDAHDSRParams(res);
            return res;
        }
    };

    std::array<SourceNode, numOps> sourceNodes;
    std::array<SelfNode, numOps> selfNodes;
    std::array<MatrixNode, matrixSize> matrixNodes;
    std::array<MixerNode, numOps> mixerNodes;
    std::array<MacroNode, numMacros> macroNodes;
    OutputNode output;

    void resetToInit();
    std::string toState() const;
    bool fromState(const std::string &);

  private:
    bool fromStateV1(const std::string &);
};
} // namespace baconpaul::six_sines
#endif // PATCH_H
