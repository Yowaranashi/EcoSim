#include "core/module_manager.h"

#include <algorithm>
#include <unordered_set>

namespace ecosim {

ModuleManager::ModuleManager(ModuleRegistry &registry, ModuleContext &context)
    : registry_(registry), context_(context) {}

bool ModuleManager::buildModules(const std::vector<ModuleInstanceConfig> &instances, ErrorPolicy policy, Logger &logger) {
    modules_.clear();
    start_order_.clear();

    for (const auto &instance : instances) {
        if (!instance.enabled) {
            continue;
        }
        auto manifest = registry_.findManifest(instance.type_id);
        if (!manifest) {
            logger.log(LogChannel::System, "Missing manifest for module type: " + instance.type_id);
            if (policy == ErrorPolicy::FailFast) {
                return false;
            }
            continue;
        }
        auto module = registry_.create(instance, context_);
        if (!module) {
            logger.log(LogChannel::System, "Missing factory for module type: " + instance.type_id);
            if (manifest->criticality == Criticality::Critical) {
                return false;
            }
            if (manifest->criticality == Criticality::Important && policy == ErrorPolicy::FailFast) {
                return false;
            }
            continue;
        }
        modules_.push_back(std::move(module));
    }
    return true;
}

std::vector<std::string> ModuleManager::dependencyOrder(
    const std::map<std::string, std::vector<std::string>> &deps) const {
    std::map<std::string, int> indegree;
    for (const auto &pair : deps) {
        indegree[pair.first] = 0;
    }
    for (const auto &pair : deps) {
        for (const auto &dep : pair.second) {
            if (indegree.find(dep) == indegree.end()) {
                continue;
            }
            indegree[pair.first]++;
        }
    }

    std::vector<std::string> order;
    std::vector<std::string> ready;
    for (const auto &pair : indegree) {
        if (pair.second == 0) {
            ready.push_back(pair.first);
        }
    }
    std::sort(ready.begin(), ready.end());

    while (!ready.empty()) {
        auto node = ready.front();
        ready.erase(ready.begin());
        order.push_back(node);
        for (const auto &pair : deps) {
            if (std::find(pair.second.begin(), pair.second.end(), node) != pair.second.end()) {
                indegree[pair.first]--;
                if (indegree[pair.first] == 0) {
                    ready.push_back(pair.first);
                    std::sort(ready.begin(), ready.end());
                }
            }
        }
    }
    return order;
}

bool ModuleManager::startModules(ErrorPolicy policy, Logger &logger) {
    std::map<std::string, std::vector<std::string>> deps_by_type;
    for (const auto &module : modules_) {
        auto manifest = registry_.findManifest(module->typeId());
        if (!manifest) {
            continue;
        }
        deps_by_type[module->typeId()] = manifest->dependencies;
    }

    for (const auto &module : modules_) {
        auto manifest = registry_.findManifest(module->typeId());
        if (!manifest) {
            continue;
        }
        for (const auto &dep : manifest->dependencies) {
            if (deps_by_type.find(dep) == deps_by_type.end()) {
                logger.log(LogChannel::System,
                           "Missing dependency " + dep + " for module type " + module->typeId());
                if (manifest->criticality == Criticality::Critical) {
                    return false;
                }
                if (manifest->criticality == Criticality::Important && policy == ErrorPolicy::FailFast) {
                    return false;
                }
            }
        }
    }

    auto order = dependencyOrder(deps_by_type);
    for (const auto &type_id : order) {
        for (const auto &module : modules_) {
            if (module->typeId() != type_id) {
                continue;
            }
            logger.log(LogChannel::System, "Starting module " + module->typeId() + ":" + module->instanceId());
            module->onInit();
            module->onStart();
            start_order_.push_back(module->typeId());
        }
    }

    for (const auto &module : modules_) {
        if (std::find(order.begin(), order.end(), module->typeId()) == order.end()) {
            logger.log(LogChannel::System, "Unresolved dependencies for module type: " + module->typeId());
            if (policy == ErrorPolicy::FailFast) {
                return false;
            }
        }
    }

    return true;
}

void ModuleManager::stopModules() {
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        (*it)->onStop();
    }
}

std::vector<IModule *> ModuleManager::modules() const {
    std::vector<IModule *> result;
    for (const auto &module : modules_) {
        result.push_back(module.get());
    }
    return result;
}

IModule *ModuleManager::findModule(const std::string &type_id, const std::string &instance_id) const {
    for (const auto &module : modules_) {
        if (module->typeId() == type_id && module->instanceId() == instance_id) {
            return module.get();
        }
    }
    return nullptr;
}

} // namespace ecosim
