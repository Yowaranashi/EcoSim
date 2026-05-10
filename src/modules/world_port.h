#pragma once

#include "core/scenario.h"

#include <map>
#include <string>
#include <vector>

namespace ecosim {

struct ReadModel {
    int tick = 0;
    double time = 0.0;
    double dt = 1.0;
    int seed = 0;
    std::string scenario_id;
    std::string model_id;
    std::string integrator;
    std::vector<std::string> species_names;
    std::vector<double> state_vector;
    std::map<std::string, double> metrics;
    std::string checksum;
    std::vector<std::string> flags;

    std::map<std::string, int> population_by_species;
    int energy_total = 0;
};

struct WorldCommand {
    std::string command;
    std::map<std::string, std::string> params;
    std::map<std::string, double> numeric_params;
    ModelConfig model_config;
    bool has_model_config = false;
};

class IWorldPort {
public:
    virtual ~IWorldPort() = default;

    virtual void enqueueCommand(const std::string &command,
                                const std::map<std::string, std::string> &params) = 0;
    virtual void enqueueCommand(const WorldCommand &command) {
        enqueueCommand(command.command, command.params);
    }
    virtual const ReadModel &readModel() const = 0;
    virtual bool shouldStop() const = 0;
};

} // namespace ecosim
