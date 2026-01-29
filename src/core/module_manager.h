#pragma once

#include "core/config.h"
#include "core/module.h"
#include "core/module_registry.h"

#include <map>
#include <string>
#include <vector>

namespace ecosim {

class ModuleManager {
public:
    ModuleManager(ModuleRegistry &registry, ModuleContext &context);

    bool buildModules(const std::vector<ModuleInstanceConfig> &instances, ErrorPolicy policy, Logger &logger);
    bool startModules(ErrorPolicy policy, Logger &logger);
    void stopModules();

    std::vector<IModule *> modules() const;
    const std::vector<std::string> &startOrder() const { return start_order_; }

    IModule *findModule(const std::string &type_id, const std::string &instance_id = "default") const;

private:
    std::vector<std::string> dependencyOrder(const std::map<std::string, std::vector<std::string>> &deps) const;

    ModuleRegistry &registry_;
    ModuleContext &context_;
    std::vector<ModulePtr> modules_;
    std::vector<std::string> start_order_;
};

} // namespace ecosim
