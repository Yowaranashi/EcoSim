#include "core/app.h"
#include "core/logger.h"
#include "test_support.h"

#include <iostream>
#include <sstream>

int main() {
    std::ostringstream output;
    ecosim::Logger logger(output);
    ecosim::Application app(logger);
    auto config_path = ecosim_tests::writeTestConfig();
    if (!app.initialize(config_path)) {
        return ecosim_tests::finishTest(false, "modules_start", "Failed to initialize app");
    }
    if (!app.startModules()) {
        return ecosim_tests::finishTest(false, "modules_start", "Failed to start modules");
    }
    if (!app.moduleManager().findModule("simulation_world")) {
        return ecosim_tests::finishTest(false, "modules_start", "simulation_world module missing");
    }
    if (!app.moduleManager().findModule("scenario")) {
        return ecosim_tests::finishTest(false, "modules_start", "scenario module missing");
    }
    if (!app.moduleManager().findModule("recorder", "csv")) {
        return ecosim_tests::finishTest(false, "modules_start", "recorder module missing");
    }
    return ecosim_tests::finishTest(true, "modules_start", "All modules loaded and started");
}
