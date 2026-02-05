#include "core/app.h"
#include "core/logger.h"
#include "tests/test_support.h"

#include <iostream>
#include <sstream>

int main() {
    std::ostringstream output;
    ecosim::Logger logger(output);
    ecosim::Application app(logger);
    auto config_path = ecosim_tests::writeTestConfig();
    if (!app.initialize(config_path)) {
        return ecosim_tests::finishTest(false, "start_order", "Failed to initialize app");
    }
    if (!app.startModules()) {
        return ecosim_tests::finishTest(false, "start_order", "Failed to start modules");
    }
    auto order = app.moduleManager().startOrder();
    if (order.empty() || order.front() != "simulation_world") {
        return ecosim_tests::finishTest(false, "start_order", "simulation_world should start before dependents");
    }
    return ecosim_tests::finishTest(true, "start_order", "Start order validated");
}
