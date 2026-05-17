#pragma once

#include "core/config.h"
#include "core/console.h"
#include "core/event_bus.h"
#include "core/logger.h"
#include "core/module_manager.h"
#include "core/module_registry.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ecosim {

class Application {
public:
    explicit Application(Logger &logger);
    ~Application();

    bool initialize(const std::string &config_path);
    bool startModules();
    bool runHeadless();
    void runConsoleLoop();
    void shutdown();

    ModuleManager &moduleManager() { return module_manager_; }
    ModuleRegistry &registry() { return registry_; }
    EventBus &eventBus() { return event_bus_; }
    const AppConfig &config() const { return app_config_; }
    Console &console() { return console_; }
    const std::filesystem::path &scenariosDir() const { return scenarios_dir_; }
    std::filesystem::path expectedRecorderCsvPath() const;

private:
    struct ScenarioPathResult {
        bool ok = false;
        std::filesystem::path path;
        std::string error;
    };

    void registerCoreCommands();
    ScenarioPathResult resolveScenarioPath(const std::string &scenario_path,
                                           bool allow_existing_relative_fallback) const;
    bool prepareScenario(const std::string &scenario_path,
                         bool allow_existing_relative_fallback);
    bool runSimulationLoop();
    bool executeSimRun(const std::vector<std::string> &args);
    bool executeSensitivity(const std::vector<std::string> &args);
    std::filesystem::path resolveRunOutputPath(const std::string &output_arg) const;
    void configureRecorderOutput(const std::filesystem::path &path);
    void launchOgreViewer(const std::filesystem::path &csv_path) const;
    void listScenarios() const;
    void logOgreStatus() const;
    void setOgreRuntimeEnabled(bool enabled);

    Logger &logger_;
    EventBus event_bus_;
    AppConfig app_config_;
    ModuleRegistry registry_;
    ModuleContext context_;
    ModuleManager module_manager_;
    Console console_;
    bool running_ = false;
    bool console_running_ = false;
    bool ogre_runtime_enabled_ = false;
    std::filesystem::path config_path_;
    std::filesystem::path config_dir_;
    std::filesystem::path runtime_root_;
    std::filesystem::path scenarios_dir_;
    std::filesystem::path default_recorder_output_path_;
};

} // namespace ecosim
