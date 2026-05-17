#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace ecosim_integration {

namespace {
std::filesystem::path writeConsoleAppConfig(const std::string &file_name,
                                            const std::string &mode,
                                            const std::string &scenario_path,
                                            int max_ticks) {
    auto path = generatedDataDir() / file_name;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    file << "mode = \"" << mode << "\"\n";
    file << "error_policy = \"fail-fast\"\n";
    file << "modules_dir = \"modules\"\n";
    file << "scenario_path = \"" << scenario_path << "\"\n";
    file << "output_dir = \"output\"\n";
    file << "dt = 1.0\n";
    file << "max_ticks = " << max_ticks << "\n";
    file << "instances = [\n";
    file << "  { type = \"simulation_world\", id = \"default\", enable = true },\n";
    file << "  { type = \"recorder\", id = \"csv\", enable = true, params = { sink = \"memory\" } },\n";
    file << "  { type = \"scenario\", id = \"default\", enable = true }\n";
    file << "]\n";
    return path;
}
} // namespace

class ConsoleModeDoesNotAutostartHeadlessTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.45 console mode does not autostart headless";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);

        auto config = writeConsoleAppConfig("app_test_45_console.toml", "console", "scenario-gfl.toml", 20);
        if (!app.initialize(config.string()) || !app.startModules()) {
            return {name, false, "failed to initialize console app"};
        }
        auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            return {name, false, "simulation_world not found"};
        }
        if (app.config().mode != "console" || world->readModel().tick != 0) {
            return {name, false, "console mode should start modules without running ticks"};
        }
        if (!app.console().execute("help") || app.console().execute("sim.start")) {
            return {name, false, "help should be registered and sim.start should be removed"};
        }
        return {name, true, "console branch is selectable without headless autostart"};
    }
};

class ScenarioListAndSimRunSelectionTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.46 scenario.list and sim.run selection";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);

        auto config = writeConsoleAppConfig("app_test_46_console_run.toml", "console", "scenario-gfl.toml", 250);
        if (!app.initialize(config.string()) || !app.startModules()) {
            return {name, false, "failed to initialize console app"};
        }
        auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            return {name, false, "simulation_world not found"};
        }

        if (!app.console().execute("scenario.list")) {
            return {name, false, "scenario.list was not registered"};
        }
        if (!containsText(log_stream.str(), "scenario-gfl.toml") ||
            !containsText(log_stream.str(), "scenario-rm.toml")) {
            return {name, false, "scenario.list did not report scenarios/ TOML files"};
        }

        if (!app.console().execute("sim.run")) {
            return {name, false, "sim.run was not registered"};
        }
        if (world->readModel().scenario_id != "scenario_gfl" || world->readModel().model_id != "glv") {
            return {name, false, "sim.run did not use app.toml scenario_path from scenarios/"};
        }

        if (!app.console().execute("sim.run -s scenario-rm.toml")) {
            return {name, false, "sim.run -s was not accepted"};
        }
        if (world->readModel().scenario_id != "scenario_rm" ||
            world->readModel().model_id != "rosenzweig_macarthur") {
            return {name, false, "sim.run -s did not select the requested scenarios/ file"};
        }

        app.console().execute("sim.run -s nonexistent.toml");
        if (!containsText(log_stream.str(), "Scenario file does not exist")) {
            return {name, false, "missing scenario did not produce a readable error"};
        }
        return {name, true, "scenario listing and sim.run selection work"};
    }
};

class InvalidScenarioAndOgreCommandsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.47 invalid scenario and OGRE commands";
        auto invalid_path = repoRoot() / "scenarios" / "_invalid_console_test.toml";
        {
            std::ofstream file(invalid_path, std::ios::out | std::ios::trunc);
            file << "scenario_id = \"invalid\"\n";
            file << "model = [this is not valid TOML\n";
        }

        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);
        auto config = writeConsoleAppConfig("app_test_47_console_invalid.toml", "console", "scenario-gfl.toml", 20);
        if (!app.initialize(config.string()) || !app.startModules()) {
            std::filesystem::remove(invalid_path);
            return {name, false, "failed to initialize console app"};
        }

        app.console().execute("sim.run -s _invalid_console_test.toml");
        app.console().execute("ogre.status");
        app.console().execute("ogre.enable");
        app.console().execute("ogre.disable");
        app.console().execute("sys.quit");
        std::filesystem::remove(invalid_path);

        auto logs = log_stream.str();
        if (!containsText(logs, "Failed to load scenario")) {
            return {name, false, "invalid TOML did not produce a logged scenario error"};
        }
        if (!containsText(logs, "OGRE viewer support") || !containsText(logs, "Expected recorder CSV") ||
            !containsText(logs, "sys.quit received")) {
            return {name, false, "OGRE commands or sys.quit did not remain usable after scenario error"};
        }
        return {name, true, "console survives invalid scenario and OGRE commands are registered"};
    }
};

std::unique_ptr<IIntegrationTest> makeConsoleModeDoesNotAutostartHeadlessTest() {
    return std::make_unique<ConsoleModeDoesNotAutostartHeadlessTest>();
}

std::unique_ptr<IIntegrationTest> makeScenarioListAndSimRunSelectionTest() {
    return std::make_unique<ScenarioListAndSimRunSelectionTest>();
}

std::unique_ptr<IIntegrationTest> makeInvalidScenarioAndOgreCommandsTest() {
    return std::make_unique<InvalidScenarioAndOgreCommandsTest>();
}

} // namespace ecosim_integration
