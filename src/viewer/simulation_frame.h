#pragma once

#include <map>
#include <string>
#include <vector>

namespace ecosim::viewer {

struct ViewerFrame {
    int tick = 0;
    double time = 0.0;
    double dt = 0.0;

    std::string scenario_id;
    std::string model_id;
    std::string integrator;
    std::string checksum;

    std::vector<std::string> species_names;
    std::vector<double> state_values;
    std::map<std::string, double> state_by_species;

    std::map<std::string, double> metrics;
    std::vector<std::string> flags;
};

using SimulationFrame = ViewerFrame;

} // namespace ecosim::viewer
