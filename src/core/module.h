#pragma once

#include "core/config.h"
#include "core/event_bus.h"
#include "core/logger.h"

#include <memory>
#include <string>

namespace ecosim {

class ModuleContext {
public:
    ModuleContext(Logger &logger, EventBus &event_bus, const AppConfig &config)
        : logger_(logger), event_bus_(event_bus), config_(config) {}

    Logger &logger() { return logger_; }
    EventBus &eventBus() { return event_bus_; }
    const AppConfig &config() const { return config_; }

private:
    Logger &logger_;
    EventBus &event_bus_;
    const AppConfig &config_;
};

class IModule {
public:
    virtual ~IModule() = default;

    virtual const std::string &typeId() const = 0;
    virtual const std::string &instanceId() const = 0;

    virtual void onInit() {}
    virtual void onStart() {}
    virtual void onStop() {}

    virtual void onPreTick() {}
    virtual void onTick() {}
    virtual void onPostTick() {}
    virtual void onDeliverBufferedEvents() {}
};

using ModulePtr = std::unique_ptr<IModule>;

} // namespace ecosim
