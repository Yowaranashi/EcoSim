#pragma once

#include "core/config.h"
#include "core/console.h"
#include "core/event_bus.h"
#include "core/logger.h"
#include "core/module_manager.h"
#include "core/module_registry.h"

#include <string>

namespace ecosim {

class Application {
public:
    explicit Application(Logger &logger);

    bool initialize(const std::string &config_path);
    bool startModules();
    void runHeadless();
    void shutdown();

    ModuleManager &moduleManager() { return module_manager_; }
    ModuleRegistry &registry() { return registry_; }
    EventBus &eventBus() { return event_bus_; }
    const AppConfig &config() const { return app_config_; }
    Console &console() { return console_; }

private:
    void registerCoreCommands();

    Logger &logger_;
    EventBus event_bus_;
    AppConfig app_config_;
    ModuleRegistry registry_;
    ModuleContext context_;
    ModuleManager module_manager_;
    Console console_;
    bool running_ = false;
};

} // namespace ecosim
