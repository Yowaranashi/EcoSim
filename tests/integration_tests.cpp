#include "core/app.h"
#include "core/event_bus.h"
#include "core/logger.h"
#include "modules/simulation_world.h"

#include <filesystem>
#include <fstream>
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
} // namespace

std::string normalizePath(const std::string &path) {
    return std::filesystem::path(path).generic_string();
}

std::filesystem::path locateRepoRoot() {
    std::filesystem::path current = std::filesystem::current_path();
    for (int depth = 0; depth < 6; ++depth) {
        auto modules_manifest = current / "modules" / "recorder" / "manifest.toml";
        auto scenario_path = current / "tests" / "data" / "scenario.toml";
        if (std::filesystem::exists(modules_manifest) && std::filesystem::exists(scenario_path)) {
            return current;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return std::filesystem::current_path();
}

std::string sourcePath(const std::string &relative) {
    auto root = locateRepoRoot();
    return normalizePath((root / relative).string());
}

namespace {
std::string writeTestConfig() {
    std::filesystem::path config_path = std::filesystem::current_path() / "app_test.toml";
    std::ofstream config_file(config_path);
    auto modules_dir = sourcePath("modules");
    auto scenario_path = sourcePath("tests/data/scenario.toml");
    auto output_dir = normalizePath((std::filesystem::current_path() / "test_output").string());
    config_file << "mode = \"headless\"\n";
    config_file << "error_policy = \"fail-fast\"\n";
    config_file << "modules_dir = \"" << modules_dir << "\"\n";
    config_file << "scenario_path = \"" << scenario_path << "\"\n";
    config_file << "output_dir = \"" << output_dir << "\"\n";
    config_file << "dt = 1.0\n";
    config_file << "max_ticks = 3\n";
    config_file << "instances = [\n";
    config_file << "  { type = \"simulation_world\", id = \"default\", enable = true },\n";
    config_file << "  { type = \"scenario\", id = \"default\", enable = true },\n";
    config_file << "  { type = \"recorder\", id = \"csv\", enable = true, params = { sink = \"memory\" } }\n";
    config_file << "]\n";
    return config_path.string();
}
} // namespace

int main() {
    std::vector<TestResult> results;

    results.push_back(runTest("T1", [](std::string &message) {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        auto config_path = writeTestConfig();
        if (!app.initialize(config_path)) {
            message = "Failed to initialize app";
            return false;
        }
        if (!app.startModules()) {
            message = "Failed to start modules";
            return false;
        }
        if (!app.moduleManager().findModule("simulation_world")) {
            message = "simulation_world module missing";
            return false;
        }
        if (!app.moduleManager().findModule("scenario")) {
            message = "scenario module missing";
            return false;
        }
        if (!app.moduleManager().findModule("recorder", "csv")) {
            message = "recorder module missing";
            return false;
        }
        return true;
    }));

    results.push_back(runTest("T2", [](std::string &message) {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        auto config_path = writeTestConfig();
        if (!app.initialize(config_path)) {
            message = "Failed to init app";
            return false;
        }
        if (!app.startModules()) {
            message = "Failed to start modules";
            return false;
        }
        auto order = app.moduleManager().startOrder();
        if (order.empty() || order.front() != "simulation_world") {
            message = "simulation_world should start before dependents";
            return false;
        }
        return true;
    }));

    results.push_back(runTest("T3", [](std::string &message) {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        auto config_path = writeTestConfig();
        if (!app.initialize(config_path)) {
            message = "Failed to init app";
            return false;
        }
        if (!app.startModules()) {
            message = "Failed to start modules";
            return false;
        }

        auto world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            message = "simulation_world module missing";
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

    results.push_back(runTest("T4", [](std::string &message) {
        std::ostringstream output;
        ecosim::Logger logger(output);
        ecosim::Application app(logger);
        auto config_path = writeTestConfig();
        if (!app.initialize(config_path)) {
            message = "Failed to init app";
            return false;
        }
        if (!app.startModules()) {
            message = "Failed to start modules";
            return false;
        }
        app.runHeadless();

        auto world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            message = "simulation_world module missing";
            return false;
        }
        auto boar_it = world->readModel().population_by_species.find("boar");
        auto deer_it = world->readModel().population_by_species.find("deer");
        if (boar_it == world->readModel().population_by_species.end() ||
            deer_it == world->readModel().population_by_species.end()) {
            message = "Scenario commands did not create expected species";
            return false;
        }
        if (boar_it->second != 4 || deer_it->second != 2) {
            message = "Scenario command results did not match expected populations";
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
