#include "core/app.h"
#include "core/event_bus.h"
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
            return ecosim_tests::finishTest(false, "event_delivery", "Failed to initialize app");
        }
        if (!app.startModules()) {
            return ecosim_tests::finishTest(false, "event_delivery", "Failed to start modules");
        }
        auto world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            return ecosim_tests::finishTest(false, "event_delivery", "simulation_world module missing");
        }
        int delivered = 0;
        app.eventBus().subscribe("world.tick", [&delivered](const ecosim::SimulationEvent &) { delivered++; });
        app.runHeadless();
        if (delivered != world->readModel().tick) {
            return ecosim_tests::finishTest(false, "event_delivery", "Tick events count does not match ticks");
        }
        return ecosim_tests::finishTest(true, "event_delivery", "Event delivery matched tick count");
    } catch (const std::filesystem::filesystem_error &error) {
        return ecosim_tests::finishTest(false, "event_delivery", ecosim_tests::formatFilesystemError(error));
    }
}
