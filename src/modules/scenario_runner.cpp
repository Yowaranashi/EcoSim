#include "modules/scenario_runner.h"

#include "core/config.h"
#include "core/logger.h"

namespace ecosim {

ScenarioRunner::ScenarioRunner(const ModuleInstanceConfig &instance, ModuleContext &context)
    : type_id_(instance.type_id), instance_id_(instance.instance_id), context_(context),
      timeline_(ScenarioConfig{}) {}

void ScenarioRunner::setAvailableModules(const std::vector<std::string> &modules) {
    available_modules_.clear();
    for (const auto &module : modules) {
        available_modules_.insert(module);
    }
}

void ScenarioRunner::onStart() {
    auto scenario_path = context_.config().scenario_path;
    if (scenario_path.empty()) {
        context_.logger().log(LogChannel::System, "Scenario path not provided; skipping scenario runner");
        return;
    }

    ScenarioConfig config = ConfigLoader::loadScenario(scenario_path);
    for (const auto &required : config.requires) {
        if (available_modules_.find(required) == available_modules_.end()) {
            context_.logger().log(LogChannel::System, "Missing required module for scenario: " + required);
            initialized_ = false;
            return;
        }
    }

    timeline_ = ScenarioTimeline(config);
    initialized_ = true;

    if (world_) {
        world_->enqueueCommand("world.reset", {{"seed", std::to_string(config.seed)}});
        world_->enqueueCommand("stop.at_tick", {{"value", std::to_string(config.stop_at_tick)}});
    }
}

void ScenarioRunner::onPreTick() {
    if (!initialized_ || !world_) {
        return;
    }

    int next_tick = world_->readModel().tick + 1;
    for (const auto &action : timeline_.actionsForTick(next_tick)) {
        dispatchAction(action);
    }
}

void ScenarioRunner::dispatchAction(const ScenarioConfig::ScheduledAction &action) {
    if (action.command == "spawn") {
        world_->enqueueCommand("spawn", action.params);
    } else if (action.command == "set_param") {
        world_->enqueueCommand("set_param", action.params);
    } else if (action.command == "apply_shock") {
        world_->enqueueCommand("apply_shock", action.params);
    }
}

} // namespace ecosim
