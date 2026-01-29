#include "core/app.h"

#include "modules/recorder_csv.h"
#include "modules/scenario_runner.h"
#include "modules/simulation_world.h"

#include <algorithm>
#include <filesystem>
#include <memory>

namespace ecosim {

Application::Application(Logger &logger)
    : logger_(logger), context_(logger_, event_bus_, app_config_), module_manager_(registry_, context_) {}

bool Application::initialize(const std::string &config_path) {
    logger_.log(LogChannel::System, "Loading app config: " + config_path);
    app_config_ = ConfigLoader::loadAppConfig(config_path);

    std::filesystem::path config_dir = std::filesystem::path(config_path).parent_path();
    if (!config_dir.empty()) {
        if (!app_config_.modules_dir.empty() && std::filesystem::path(app_config_.modules_dir).is_relative()) {
            app_config_.modules_dir = (config_dir / app_config_.modules_dir).string();
        }
        if (!app_config_.scenario_path.empty() && std::filesystem::path(app_config_.scenario_path).is_relative()) {
            app_config_.scenario_path = (config_dir / app_config_.scenario_path).string();
        }
        if (!app_config_.output_dir.empty() && std::filesystem::path(app_config_.output_dir).is_relative()) {
            app_config_.output_dir = (config_dir / app_config_.output_dir).string();
        }
    }

    logger_.log(LogChannel::System, "Loading manifests from: " + app_config_.modules_dir);
    registry_.loadManifests(app_config_.modules_dir);

    registry_.registerFactory("simulation_world", [](const ModuleInstanceConfig &instance, ModuleContext &context) {
        return std::make_unique<SimulationWorld>(instance, context);
    });
    registry_.registerFactory("scenario", [](const ModuleInstanceConfig &instance, ModuleContext &context) {
        return std::make_unique<ScenarioRunner>(instance, context);
    });
    registry_.registerFactory("recorder", [](const ModuleInstanceConfig &instance, ModuleContext &context) {
        return std::make_unique<RecorderCsv>(instance, context);
    });

    if (!module_manager_.buildModules(app_config_.instances, app_config_.error_policy, logger_)) {
        return false;
    }

    auto world = dynamic_cast<SimulationWorld *>(module_manager_.findModule("simulation_world"));
    auto scenario = dynamic_cast<ScenarioRunner *>(module_manager_.findModule("scenario"));
    if (scenario) {
        std::vector<std::string> types;
        for (auto module : module_manager_.modules()) {
            types.push_back(module->typeId());
        }
        scenario->setAvailableModules(types);
        scenario->setWorld(world);
    }

    registerCoreCommands();
    return true;
}

bool Application::startModules() {
    if (!module_manager_.startModules(app_config_.error_policy, logger_)) {
        return false;
    }

    return true;
}

void Application::runHeadless() {
    running_ = true;

    auto world = dynamic_cast<SimulationWorld *>(module_manager_.findModule("simulation_world"));
    if (!world) {
        logger_.log(LogChannel::System, "simulation_world module is required for headless run");
        return;
    }

    int max_ticks = app_config_.max_ticks.value_or(1000);
    for (int tick = 0; running_; ++tick) {
        for (auto module : module_manager_.modules()) {
            module->onPreTick();
        }
        for (auto module : module_manager_.modules()) {
            module->onTick();
        }
        for (auto module : module_manager_.modules()) {
            module->onPostTick();
        }
        event_bus_.deliverBuffered();
        for (auto module : module_manager_.modules()) {
            module->onDeliverBufferedEvents();
        }

        if (world->shouldStop()) {
            logger_.log(LogChannel::System, "Stop condition reached at tick " + std::to_string(world->readModel().tick));
            running_ = false;
        }
        if (tick + 1 >= max_ticks) {
            logger_.log(LogChannel::System, "Reached max ticks");
            running_ = false;
        }
    }
}

void Application::shutdown() {
    module_manager_.stopModules();
}

void Application::registerCoreCommands() {
    console_.registerCommand("module.list", [this](const std::vector<std::string> &) {
        for (auto module : module_manager_.modules()) {
            logger_.log(LogChannel::System, "module " + module->typeId() + ":" + module->instanceId());
        }
    });
    console_.registerCommand("module.start", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "module.start is not supported in MVP (static modules)");
    });
    console_.registerCommand("module.stop", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "module.stop is not supported in MVP (static modules)");
    });
    console_.registerCommand("sim.run", [this](const std::vector<std::string> &) {
        runHeadless();
    });
    console_.registerCommand("sim.start", [this](const std::vector<std::string> &) {
        runHeadless();
    });
    console_.registerCommand("sim.pause", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "sim.pause is a no-op in headless MVP");
    });
    console_.registerCommand("sim.resume", [this](const std::vector<std::string> &) {
        logger_.log(LogChannel::System, "sim.resume is a no-op in headless MVP");
    });
    console_.registerCommand("sys.quit", [this](const std::vector<std::string> &) {
        running_ = false;
        logger_.log(LogChannel::System, "sys.quit received");
    });
}

} // namespace ecosim
