#pragma once

#include <map>
#include <string>
#include <vector>

namespace ecosim {

struct IntegratorConfig {
    std::string type = "euler";
    double dt = 1.0;
};

struct SpeciesConfig {
    std::string id;
    double initial_state = 0.0;
    std::map<std::string, double> parameters;
};

struct ModelConfig {
    std::string model_id;
    std::vector<SpeciesConfig> species;
    std::map<std::string, double> initial_state;
    std::map<std::string, double> parameters;
    std::vector<std::vector<double>> interaction_matrix;
};

struct ScheduledAction {
    int tick = 0;
    std::string command;
    std::map<std::string, std::string> params;
};

struct ScenarioDefinition {
    std::string scenario_id;
    ModelConfig model;
    int seed = 0;
    int stop_at_tick = 0;
    IntegratorConfig integrator;
    std::vector<std::string> requires;
    std::vector<ScheduledAction> schedule;
};

struct ScenarioConfig : ScenarioDefinition {
    using ScheduledAction = ecosim::ScheduledAction;
};

class ScenarioTimeline {
public:
    explicit ScenarioTimeline(ScenarioConfig config);

    const ScenarioConfig &config() const { return config_; }
    std::vector<ScenarioConfig::ScheduledAction> actionsForTick(int tick) const;

private:
    ScenarioConfig config_;
};

} // namespace ecosim
