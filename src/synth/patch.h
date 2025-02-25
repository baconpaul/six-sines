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

#ifndef BACONPAUL_SIX_SINES_SYNTH_PATCH_H
#define BACONPAUL_SIX_SINES_SYNTH_PATCH_H

#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <clap/clap.h>
#include "configuration.h"
#include "sst/cpputils/constructors.h"
#include "sst/cpputils/active_set_overlay.h"
#include "sst/basic-blocks/params/ParamMetadata.h"
#include "sst/basic-blocks/dsp/Lag.h"
#include "sst/basic-blocks/modulators/DAHDSREnvelope.h"
#include "sst/plugininfra/patch-support/patch_base.h"
#include "synth/matrix_index.h"
#include "dsp/sintable.h"

namespace baconpaul::six_sines
{
namespace scpu = sst::cpputils;
namespace pats = sst::plugininfra::patch_support;
using md_t = sst::basic_blocks::params::ParamMetaData;
struct Param : pats::ParamBase, sst::cpputils::active_set_overlay<Param>::participant
{
    Param(const md_t &m) : pats::ParamBase(m) {}

    Param &operator=(const float &val)
    {
        value = val;
        return *this;
    }

    uint64_t adhocFeatures{0};
    enum AdHocFeatureValues : uint64_t
    {
        ENVTIME = 1 << 0,     // tag for ADSR envs we changed version 2-3
        TRIGGERMODE = 1 << 1, // trigger mode for when we nuked voice
        WAVEFORM = 1 << 2,
        SOLO = 1 << 3
    };

    bool isTemposynced() const
    {
        if (tempoSyncPartner)
            return tempoSyncPartner->value;

        return false;
    }

    Param *tempoSyncPartner{nullptr};

    sst::basic_blocks::dsp::LinearLag<float, false> lag;
};

struct Patch : pats::PatchBase<Patch, Param>
{
    static constexpr uint32_t patchVersion{9};
    static constexpr const char *id{"org.baconpaul.six-sines"};

    static constexpr uint32_t floatFlags{CLAP_PARAM_IS_AUTOMATABLE};
    static constexpr uint32_t boolFlags{CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED};

    static md_t floatMd() { return md_t().asFloat().withFlags(floatFlags); }
    static md_t floatEnvRateMd()
    {
        return md_t().asFloat().withFlags(floatFlags).as25SecondExpTime();
    }
    static md_t boolMd() { return md_t().asBool().withFlags(boolFlags); }
    static md_t intMd() { return md_t().asInt().withFlags(boolFlags); }

    Patch()
        : pats::PatchBase<Patch, Param>(),
          sourceNodes(scpu::make_array_bind_first_index<SourceNode, numOps>()),
          selfNodes(scpu::make_array_bind_first_index<SelfNode, numOps>()),
          matrixNodes(scpu::make_array_bind_first_index<MatrixNode, matrixSize>()),
          mixerNodes(scpu::make_array_bind_first_index<MixerNode, numOps>()),
          macroNodes(scpu::make_array_bind_first_index<MacroNode, numMacros>()), fineTuneMod(),
          mainPanMod()
    {
        MatrixIndex::initialize();
        auto pushParams = [this](auto &from) { this->pushMultipleParams(from.params()); };

        pushParams(output);
        std::for_each(sourceNodes.begin(), sourceNodes.end(), pushParams);
        std::for_each(selfNodes.begin(), selfNodes.end(), pushParams);
        std::for_each(mixerNodes.begin(), mixerNodes.end(), pushParams);
        std::for_each(matrixNodes.begin(), matrixNodes.end(), pushParams);
        std::for_each(macroNodes.begin(), macroNodes.end(), pushParams);

        pushParams(fineTuneMod);
        pushParams(mainPanMod);

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
                          .asLfoRate(-7, 11)
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
                                                        {2, "Sawtooth"},
                                                        {3, "Triangle"},
                                                        {4, "Pulse"},
                                                        {5, "Noise"},
                                                        {6, "S and H"}})),
              lfoActive(boolMd()
                            .withDefault(true)
                            .withID(id0 + 4)
                            .withName(name + " LFO Active")
                            .withGroupName(name)),
              tempoSync(md_t() // non-automatable
                            .asBool()
                            .withID(id0 + 5)
                            .withName(name + " Temposync")
                            .withGroupName(name)
                            .withDefault(false)),
              lfoBipolar(boolMd()
                             .withDefault(true)
                             .withID(id0 + 6)
                             .withName(name + " Bipolar")
                             .withGroupName(name)),
              lfoIsEnveloped(boolMd()
                                 .withDefault(false)
                                 .withID(id0 + 7)
                                 .withName(name + " Is Enveloped")
                                 .withGroupName(name)),
              lfoStartPhase(floatMd()
                                .withDefault(0)
                                .withID(id0 + 8)
                                .withName(name + " Start Phase")
                                .withGroupName(name)
                                .withRange(0., 1.)
                                .withLinearScaleFormatting(""))
        {
            lfoRate.tempoSyncPartner = &tempoSync;
        }

        Param lfoRate, lfoDeform, lfoShape, lfoActive, tempoSync, lfoBipolar, lfoIsEnveloped,
            lfoStartPhase;

        void appendLFOParams(std::vector<Param *> &res)
        {
            res.push_back(&lfoRate);
            res.push_back(&lfoDeform);
            res.push_back(&lfoShape);
            res.push_back(&tempoSync);
            res.push_back(&lfoBipolar);
            res.push_back(&lfoIsEnveloped);
            res.push_back(&lfoStartPhase);
        }

        enum LFOTargets
        {
            LFO_RATE = 50,
            LFO_DEFORM = 51,
            LFO_STARTPHASE = 52
        };

        void appendLFOTargetName(std::vector<std::pair<int32_t, std::string>> &res)
        {
            res.emplace_back(-1, "");
            res.emplace_back(LFOTargets::LFO_RATE, "LFO Rate");
            res.emplace_back(LFOTargets::LFO_DEFORM, "LFO Deform");
            res.emplace_back(LFOTargets::LFO_STARTPHASE, "LFO Phase");
        }
    };

    struct DAHDSRMixin
    {

        DAHDSRMixin(const std::string name, int id0, bool longAdsr, bool alwaysAdd = false,
                    int id1 = -1)
            : delay(floatEnvRateMd()
                        .withName(name + " Env Delay")
                        .withGroupName(name)
                        .withDefault(0.f)
                        .withID(id0 + 0)),
              attack(floatEnvRateMd()
                         .withName(name + " Env Attack")
                         .withGroupName(name)
                         .withDefault(longAdsr ? 0.05 : 0.f)
                         .withID(id0 + 1)),
              hold(floatEnvRateMd()
                       .withName(name + " Env Hold")
                       .withGroupName(name)
                       .withDefault(0.f)
                       .withID(id0 + 2)),
              decay(floatEnvRateMd()
                        .withName(name + " Env Decay")
                        .withGroupName(name)
                        .withDefault(longAdsr ? 0.3 : 0.f)
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
                          .withDefault(longAdsr ? 0.4 : 1.f)
                          .withID(id0 + 5)),
              envPower(boolMd()
                           .withName(name + " Env Power")
                           .withGroupName(name)
                           .withDefault(true)
                           .withID(id0 + 6)),
              aShape(floatMd()
                         .asPercentBipolar()
                         .withName(name + " Attack Shape")
                         .withGroupName(name)
                         .withDefault(0)
                         .withID(id0 + 7)),
              dShape(floatMd()
                         .asPercentBipolar()
                         .withName(name + " Decay Shape")
                         .withGroupName(name)
                         .withDefault(0)
                         .withID(id0 + 8)),
              rShape(floatMd()
                         .asPercentBipolar()
                         .withName(name + " Release Shape")
                         .withGroupName(name)
                         .withDefault(0)
                         .withID(id0 + 9)),
              triggerMode(intMd()
                              .withRange(0, 3)
                              .withName(name + " Env Trigger Mode")
                              .withGroupName(name)
                              .withDefault(3)
                              .withID(id0 + 10)
                              .withUnorderedMapFormatting({{0, "Gate Start"},
                                                           {1, "Voice Start"},
                                                           {3, "Key Press"},
                                                           {2, "Patch Default"}})),
              envIsMultiplcative(boolMd()
                                     .withName(name + " Env is Multiplicative")
                                     .withGroupName(name)
                                     .withDefault(!alwaysAdd)
                                     .withID(id0 + 11)
                                     .withUnorderedMapFormatting({{0, "Add"}, {1, "Scale"}})),
              envIsOneShot(boolMd()
                               .withName(name + " Is OneShot")
                               .withGroupName(name)
                               .withDefault(false)
                               .withID(id0 + 12)),
              envTriggersFromZero(boolMd()
                                      .withName(name + " Triggers from Zero")
                                      .withGroupName(name)
                                      .withDefault(false)
                                      .withID((id1 > 0 ? id1 : (id0 + 13)) + 0))
        {
            delay.adhocFeatures = Param::AdHocFeatureValues::ENVTIME;
            attack.adhocFeatures = Param::AdHocFeatureValues::ENVTIME;
            hold.adhocFeatures = Param::AdHocFeatureValues::ENVTIME;
            decay.adhocFeatures = Param::AdHocFeatureValues::ENVTIME;
            release.adhocFeatures = Param::AdHocFeatureValues::ENVTIME;

            triggerMode.adhocFeatures = Param::AdHocFeatureValues::TRIGGERMODE;
        }

        Param delay, attack, hold, decay, sustain, release, envPower;
        Param aShape, dShape, rShape, triggerMode, envIsMultiplcative, envIsOneShot,
            envTriggersFromZero;

        void appendDAHDSRParams(std::vector<Param *> &res)
        {
            res.push_back(&delay);
            res.push_back(&attack);
            res.push_back(&hold);
            res.push_back(&decay);
            res.push_back(&sustain);
            res.push_back(&release);
            res.push_back(&envPower);
            res.push_back(&aShape);
            res.push_back(&dShape);
            res.push_back(&rShape);
            res.push_back(&triggerMode);
            res.push_back(&envIsMultiplcative);
            res.push_back(&envIsOneShot);
            res.push_back(&envTriggersFromZero);
        }

        enum DAHDSRTargets
        {
            ENV_DELAY = 45,
            ENV_ATTACK = 40,
            ENV_HOLD = 41,
            ENV_DECAY = 42,
            ENV_SUSTAIN = 43,
            ENV_RELEASE = 44
        };

        void appendDAHDSRTargetName(std::vector<std::pair<int32_t, std::string>> &res)
        {
            res.emplace_back(-1, "");
            res.emplace_back(ENV_DELAY, "Env Delay");
            res.emplace_back(ENV_ATTACK, "Env Attack");
            res.emplace_back(ENV_HOLD, "Env Hold");
            res.emplace_back(ENV_DECAY, "Env Decay");
            res.emplace_back(ENV_SUSTAIN, "Env Sustain");
            res.emplace_back(ENV_RELEASE, "Env Release");
        }
    };

    struct ModulationMixin
    {
        ModulationMixin(const std::string name, int id0)
            : modsource(scpu::make_array_lambda<Param, numModsPer>(
                  [id0, name](int i)
                  {
                      return md_t()
                          .asInt()
                          .withRange(0, 2048)
                          .withID(id0 + 2 * i)
                          .withName(name + " Mod Source " + std::to_string(i))
                          .withGroupName(name)
                          .withDefault(0);
                  })),
              moddepth(scpu::make_array_lambda<Param, numModsPer>(
                  [id0, name](int i)
                  {
                      return floatMd()
                          .asPercentBipolar()
                          .withID(id0 + 2 * i + 1)
                          .withName(name + " Mod Depth " + std::to_string(i))
                          .withGroupName(name)
                          .withDefault(0);
                  }))
        {
        }
        std::array<Param, numModsPer> modsource, moddepth;

        void appendModulationParams(std::vector<Param *> &res)
        {
            for (int i = 0; i < numModsPer; ++i)
            {
                res.push_back(&modsource[i]);
                res.push_back(&moddepth[i]);
            }
        }
    };

    struct SourceNode : public DAHDSRMixin, public LFOMixin, public ModulationMixin
    {
        static constexpr uint32_t idBase{1500}, idStride{250};

        // These stream
        enum TargetID : int32_t
        {
            SKIP = -1,
            NONE = 0,
            DIRECT = 10,
            DIRECT_FINE = 11,
            STARTING_PHASE = 15,
            ENV_DEPTH_ATTEN = 20,
            LFO_DEPTH_ATTEN = 30,
        };

        std::vector<std::pair<int32_t, std::string>> targetList{
            {TargetID::NONE, "Off"},
            {TargetID::SKIP, ""},
            {TargetID::DIRECT, "Ratio"},
            {TargetID::DIRECT_FINE, "Ratio (Fine)"},
            {TargetID::STARTING_PHASE, "Phase"},
            {TargetID::ENV_DEPTH_ATTEN, "Env Atten"},
            {TargetID::LFO_DEPTH_ATTEN, "LFO Atten"},

        };

        SourceNode(size_t idx)
            : ratio(floatMd()
                        .withRange(-5, 5)
                        .withATwoToTheBFormatting(1, 1, "x")
                        .withName(name(idx) + " Ratio")
                        .withGroupName(name(idx))
                        .withDecimalPlaces(4)
                        .withDefault(0.0)
                        .withID(id(0, idx))
                        .withFeature(md_t::Features::BELOW_ONE_IS_INVERSE_FRACTION)
                        .withFeature(md_t::Features::ALLOW_FRACTIONAL_TYPEINS)),
              active(boolMd()
                         .withGroupName(name(idx))
                         .withName(name(idx) + " Active")
                         .withDefault(idx == 0)
                         .withID(id(1, idx))),
              envToRatio(floatMd()
                             .withRange(-5, 5)
                             .withATwoToTheBFormatting(1, 1, "offset")
                             .withName(name(idx) + " Env to Ratio")
                             .withGroupName(name(idx))
                             .withDecimalPlaces(4)
                             .withDefault(0.f)
                             .withID(id(2, idx))
                             .withFeature(md_t::Features::BELOW_ONE_IS_INVERSE_FRACTION)
                             .withFeature(md_t::Features::ALLOW_FRACTIONAL_TYPEINS)),
              lfoToRatio(floatMd()
                             .withRange(-5, 5)
                             .withATwoToTheBFormatting(1, 1, "offset")
                             .withName(name(idx) + " LFO to Ratio")
                             .withGroupName(name(idx))
                             .withDecimalPlaces(4)
                             .withDefault(0.f)
                             .withID(id(3, idx))
                             .withFeature(md_t::Features::BELOW_ONE_IS_INVERSE_FRACTION)
                             .withFeature(md_t::Features::ALLOW_FRACTIONAL_TYPEINS)),
              envToRatioFine(floatMd()
                                 .withRange(-200, 200)
                                 .withLinearScaleFormatting(" cents")
                                 .withName(name(idx) + " Env to Ratio (Fine)")
                                 .withGroupName(name(idx))
                                 .withDecimalPlaces(4)
                                 .withDefault(0.f)
                                 .withID(id(10, idx))),
              lfoToRatioFine(floatMd()
                                 .withRange(-200, 200)
                                 .withLinearScaleFormatting(" cents")
                                 .withName(name(idx) + " LFO to Ratio (Fine)")
                                 .withGroupName(name(idx))
                                 .withDecimalPlaces(4)
                                 .withDefault(0.f)
                                 .withID(id(11, idx))),
              waveForm(intMd()
                           .withName(name(idx) + " Waveform")
                           .withGroupName(name(idx))
                           .withID(id(5, idx))
                           .withRange(0, SinTable::WaveForm::NUM_WAVEFORMS - 1)
                           .withDefault(0)
                           .withUnorderedMapFormatting({
                               {SinTable::WaveForm::SIN, "Sin"},
                               {SinTable::WaveForm::SIN_FIFTH, "Sin^5 x"},
                               {SinTable::WaveForm::SQUARISH, "Squarish"},
                               {SinTable::WaveForm::SAWISH, "Sawish"},
                               {SinTable::WaveForm::TRIANGLE, "Triangle"},
                               {SinTable::WaveForm::SIN_OF_CUBED, "Sin(x^3)"},
                               {SinTable::WaveForm::TX2, "TX 2"},
                               {SinTable::WaveForm::TX3, "TX 3"},
                               {SinTable::WaveForm::TX4, "TX 4"},
                               {SinTable::WaveForm::TX5, "TX 5"},
                               {SinTable::WaveForm::TX6, "TX 6"},
                               {SinTable::WaveForm::TX7, "TX 7"},
                               {SinTable::WaveForm::TX8, "TX 8"},
                               {SinTable::WaveForm::SPIKY_TX2, "Spiky TX 2"},
                               {SinTable::WaveForm::SPIKY_TX4, "Spiky TX 4"},
                               {SinTable::WaveForm::SPIKY_TX6, "Spiky TX 6"},
                               {SinTable::WaveForm::SPIKY_TX8, "Spiky TX 8"},
                               {SinTable::WaveForm::HANN_WINDOW, "Hann"},
                               {SinTable::WaveForm::BLACKMAN_HARRIS_WINDOW, "Blackman Harris"},
                               {SinTable::WaveForm::HALF_BLACKMAN_HARRIS_WINDOW,
                                std::string() + u8"\U000000BD" + " Blackman Harris"},
                               {SinTable::WaveForm::TUKEY_WINDOW, "Tukey"},
                           })),

              keyTrack(boolMd()
                           .withName(name(idx) + " Keytrack")
                           .withGroupName(name(idx))
                           .withID(id(6, idx))
                           .withDefault(true)),
              keyTrackValueIsLow(boolMd()
                                     .withName(name(idx) + " Keytrack Frequency is Low Frequency")
                                     .withGroupName(name(idx))
                                     .withID(id(17, idx))
                                     .withDefault(false)),
              keyTrackValue(floatMd()
                                .withName(name(idx) + " Absolute Frequency at Ratio=1")
                                .withGroupName(name(idx))
                                .withDefault(0)
                                .withRange(-70, 70)
                                .withSemitoneZeroAt400Formatting()
                                .withID(id(7, idx))),
              keyTrackLowFrequencyValue(
                  floatMd()
                      .withName(name(idx) + " Keytrack Frequency at Ratio=1 (Low Frequency)")
                      .withGroupName(name(idx))
                      .withRange(0, 10)
                      .withLinearScaleFormatting("Hz")
                      .withID(id(18, idx))
                      .withDefault(1)),
              startingPhase(floatMd()
                                .withName(name(idx) + " Phase")
                                .withGroupName(name(idx))
                                .asPercent()
                                .withDefault(0)
                                .withID(id(8, idx))),
              octTranspose(intMd()
                               .withName(name(idx) + " Octave Transpose")
                               .withGroupName(name(idx))
                               .withRange(-3, 3)
                               .withDefault(0)
                               .withID(id(9, idx))),
              unisonParticipation(intMd()
                                      .withRange(0, 3)
                                      .withDefault(3)
                                      .withID(id(14, idx))
                                      .withName(name(idx) + " Unison Participation")
                                      .withGroupName(name(idx))
                                      .withUnorderedMapFormatting({{0, "Skip Unison"},
                                                                   {1, "Tuning Only"},
                                                                   {2, "Pan Only"},
                                                                   {3, "Tuning and Pan"}})),
              unisonToMain(intMd()
                               .withRange(0, 3)
                               .withDefault(0)
                               .withID(id(15, idx))
                               .withName(name(idx) + " Unison Main Output")
                               .withGroupName(name(idx))
                               .withUnorderedMapFormatting({{0, "All to Main"},
                                                            {1, "Center to Main"},
                                                            {2, "Sides to Main"},
                                                            {3, "None to Main"}})),
              unisonToOpOut(intMd()
                                .withRange(0, 2)
                                .withDefault(0)
                                .withID(id(16, idx))
                                .withName(name(idx) + " Unison Operator Output Output")
                                .withGroupName(name(idx))
                                .withUnorderedMapFormatting({{0, "All to Op Output"},
                                                             {1, "Center to Op Output"},
                                                             {2, "Sides to Op Output"}})),

              DAHDSRMixin(name(idx), id(100, idx), false), LFOMixin(name(idx), id(45, idx)),
              ModulationMixin(name(idx), id(150, idx)),
              modtarget(scpu::make_array_lambda<Param, numModsPer>(
                  [this, idx](int i)
                  {
                      return md_t()
                          .withName(name(idx) + " Mod Target " + std::to_string(i))
                          .withGroupName(name(idx))
                          .withRange(0, 2000)
                          .withDefault(TargetID::NONE)
                          .withID(id(160 + i, idx));
                  }))

        {
            index = idx;
            waveForm.adhocFeatures = Param::AdHocFeatureValues::WAVEFORM;
            appendLFOTargetName(targetList);
            appendDAHDSRTargetName(targetList);
        }

        std::string name(int idx) const { return "Op " + std::to_string(idx + 1) + " Source"; }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        int index{-1};
        std::string name() const
        {
            if (index < 0)
                throw std::runtime_error("Only call this post setup");
            return name(index);
        }

        Param ratio;
        Param active;

        Param envToRatio, envToRatioFine;
        Param lfoToRatio, lfoToRatioFine;

        Param waveForm;

        Param keyTrack, keyTrackValue, keyTrackLowFrequencyValue, keyTrackValueIsLow;

        Param startingPhase, octTranspose;

        Param unisonParticipation, unisonToMain, unisonToOpOut;

        std::array<Param, numModsPer> modtarget;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&ratio,
                                     &active,
                                     &envToRatio,
                                     &lfoToRatio,
                                     &waveForm,
                                     &keyTrack,
                                     &keyTrackLowFrequencyValue,
                                     &keyTrackValueIsLow,
                                     &keyTrackValue,
                                     &startingPhase,
                                     &octTranspose,
                                     &envToRatioFine,
                                     &lfoToRatioFine,
                                     &unisonParticipation,
                                     &unisonToMain,
                                     &unisonToOpOut};
            for (int i = 0; i < numModsPer; ++i)
                res.push_back(&modtarget[i]);
            appendDAHDSRParams(res);
            appendLFOParams(res);
            appendModulationParams(res);
            return res;
        }
    };
    struct SelfNode : public DAHDSRMixin, LFOMixin, ModulationMixin
    {
        // These stream
        enum TargetID : int32_t
        {
            SKIP = -1,
            NONE = 0,
            DIRECT = 10,
            DEPTH_ATTEN = 20,
            LFO_DEPTH_ATTEN = 30
        };

        std::vector<std::pair<int32_t, std::string>> targetList{
            {TargetID::NONE, "Off"},
            {TargetID::SKIP, ""},
            {TargetID::DIRECT, "Feedback Level"},
            {TargetID::DEPTH_ATTEN, "Env Atten"},
            {TargetID::LFO_DEPTH_ATTEN, "LFO Atten"}};

        // Once streamed you cant change these bases or the individual ids inside
        static constexpr uint32_t idBase{10000}, idStride{100};
        SelfNode(size_t idx)
            : fbLevel(floatMd()
                          .asPercentBipolar()
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
              envToFB(floatMd()
                          .asPercentBipolar()
                          .withName(name(idx) + " Env to FB Amplitude")
                          .withGroupName(name(idx))
                          .withID(id(27, idx))
                          .withDefault(0)),
              overdrive(boolMd()
                            .withName(name(idx) + " Overdrive")
                            .withGroupName(name(idx))
                            .withDefault(false)
                            .withID(id(28, idx))),
              ModulationMixin(name(idx), id(40, idx)),
              modtarget(scpu::make_array_lambda<Param, numModsPer>(
                  [this, idx](int i)
                  {
                      return md_t()
                          .withName(name(idx) + " Mod Target " + std::to_string(i))
                          .withGroupName(name(idx))
                          .withRange(0, 2000)
                          .withDefault(TargetID::NONE)
                          .withID(id(55 + i, idx));
                  }))
        {
            index = idx;
            appendLFOTargetName(targetList);
            appendDAHDSRTargetName(targetList);
        }

        std::string name(int idx) const { return "Op " + std::to_string(idx + 1) + " Feedback"; }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        int index{-1};
        std::string name() const
        {
            if (index < 0)
                throw std::runtime_error("Only call this post setup");
            return name(index);
        }

        Param fbLevel, lfoToFB, envToFB, overdrive;
        Param active;

        std::array<Param, numModsPer> modtarget;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&fbLevel, &active, &lfoToFB, &envToFB, &overdrive};
            appendDAHDSRParams(res);
            appendLFOParams(res);

            for (int i = 0; i < numModsPer; ++i)
                res.push_back(&modtarget[i]);
            appendModulationParams(res);

            return res;
        }
    };

    struct MatrixNode : public DAHDSRMixin, public LFOMixin, public ModulationMixin
    {
        static constexpr uint32_t idBase{30000}, idStride{200};

        enum TargetID : int32_t
        {
            SKIP = -1,
            NONE = 0,
            DIRECT = 10,
            DEPTH_ATTEN = 20,
            LFO_DEPTH_ATTEN = 30
        };

        std::vector<std::pair<int32_t, std::string>> targetList{
            {TargetID::NONE, "Off"},
            {TargetID::SKIP, ""},
            {TargetID::DIRECT, "Level"},
            {TargetID::DEPTH_ATTEN, "Env Atten"},
            {TargetID::LFO_DEPTH_ATTEN, "LFO Atten"},
        };

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
              modulationMode(intMd()
                                 .withRange(0, 3)
                                 .withDefault(0)
                                 .withName(name(idx) + " modulation mode")
                                 .withGroupName(name(idx))
                                 .withID(id(35, idx))
                                 .withUnorderedMapFormatting({{0, "Phase Modulation"},
                                                              {1, "Ring Modulation"},
                                                              {2, "Linear FM"},
                                                              {3, "Exponential FM"}})),
              DAHDSRMixin(name(idx), id(2, idx), false, false, id(50, idx)),
              LFOMixin(name(idx), id(14, idx)),
              lfoToDepth(floatMd()
                             .asPercentBipolar()
                             .withName(name(idx) + " LFO to Depth")
                             .withGroupName(name(idx))
                             .withDefault(0.0)
                             .withID(id(25, idx))),
              envToLevel(floatMd()
                             .asPercentBipolar()
                             .withName(name(idx) + " EnvToLevel")
                             .withGroupName(name(idx))
                             .withID(id(27, idx))
                             .withDefault(1.f)),
              overdrive(boolMd()
                            .withName(name(idx) + " Overdrive")
                            .withGroupName(name(idx))
                            .withDefault(false)
                            .withID(id(28, idx))),

              ModulationMixin(name(idx), id(70, idx)),
              modtarget(scpu::make_array_lambda<Param, numModsPer>(
                  [this, idx](int i)
                  {
                      return md_t()
                          .withName(name(idx) + " Mod Target " + std::to_string(i))
                          .withGroupName(name(idx))
                          .withRange(0, 2000)
                          .withDefault(TargetID::NONE)
                          .withID(id(80 + i, idx));
                  }))

        {
            index = idx;
            appendLFOTargetName(targetList);
            appendDAHDSRTargetName(targetList);
        }

        Param level;
        Param active;
        Param modulationMode;
        Param lfoToDepth;
        Param envToLevel;
        Param overdrive;

        std::array<Param, numModsPer> modtarget;

        // Use *this* version in constructor
        std::string name(int idx) const
        {
            return "Op " + std::to_string(MatrixIndex::sourceIndexAt(idx) + 1) + " to Op " +
                   std::to_string(MatrixIndex::targetIndexAt(idx) + 1);
        }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        int index{-1};
        std::string name() const
        {
            if (index < 0)
                throw std::runtime_error("Only call this post setup");
            return name(index);
        }

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&level,      &active,     &modulationMode,
                                     &lfoToDepth, &envToLevel, &overdrive};
            appendDAHDSRParams(res);
            appendLFOParams(res);

            for (int i = 0; i < numModsPer; ++i)
                res.push_back(&modtarget[i]);
            appendModulationParams(res);

            return res;
        }
    };

    struct MixerNode : public DAHDSRMixin, public LFOMixin, public ModulationMixin
    {
        static constexpr uint32_t idBase{20000}, idStride{100};

        enum TargetID : int32_t
        {
            SKIP = -1,
            NONE = 0,
            DIRECT = 10,
            PAN = 15,
            DEPTH_ATTEN = 20,
            LFO_DEPTH_ATTEN = 30,
            LFO_DEPTH_PAN_ATTEN = 35,
        };

        std::vector<std::pair<int32_t, std::string>> targetList{
            {TargetID::NONE, "Off"},
            {TargetID::SKIP, ""},
            {TargetID::DIRECT, "Amplitude"},
            {TargetID::PAN, "Pan"},
            {TargetID::SKIP, ""},
            {TargetID::DEPTH_ATTEN, "Env Atten"},
            {TargetID::LFO_DEPTH_ATTEN, std::string() + "LFO" + u8"\U00002192" + "Amp Atten"},
            {TargetID::LFO_DEPTH_PAN_ATTEN, std::string() + "LFO" + u8"\U00002192" + "Pan Atten"},

        };

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
              DAHDSRMixin(name(idx), id(2, idx), false, false, id(20, idx)),
              pan(floatMd()
                      .asPercentBipolar()
                      .withName(name(idx) + " Pan")
                      .withGroupName(name(idx))
                      .withDefault(0.f)
                      .withID(id(15, idx))),
              LFOMixin(name(idx), id(30, idx)),
              lfoToLevel(floatMd()
                             .asPercentBipolar()
                             .withName(name(idx) + " LFO to Level")
                             .withGroupName(name(idx))
                             .withID(id(40, idx))
                             .withDefault(0)),
              lfoToPan(floatMd()
                           .asPercentBipolar()
                           .withName(name(idx) + " LFO to Pan")
                           .withGroupName(name(idx))
                           .withID(id(41, idx))
                           .withDefault(0)),
              envToLevel(floatMd()
                             .asPercentBipolar()
                             .withName(name(idx) + " EnvToLevel")
                             .withGroupName(name(idx))
                             .withID(id(42, idx))
                             .withDefault(1.f)),
              solo(boolMd()
                       .withName(name(idx) + " Solo")
                       .withGroupName(name(idx))
                       .withID(id(43, idx))
                       .withDefault(false)),
              ModulationMixin(name(idx), id(50, idx)),
              modtarget(scpu::make_array_lambda<Param, numModsPer>(
                  [this, idx](int i)
                  {
                      return md_t()
                          .withName(name(idx) + " Mod Target " + std::to_string(i))
                          .withGroupName(name(idx))
                          .withRange(0, 2000)
                          .withDefault(TargetID::NONE)
                          .withID(id(60 + i, idx));
                  }))

        {
            index = idx;
            appendLFOTargetName(targetList);
            appendDAHDSRTargetName(targetList);
            solo.adhocFeatures = Param::AdHocFeatureValues::SOLO;
        }

        std::string name(int idx) const { return "Op " + std::to_string(idx + 1) + " Mixer"; }
        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        int index{-1};
        std::string name() const
        {
            if (index < 0)
                throw std::runtime_error("Only call this post setup");
            return name(index);
        }

        Param level, pan, lfoToLevel, lfoToPan, envToLevel, solo;
        Param active;
        std::array<Param, numModsPer> modtarget;
        bool isMutedDueToSoloAway{false};

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&level,    &active,     &pan, &lfoToLevel,
                                     &lfoToPan, &envToLevel, &solo};
            appendDAHDSRParams(res);
            appendLFOParams(res);

            for (int i = 0; i < numModsPer; ++i)
                res.push_back(&modtarget[i]);
            appendModulationParams(res);

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

    struct ModulationOnlyNode : public DAHDSRMixin, public ModulationMixin, public LFOMixin
    {
        static constexpr uint32_t idBase{50000}, idStride{1000};

        enum TargetID : int32_t
        {
            SKIP = -1,
            NONE = 0,
            DIRECT = 10,
            ENVDEP_DIR = 14,
            LFODEP_DIR = 17,
            DEPTH_ATTEN = 20,
            LFO_DEPTH_ATTEN = 25
        };

        std::vector<std::pair<int32_t, std::string>> targetList{
            {TargetID::NONE, "Off"},
            {TargetID::SKIP, ""},
            {TargetID::DIRECT, "TARGET"},
            {TargetID::ENVDEP_DIR, "Env Depth"},
            {TargetID::LFODEP_DIR, "LFO Depth"},
            {TargetID::SKIP, ""},
            {TargetID::DEPTH_ATTEN, "Env Atten"},
            {TargetID::LFO_DEPTH_ATTEN, "LFO Atten"},
        };

        ModulationOnlyNode(const std::string &opName, const std::string &outn, int idx)
            : DAHDSRMixin(opName, id(0, idx), false, true),

              ModulationMixin(opName, id(100, idx)),
              modtarget(scpu::make_array_lambda<Param, numModsPer>(
                  [this, opName, idx](int i)
                  {
                      return md_t()
                          .withName(opName + " Mod Target " + std::to_string(i))
                          .withGroupName(opName)
                          .withRange(0, 2000)
                          .withDefault(TargetID::NONE)
                          .withID(id(200 + i, idx));
                  })),
              LFOMixin(opName, id(300, idx)), lfoDepth(floatMd()
                                                           .asPercentBipolar()
                                                           .withName(opName + " LFO Depth")
                                                           .withGroupName(opName)
                                                           .withDefault(0)
                                                           .withID(id(400, idx))),
              envDepth(floatMd()
                           .asPercentBipolar()
                           .withName(opName + " Env Depth")
                           .withGroupName(opName)
                           .withID(id(401, idx))
                           .withDefault(0.f))

        {
            for (auto &[id, n] : targetList)
            {
                if (id == TargetID::DIRECT)
                    n = outn;
            }
            cacheName = opName;
            appendLFOTargetName(targetList);
            appendDAHDSRTargetName(targetList);
        }

        std::string cacheName{};
        std::string name() const { return cacheName; }

        uint32_t id(int f, int idx) const { return idBase + idStride * idx + f; }

        Param lfoDepth, envDepth;

        std::array<Param, numModsPer> modtarget;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&lfoDepth, &envDepth};
            appendDAHDSRParams(res);

            for (int i = 0; i < numModsPer; ++i)
                res.push_back(&modtarget[i]);
            appendModulationParams(res);
            appendLFOParams(res);

            return res;
        }
    };

    struct FineTuneNode : ModulationOnlyNode
    {
        enum localTarget
        {
            COARSE = 11
        };
        FineTuneNode()
            : ModulationOnlyNode("Fine Tune Mod", "Fine Tune", 0),
              coarseTune(floatMd()
                             .withName(name() + " Coarse Tuning")
                             .withGroupName(name())
                             .withRange(-24, 24)
                             .withLinearScaleFormatting("semitones")
                             .withID(id(402, 0))),
              lfoCoarseDepth(floatMd()
                                 .asPercentBipolar()
                                 .withName(name() + " Coarse LFO Depth")
                                 .withGroupName(name())
                                 .withID(id(403, 0))
                                 .withDefault(0.f)),
              envCoarseDepth(floatMd()
                                 .asPercentBipolar()
                                 .withName(name() + " Env Coarse Depth")
                                 .withGroupName(name())
                                 .withID(id(404, 0))
                                 .withDefault(0.f))
        {
            auto it = targetList.begin();
            while (it != targetList.end() && it->first != DIRECT)
                ++it;

            targetList.insert(it + 1, {localTarget::COARSE, "Coarse Tune"});
        }

        std::vector<Param *> params()
        {
            auto res = ModulationOnlyNode::params();

            res.push_back(&coarseTune);
            res.push_back(&lfoCoarseDepth);
            res.push_back(&envCoarseDepth);
            return res;
        }

        Param coarseTune;

        Param lfoCoarseDepth, envCoarseDepth;
    };

    struct MainPanNode : ModulationOnlyNode
    {
        MainPanNode() : ModulationOnlyNode("Main Pan Mod", "Main Pan", 1) {}
    };

    struct OutputNode : public DAHDSRMixin, public ModulationMixin, public LFOMixin
    {
        static constexpr uint32_t idBase{500};

        enum TargetID : int32_t
        {
            SKIP = -1,
            NONE = 0,
            DIRECT = 10,
            PAN = 15,
            DEPTH_ATTEN = 20,
            LFO_DEPTH_ATTEN = 25,
        };

        std::vector<std::pair<int32_t, std::string>> targetList{
            {TargetID::NONE, "Off"},
            {TargetID::SKIP, ""},
            {TargetID::DIRECT, "Amplitude"},
            {TargetID::DEPTH_ATTEN, "Env Atten"},
            {TargetID::LFO_DEPTH_ATTEN, "LFO Atten"},
            {TargetID::PAN, "Pan"},
        };

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
                                 .withID(id(22))),
              playMode(md_t()
                           .asInt()
                           .withRange(0, 1)
                           .withName(name() + " Play Mode")
                           .withGroupName(name())
                           .withDefault(0)
                           .withID(id(23))
                           .withUnorderedMapFormatting({{0, "Poly"}, {1, "Mono"}})),
              bendUp(floatMd()
                         .withRange(0, 48)
                         .withName(name() + " PB Up")
                         .withGroupName(name())
                         .withDefault(2)
                         .withID(id(24))
                         .withLinearScaleFormatting("")),
              bendDown(floatMd()
                           .withRange(0, 48)
                           .withName(name() + " PB Up")
                           .withGroupName(name())
                           .withDefault(2)
                           .withID(id(25))
                           .withLinearScaleFormatting("")),
              polyLimit(md_t()
                            .asInt()
                            .withRange(1, maxVoices)
                            .withName(name() + " PolyLimit")
                            .withGroupName(name())
                            .withDefault(maxVoices)
                            .withID(id(26))
                            .withLinearScaleFormatting("")),
              defaultTrigger(
                  md_t()
                      .asInt()
                      .withRange(0, 1)
                      .withName(name() + " Default Env Mode")
                      .withGroupName(name())
                      .withDefault(0)
                      .withID(id(27))
                      .withUnorderedMapFormatting({{0, "On Gated"}, {1, "On Key press"}})),
              portaTime(floatMd()
                            .withRange(-8, 2)
                            .withName(name() + " Portamento Time")
                            .withGroupName(name())
                            .withDefault(-8)
                            .withID(id(28))
                            .withDecimalPlaces(4)
                            .withLog2SecondsFormatting()
                            .withMilisecondsBelowOneSecond()
                            .withCustomMinDisplay("Off")),
              portaContinuation(intMd()
                                    .withID(id(43))
                                    .withName(name() + " Porta Continuation")
                                    .withRange(0, 1)
                                    .withDefault(0)
                                    .withGroupName(name())
                                    .withUnorderedMapFormatting(
                                        {{0, "Reset On Voice"}, {1, "Restart From Last"}})),

              pianoModeActive(md_t()
                                  .asBool()
                                  .withName(name() + " Piano Mode Active")
                                  .withGroupName(name())
                                  .withDefault(true)
                                  .withID(id(29))),
              unisonCount(intMd()
                              .withName(name() + " Unison Voice Count")
                              .withGroupName(name())
                              .withRange(1, 5)
                              .withDefault(1)
                              .withID(id(30))),
              unisonSpread(floatMd()
                               .withRange(0, 2.0 / 12.0)
                               .withName(name() + " Unison Ratio Shift")
                               .withGroupName(name())
                               .withDefault(.1 / 12.0)
                               .withDecimalPlaces(4)
                               .withATwoToTheBFormatting(1, 1, "x")
                               .withID(id(31))),
              uniPhaseRand(boolMd()
                               .withName(name() + " Unison Phase Randomized")
                               .withGroupName(name())
                               .withDefault(true)
                               .withID(id(32))),
              mpeActive(boolMd()
                            .withName(name() + " MPE Active")
                            .withGroupName(name())
                            .withDefault(false)
                            .withID(id(33))),
              mpeBendRange(intMd()
                               .withName(name() + " MPE Bend Range")
                               .withGroupName(name())
                               .withRange(1, 96)
                               .withDefault(24)
                               .withID(id(34))),
              octTranspose(intMd()
                               .withName(name() + " Octave Transpose")
                               .withGroupName(name())
                               .withRange(-3, 3)
                               .withDefault(0)
                               .withID(id(35))),
              fineTune(floatMd()
                           .withName(name() + " Fine Tuning")
                           .withGroupName(name())
                           .withRange(-200, 200)
                           .withDefault(0)
                           .withLinearScaleFormatting("cents")
                           .withID(id(36))),

              pan(floatMd()
                      .withName(name() + " Pan")
                      .withGroupName(name())
                      .asPercentBipolar()
                      .withDefault(0)
                      .withID(id(37))),
              attackFloorOnRetrig(boolMd()
                                      .withName(name() + " Attack is Floored on Retrigger")
                                      .withGroupName(name())
                                      .withDefault(1)
                                      .withID(id(38))),

              rephaseOnRetrigger(boolMd()
                                     .withName(name() + " Soft Reset Phase on Retrigger")
                                     .withGroupName(name())
                                     .withDefault(false)
                                     .withID(id(39))),

              sampleRateStrategy(
                  intMd()
                      .withName(name() + " Sample Rate Strategy")
                      .withGroupName(name())
                      .withDefault(SampleRateStrategy::SR_110120)
                      .withID(id(40))
                      .withRange(SampleRateStrategy::SR_110120, SampleRateStrategy::SR_220240)
                      .withUnorderedMapFormatting({
                          {SampleRateStrategy::SR_110120, "110.25/120 kHz"},
                          {SampleRateStrategy::SR_132144, "136.3/144 kHz"},
                          {SampleRateStrategy::SR_176192, "176.4/192 kHz"},
                          {SampleRateStrategy::SR_220240, "220.5/240 kHz"},
                      })),
              resampleEngine(intMd()
                                 .withName(name() + " Resampler Engine")
                                 .withGroupName(name())
                                 .withDefault(ResamplerEngine::SRC_FAST)
                                 .withRange(ResamplerEngine::SRC_FAST, ResamplerEngine::ZOH)
                                 .withID(id(41))
                                 .withUnorderedMapFormatting({
                                     {ResamplerEngine::SRC_FAST, "SRC Fast (rec)"},
                                     {ResamplerEngine::SRC_MEDIUM, "SRC Medium"},
                                     {ResamplerEngine::SRC_BEST, "SRC Expensive"},
                                     {ResamplerEngine::LANCZOS, "Lanczos A=4"},
                                     {ResamplerEngine::LINTERP, "Linear Interp"},
                                     {ResamplerEngine::ZOH, "ZOH"},
                                 })),
              unisonPan(floatMd()
                            .withName(name() + " Unison Stereo Field")
                            .asPercent()
                            .withDefault(0.8)
                            .withGroupName(name())
                            .withID(id(42))),
              ModulationMixin(name(), id(120)),
              modtarget(scpu::make_array_lambda<Param, numModsPer>(
                  [this](int i)
                  {
                      return md_t()
                          .withName(name() + " Mod Target " + std::to_string(i))
                          .withGroupName(name())
                          .withRange(0, 2000)
                          .withDefault(TargetID::NONE)
                          .withID(id(150 + i));
                  })),
              LFOMixin(name(), id(200)), lfoDepth(floatMd()
                                                      .asPercentBipolar()
                                                      .withName(name() + " LFO Depth")
                                                      .withGroupName(name())
                                                      .withDefault(0)
                                                      .withID(id(220)))
        {
            defaultTrigger.adhocFeatures = Param::AdHocFeatureValues::TRIGGERMODE;
            appendLFOTargetName(targetList);
            appendDAHDSRTargetName(targetList);
        }

        std::string name() const { return "Main"; }
        uint32_t id(int f) const { return idBase + f; }

        Param level, velSensitivity, playMode;
        Param bendUp, bendDown, polyLimit, defaultTrigger, portaTime, portaContinuation,
            pianoModeActive;
        Param unisonCount, unisonSpread, uniPhaseRand, unisonPan;
        Param mpeActive, mpeBendRange;
        Param octTranspose, fineTune, pan, lfoDepth;
        Param attackFloorOnRetrig, rephaseOnRetrigger;
        Param sampleRateStrategy, resampleEngine;

        std::array<Param, numModsPer> modtarget;

        std::vector<Param *> params()
        {
            std::vector<Param *> res{&level,
                                     &velSensitivity,
                                     &playMode,
                                     &bendUp,
                                     &bendDown,
                                     &polyLimit,
                                     &defaultTrigger,
                                     &portaTime,
                                     &portaContinuation,
                                     &pianoModeActive,
                                     &unisonCount,
                                     &unisonSpread,
                                     &unisonPan,
                                     &uniPhaseRand,
                                     &mpeActive,
                                     &mpeBendRange,
                                     &octTranspose,
                                     &pan,
                                     &fineTune,
                                     &lfoDepth,
                                     &attackFloorOnRetrig,
                                     &rephaseOnRetrigger,
                                     &sampleRateStrategy,
                                     &resampleEngine};
            appendDAHDSRParams(res);

            for (int i = 0; i < numModsPer; ++i)
                res.push_back(&modtarget[i]);
            appendModulationParams(res);
            appendLFOParams(res);

            return res;
        }
    };

    std::array<SourceNode, numOps> sourceNodes;
    std::array<SelfNode, numOps> selfNodes;
    std::array<MatrixNode, matrixSize> matrixNodes;
    std::array<MixerNode, numOps> mixerNodes;
    std::array<MacroNode, numMacros> macroNodes;
    OutputNode output;

    FineTuneNode fineTuneMod;
    MainPanNode mainPanMod;

    char name[256]{"Init"};

    float migrateParamValueFromVersion(Param *p, float value, uint32_t version);
    void migratePatchFromVersion(uint32_t version);
};
} // namespace baconpaul::six_sines
#endif // PATCH_H
