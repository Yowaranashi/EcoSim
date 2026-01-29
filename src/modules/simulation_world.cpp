#include "modules/simulation_world.h"

#include "core/logger.h"

namespace ecosim {

SimulationWorld::SimulationWorld(const ModuleInstanceConfig &instance, ModuleContext &context)
    : type_id_(instance.type_id), instance_id_(instance.instance_id), context_(context) {}

void SimulationWorld::onInit() {
    read_model_.tick = 0;
    read_model_.seed = 0;
    read_model_.population_by_species.clear();
    read_model_.energy_total = 0;
}

void SimulationWorld::enqueueCommand(const std::string &command, const std::map<std::string, std::string> &params) {
    pending_commands_.push_back({command, params});
}

void SimulationWorld::onPreTick() {
    for (const auto &entry : pending_commands_) {
        applyCommand(entry.first, entry.second);
    }
    pending_commands_.clear();
}

void SimulationWorld::onTick() {
    read_model_.tick += 1;
    for (const auto &species : species_order_) {
        auto &count = read_model_.population_by_species[species];
        count += 1;
    }
    read_model_.energy_total = 0;
    for (const auto &pair : read_model_.population_by_species) {
        read_model_.energy_total += pair.second * 2;
    }
    emitTickEvent();
}

void SimulationWorld::applyCommand(const std::string &command, const std::map<std::string, std::string> &params) {
    if (command == "world.reset") {
        auto seed_it = params.find("seed");
        if (seed_it != params.end()) {
            read_model_.seed = std::stoi(seed_it->second);
        }
        read_model_.tick = 0;
        read_model_.population_by_species.clear();
        species_order_.clear();
        context_.logger().log(LogChannel::System, "World reset with seed " + std::to_string(read_model_.seed));
    } else if (command == "spawn") {
        auto species_it = params.find("species");
        auto count_it = params.find("count");
        if (species_it != params.end() && count_it != params.end()) {
            auto &count = read_model_.population_by_species[species_it->second];
            if (count == 0) {
                species_order_.push_back(species_it->second);
            }
            count += std::stoi(count_it->second);
        }
    } else if (command == "set_param") {
        auto name_it = params.find("name");
        auto value_it = params.find("value");
        if (name_it != params.end() && value_it != params.end()) {
            params_[name_it->second] = std::stod(value_it->second);
        }
    } else if (command == "apply_shock") {
        auto strength_it = params.find("strength");
        if (strength_it != params.end()) {
            double strength = std::stod(strength_it->second);
            for (const auto &species : species_order_) {
                auto &count = read_model_.population_by_species[species];
                count = static_cast<int>(count * (1.0 - strength));
            }
        }
    } else if (command == "stop.at_tick") {
        auto tick_it = params.find("value");
        if (tick_it != params.end()) {
            stop_at_tick_ = std::stoi(tick_it->second);
        }
    }
}

void SimulationWorld::emitTickEvent() {
    SimulationEvent event;
    event.type = "world.tick";
    event.tick = read_model_.tick;
    event.payload["seed"] = std::to_string(read_model_.seed);
    event.payload["tick"] = std::to_string(read_model_.tick);
    event.payload["energy_total"] = std::to_string(read_model_.energy_total);
    for (const auto &pair : read_model_.population_by_species) {
        event.payload["population." + pair.first] = std::to_string(pair.second);
    }
    context_.eventBus().emit(event);
    context_.logger().log(LogChannel::Simulation,
                          "Tick " + std::to_string(read_model_.tick) + " population=" +
                              std::to_string(read_model_.population_by_species.size()));
}

bool SimulationWorld::shouldStop() const {
    return stop_at_tick_ >= 0 && read_model_.tick >= stop_at_tick_;
}

std::string SimulationWorld::checksum() const {
    long long total = 0;
    for (const auto &pair : read_model_.population_by_species) {
        total = total * 31 + pair.second;
    }
    total = total * 31 + read_model_.energy_total;
    total = total * 31 + read_model_.seed;
    return std::to_string(total);
}

} // namespace ecosim
