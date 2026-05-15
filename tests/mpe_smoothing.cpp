/*
 * Per-voice smoothing of MPE / note-expression values.
 *
 * Voice::renderBlock advances five OnePoleLags at the top of the block: three
 * MPE (pressure / timbre / bend-in-semis) and two note-expression
 * (tuningInSemis / panBipolar). On the first block after attack() they snap;
 * thereafter they smooth at ~5 ms. Mod-matrix MPE sources are repointed at
 * the lag's .v in rebindPointer.
 */

#include "catch2/catch2.hpp"
#include "configuration.h"
#include "synth/patch.h"
#include "synth/synth.h"
#include "synth/voice.h"
#include "synth/mod_matrix.h"

#include <memory>

using namespace baconpaul::six_sines;

namespace
{
// Minimal patch config: one operator audible so a voice has somewhere to go,
// envelopes long enough that the voice doesn't auto-release during the test.
void configureMinimalPatch(Patch &patch)
{
    patch.output.level.value = 0.5f;
    patch.output.playMode.value = 0.f; // poly
    patch.output.polyLimit.value = (float)maxVoices;
    patch.output.unisonCount.value = 1.f;

    auto setSustainedEnv = [](Patch::DAHDSRMixin &e)
    {
        e.delay.value = 0.f;
        e.attack.value = 0.f;
        e.hold.value = 0.f;
        e.decay.value = 0.f;
        e.sustain.value = 1.f;
        e.release.value = 0.5f;
        e.envPower.value = 1.f;
        e.envIsMultiplcative.value = 1.f;
        e.envIsOneShot.value = 0.f;
    };
    setSustainedEnv(patch.output);

    for (int i = 0; i < (int)numOps; ++i)
    {
        patch.sourceNodes[i].active.value = (i == 0) ? 1.f : 0.f;
        patch.sourceNodes[i].ratio.value = 0.f;
        patch.sourceNodes[i].keyTrack.value = 1.f;
        setSustainedEnv(patch.sourceNodes[i]);

        patch.mixerNodes[i].active.value = (i == 0) ? 1.f : 0.f;
        patch.mixerNodes[i].level.value = 0.5f;
        setSustainedEnv(patch.mixerNodes[i]);
    }
}

// Bring up a Synth + minimal patch ready to receive noteOns. No noteOn fired.
std::unique_ptr<Synth> bringUpSynth(double hostSampleRate = 48000.0)
{
    auto s = std::make_unique<Synth>(false);
    s->setSampleRate(hostSampleRate);
    configureMinimalPatch(s->patch);
    s->reapplyControlSettings();
    return s;
}
} // namespace

// Test 1: on the first block after attack(), each lag must snap exactly to
// whatever value sits in the raw field at the moment renderBlock runs. This
// is the chirp regression — voicemanager dispatches the initial MPE setters
// AFTER attack() but BEFORE the first audio block, and a snap-in-attack
// design would have captured stale values.
TEST_CASE("mpe_lag attack snap", "[mpe_smoothing]")
{
    auto synth = bringUpSynth();
    synth->voiceManager->processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    auto *v = synth->head;
    REQUIRE(v != nullptr);

    // Simulate setVoiceMIDIMPE* / setNoteExpression dispatching after attack.
    v->voiceValues.mpePressure = 0.8f;
    v->voiceValues.mpeTimbre = 0.3f;
    v->voiceValues.mpeBendInSemis = 7.0f;
    v->voiceValues.noteExpressionTuningInSemis = -2.0f;
    v->voiceValues.noteExpressionPanBipolar = 0.5f;

    REQUIRE(v->voiceValues.firstBlockAfterAttack);
    synth->process(nullptr);
    REQUIRE_FALSE(v->voiceValues.firstBlockAfterAttack);

    REQUIRE(v->voiceValues.mpePressureLag.v == Approx(0.8f).margin(1e-7));
    REQUIRE(v->voiceValues.mpeTimbreLag.v == Approx(0.3f).margin(1e-7));
    REQUIRE(v->voiceValues.mpeBendInSemisLag.v == Approx(7.0f).margin(1e-6));
    REQUIRE(v->voiceValues.noteExpressionTuningInSemisLag.v == Approx(-2.0f).margin(1e-6));
    REQUIRE(v->voiceValues.noteExpressionPanBipolarLag.v == Approx(0.5f).margin(1e-7));
}

// Test 2: after the first block, target changes must smooth rather than snap.
// One block in mid-flight is strictly between old and new; ~50 blocks past
// the lag's time constant lands at the target within a tight margin.
TEST_CASE("mpe_lag mid-note smoothing", "[mpe_smoothing]")
{
    auto synth = bringUpSynth();
    synth->voiceManager->processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    auto *v = synth->head;
    REQUIRE(v != nullptr);

    // First block: snap to 0.
    v->voiceValues.mpePressure = 0.f;
    synth->process(nullptr);
    REQUIRE(v->voiceValues.mpePressureLag.v == 0.f);

    // Step to 1.0 and process one host block. Lag must be in-flight, not snapped.
    v->voiceValues.mpePressure = 1.f;
    synth->process(nullptr);
    REQUIRE(v->voiceValues.mpePressureLag.v > 0.f);
    REQUIRE(v->voiceValues.mpePressureLag.v < 1.f);

    // After many blocks the lag converges to the target.
    for (int i = 0; i < 200; ++i)
        synth->process(nullptr);
    REQUIRE(v->voiceValues.mpePressureLag.v == Approx(1.f).margin(1e-3));
}

// Test 3: every attack() resets firstBlockAfterAttack, so a fresh voice
// always snaps to its own initial MPE state — no leak from another voice
// already in flight on the same engine.
TEST_CASE("mpe_lag fresh attack always snaps", "[mpe_smoothing]")
{
    auto synth = bringUpSynth();

    // Voice A: trigger, set bend, run enough host blocks that A's lag is
    // safely past the snap (firstBlockAfterAttack cleared) and smoothing
    // toward a different target.
    synth->voiceManager->processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    auto *vA = synth->head;
    REQUIRE(vA != nullptr);
    vA->voiceValues.mpeBendInSemis = 12.0f;
    synth->process(nullptr);
    REQUIRE(vA->voiceValues.mpeBendInSemisLag.v == Approx(12.0f).margin(1e-6));
    vA->voiceValues.mpeBendInSemis = 0.0f;
    for (int i = 0; i < 5; ++i)
        synth->process(nullptr);
    REQUIRE_FALSE(vA->voiceValues.firstBlockAfterAttack);

    // Voice B: trigger on a different key. Its own firstBlockAfterAttack
    // must be true (independent of A's), and its first block must snap to
    // its own target, not chirp from 0.
    synth->voiceManager->processNoteOnEvent(0, 0, 62, -1, 0.8f, 0.f);
    Voice *vB = nullptr;
    for (auto *c = synth->head; c != nullptr; c = c->next)
        if (c != vA)
            vB = c;
    REQUIRE(vB != nullptr);
    REQUIRE(vB->voiceValues.firstBlockAfterAttack);
    vB->voiceValues.mpeBendInSemis = -7.0f;
    synth->process(nullptr);
    REQUIRE(vB->voiceValues.mpeBendInSemisLag.v == Approx(-7.0f).margin(1e-6));
}

// Test 4: rebindPointer must point MPE_* mod sources at the lag's .v, not at
// the raw field. Otherwise smoothing wouldn't reach the mod matrix.
TEST_CASE("mpe_lag mod source pointer rebind", "[mpe_smoothing]")
{
    auto synth = bringUpSynth();

    // Wire OutputNode's three mod slots to the three MPE sources before
    // triggering noteOn so attack()/bindModulation() pick them up.
    synth->patch.output.modsource[0].value = (float)ModMatrixConfig::Source::MPE_PRESSURE;
    synth->patch.output.modsource[1].value = (float)ModMatrixConfig::Source::MPE_TIMBRE;
    synth->patch.output.modsource[2].value = (float)ModMatrixConfig::Source::MPE_PITCHBEND;
    synth->reapplyControlSettings();

    synth->voiceManager->processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    auto *v = synth->head;
    REQUIRE(v != nullptr);

    // Points-at-lag .v.
    REQUIRE(v->out.sourcePointers[0] ==
            static_cast<const float *>(&v->voiceValues.mpePressureLag.v));
    REQUIRE(v->out.sourcePointers[1] == static_cast<const float *>(&v->voiceValues.mpeTimbreLag.v));
    REQUIRE(v->out.sourcePointers[2] ==
            static_cast<const float *>(&v->voiceValues.mpeBendInSemisLag.v));

    // Does NOT point at the raw field.
    REQUIRE(v->out.sourcePointers[0] != static_cast<const float *>(&v->voiceValues.mpePressure));
    REQUIRE(v->out.sourcePointers[1] != static_cast<const float *>(&v->voiceValues.mpeTimbre));
    REQUIRE(v->out.sourcePointers[2] != static_cast<const float *>(&v->voiceValues.mpeBendInSemis));
}
