#include "core/scenario.h"

#include <algorithm>
#include <utility>

namespace ecosim {

ScenarioTimeline::ScenarioTimeline(ScenarioConfig config) : config_(std::move(config)) {
    std::stable_sort(config_.schedule.begin(), config_.schedule.end(),
                     [](const auto &a, const auto &b) { return a.tick < b.tick; });
}

std::vector<ScenarioConfig::ScheduledAction> ScenarioTimeline::actionsForTick(int tick) const {
    auto first = std::lower_bound(config_.schedule.begin(), config_.schedule.end(), tick,
                                  [](const ScenarioConfig::ScheduledAction &action, int value) {
                                      return action.tick < value;
                                  });
    auto last = std::upper_bound(first, config_.schedule.end(), tick,
                                 [](int value, const ScenarioConfig::ScheduledAction &action) {
                                     return value < action.tick;
                                 });

    return {first, last};
}

} // namespace ecosim
