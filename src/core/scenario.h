#pragma once

#include "core/config.h"

#include <vector>

namespace ecosim {

class ScenarioTimeline {
public:
    explicit ScenarioTimeline(ScenarioConfig config);

    const ScenarioConfig &config() const { return config_; }
    std::vector<ScenarioConfig::ScheduledAction> actionsForTick(int tick) const;

private:
    ScenarioConfig config_;
};

} // namespace ecosim
