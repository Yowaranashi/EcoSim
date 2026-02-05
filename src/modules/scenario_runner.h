#pragma once

#include "core/module.h"
#include "core/scenario.h"
#include "modules/world_port.h"

#include <set>

namespace ecosim {

class ScenarioRunner : public IModule {
public:
    ScenarioRunner(const ModuleInstanceConfig &instance, ModuleContext &context);

    const std::string &typeId() const override { return type_id_; }
    const std::string &instanceId() const override { return instance_id_; }

    void onStart() override;
    void onPreTick() override;

    void setWorld(IWorldPort *world) { world_ = world; }
    void setAvailableModules(const std::vector<std::string> &modules);

private:
    void dispatchAction(const ScenarioConfig::ScheduledAction &action);

    std::string type_id_;
    std::string instance_id_;
    ModuleContext &context_;
    ScenarioTimeline timeline_;
    std::set<std::string> available_modules_;
    IWorldPort *world_ = nullptr;
    bool initialized_ = false;
};

} // namespace ecosim
