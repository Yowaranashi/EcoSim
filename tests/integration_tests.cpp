#include "core/app.h"
#include "core/event_bus.h"
#include "core/logger.h"
#include "modules/simulation_world.h"

#include <functional>
#include <iostream>
#include <sstream>

namespace {
struct TestResult {
    std::string name;
    bool passed = false;
    std::string message;
};

TestResult runTest(const std::string &name, const std::function<bool(std::string &)> &test) {
    std::string message;
    bool passed = false;
    try {
        passed = test(message);
    } catch (const std::exception &ex) {
        message = ex.what();
        passed = false;
    }
    return {name, passed, message};
}
}

std::string sourcePath(const std::string &relative) {
#ifdef ECOSIM_SOURCE_DIR
    return std::string(ECOSIM_SOURCE_DIR) + "/" + relative;
#else
    return relative;
#endif
}

int main() {
    std::vector<TestResult> results;

    results.push_back(runTest("T1", [](std::string &message) {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        if (!app.initialize(sourcePath("tests/data/app_base.toml"))) {
            message = "Failed to initialize app";
            return false;
        }
        if (app.registry().manifests().size() < 3) {
            message = "Expected manifests to load";
            return false;
        }
        return true;
    }));

    results.push_back(runTest("T2", [](std::string &message) {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        if (app.initialize(sourcePath("tests/data/app_missing_important.toml"))) {
            message = "Expected initialize to fail on missing Important factory";
            return false;
        }

        ecosim::Application app_order(logger);
        if (!app_order.initialize(sourcePath("tests/data/app_base.toml"))) {
            message = "Failed to init base app";
            return false;
        }
        if (!app_order.startModules()) {
            message = "Failed to start modules";
            return false;
        }
        auto order = app_order.moduleManager().startOrder();
        if (order.empty() || order.front() != "simulation_world") {
            message = "simulation_world should start first";
            return false;
        }
        return true;
    }));

    results.push_back(runTest("T3", [](std::string &message) {
        ecosim::EventBus bus;
        bool delivered = false;
        bus.subscribe("event.test", [&delivered](const ecosim::SimulationEvent &) { delivered = true; });
        ecosim::SimulationEvent event{ "event.test", 1, {} };
        bus.emit(event);
        if (delivered) {
            message = "Event delivered before buffer flush";
            return false;
        }
        if (bus.bufferedCount() != 1) {
            message = "Event buffer should contain one event";
            return false;
        }
        bus.deliverBuffered();
        if (!delivered) {
            message = "Event not delivered after buffer flush";
            return false;
        }
        return true;
    }));

    results.push_back(runTest("T4", [](std::string &message) {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        if (!app.initialize(sourcePath("tests/data/app_base.toml"))) {
            message = "Failed to init app";
            return false;
        }
        if (!app.startModules()) {
            message = "Failed to start modules";
            return false;
        }

        auto world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        auto recorder = app.moduleManager().findModule("recorder", "csv");
        if (!world || !recorder) {
            message = "Required modules not found";
            return false;
        }

        int delivered = 0;
        app.eventBus().subscribe("world.tick", [&delivered](const ecosim::SimulationEvent &) { delivered++; });
        app.runHeadless();

        if (delivered != world->readModel().tick) {
            message = "Tick events count does not match ticks";
            return false;
        }
        return true;
    }));

    results.push_back(runTest("T5", [](std::string &message) {
        auto run_once = [](std::string &checksum) {
            std::ostringstream output;
            ecosim::Logger logger(output);
            ecosim::Application app(logger);
            if (!app.initialize(sourcePath("tests/data/app_base.toml"))) {
                return false;
            }
            if (!app.startModules()) {
                return false;
            }
            app.runHeadless();
            auto world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
            if (!world) {
                return false;
            }
            checksum = world->checksum();
            return true;
        };

        std::string checksum_a;
        std::string checksum_b;
        if (!run_once(checksum_a) || !run_once(checksum_b)) {
            message = "Failed to run scenario";
            return false;
        }
        if (checksum_a != checksum_b) {
            message = "Checksums differ across runs";
            return false;
        }
        return true;
    }));

    int failures = 0;
    for (const auto &result : results) {
        std::cout << result.name << ": " << (result.passed ? "PASS" : "FAIL") << '\n';
        if (!result.passed) {
            std::cout << "  " << result.message << '\n';
            failures++;
        }
    }
    return failures == 0 ? 0 : 1;
}
