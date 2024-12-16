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

#ifndef BACONPAUL_FMTHING_UI_PATCH_CONTINUOUS_H
#define BACONPAUL_FMTHING_UI_PATCH_CONTINUOUS_H

#include <cstdint>
#include "sst/jucegui/data/Continuous.h"
#include "sst/jucegui/components/Knob.h"

#include "ifm-editor.h"

namespace baconpaul::fm::ui
{
struct PatchContinuous : jdat::Continuous
{
    IFMEditor &editor;
    uint32_t pid;
    Param *p{nullptr};
    PatchContinuous(IFMEditor &e, uint32_t id) : editor(e), pid(id)
    {
        p = e.patchCopy.paramMap.at(id);
    }
    ~PatchContinuous() override = default;

    std::string getLabel() const override
    {
        auto g = p->meta.groupName;
        auto r = p->meta.name;
        auto gp = r.find(g);
        if (gp != std::string::npos)
        {
            r = r.substr(g.size() + 1); // trailing space
        }
        return r;
    }
    float getValue() const override { return p->value; }
    void setValueFromGUI(const float &f) override
    {
        p->value = f;
        editor.uiToAudio.push({Synth::UIToAudioMsg::Action::SET_PARAM, pid, f});
        editor.flushOperator();
    }
    void setValueFromModel(const float &f) override { p->value = f; }
    float getDefaultValue() const override { return p->meta.defaultVal; }
    bool isBipolar() const override { return p->meta.isBipolar(); }
    float getMin() const override { return p->meta.minVal; }
    float getMax() const override { return p->meta.maxVal; }
};

template <typename P, typename T = jcmp::Knob>
void createComponent(IFMEditor &e, P &panel, uint32_t id, std::unique_ptr<T> &cm,
                     std::unique_ptr<PatchContinuous> &pc)
{
    pc = std::make_unique<PatchContinuous>(e, id);
    cm = std::make_unique<T>();

    cm->onBeginEdit = [&e, id, &panel]()
    {
        e.uiToAudio.push({Synth::UIToAudioMsg::Action::BEGIN_EDIT, id});
        panel.beginEdit();
    };
    cm->onEndEdit = [&e, id, &panel]()
    {
        e.uiToAudio.push({Synth::UIToAudioMsg::Action::END_EDIT, id});
        panel.endEdit();
    };
    cm->setSource(pc.get());

    e.componentByID[id] = juce::Component::SafePointer<juce::Component>(cm.get());
}
} // namespace baconpaul::fm::ui
#endif // PATCH_CONTINUOUS_H
