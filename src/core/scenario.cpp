#include "core/scenario.h"

#include <algorithm>

namespace ecosim {

ScenarioTimeline::ScenarioTimeline(ScenarioConfig config) : config_(std::move(config)) {
    std::sort(config_.schedule.begin(), config_.schedule.end(),
              [](const auto &a, const auto &b) { return a.tick < b.tick; });
}

std::vector<ScenarioConfig::ScheduledAction> ScenarioTimeline::actionsForTick(int tick) const {
    std::vector<ScenarioConfig::ScheduledAction> actions;
    for (const auto &action : config_.schedule) {
        if (action.tick == tick) {
            actions.push_back(action);
        }
    }
    return actions;
}

} // namespace ecosim
