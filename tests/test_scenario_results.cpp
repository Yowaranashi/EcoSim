#include "core/app.h"
#include "core/logger.h"
#include "modules/simulation_world.h"
#include "test_support.h"

#include <filesystem>
#include <iostream>
#include <sstream>

int main() {
    try {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        auto config_path = ecosim_tests::writeTestConfig();
        if (!app.initialize(config_path)) {
            return ecosim_tests::finishTest(false, "scenario_results", "Failed to initialize app");
        }
        if (!app.startModules()) {
            return ecosim_tests::finishTest(false, "scenario_results", "Failed to start modules");
        }
        app.runHeadless();

        auto world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            return ecosim_tests::finishTest(false, "scenario_results", "simulation_world module missing");
        }
        auto boar_it = world->readModel().population_by_species.find("boar");
        auto deer_it = world->readModel().population_by_species.find("deer");
        if (boar_it == world->readModel().population_by_species.end() ||
            deer_it == world->readModel().population_by_species.end()) {
            return ecosim_tests::finishTest(false, "scenario_results",
                                            "Scenario commands did not create expected species");
        }
        if (boar_it->second != 4 || deer_it->second != 2) {
            return ecosim_tests::finishTest(false, "scenario_results",
                                            "Scenario command results did not match expected populations");
        }
        return ecosim_tests::finishTest(true, "scenario_results", "Scenario results matched expectations");
    } catch (const std::filesystem::filesystem_error &error) {
        return ecosim_tests::finishTest(false, "scenario_results", ecosim_tests::formatFilesystemError(error));
    }
}
