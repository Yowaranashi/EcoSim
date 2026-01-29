#include "core/module_registry.h"

#include <filesystem>

namespace ecosim {

void ModuleRegistry::loadManifests(const std::filesystem::path &modules_dir) {
    manifests_.clear();
    if (!std::filesystem::exists(modules_dir)) {
        return;
    }
    for (const auto &entry : std::filesystem::directory_iterator(modules_dir)) {
        if (!entry.is_directory()) {
            continue;
        }
        auto manifest_path = entry.path() / "manifest.toml";
        if (!std::filesystem::exists(manifest_path)) {
            continue;
        }
        auto manifest = ConfigLoader::loadManifest(manifest_path.string());
        if (!manifest.type_id.empty()) {
            manifests_[manifest.type_id] = manifest;
        }
    }
}

void ModuleRegistry::registerFactory(const std::string &type_id, Factory factory) {
    factories_[type_id] = std::move(factory);
}

const ModuleManifest *ModuleRegistry::findManifest(const std::string &type_id) const {
    auto it = manifests_.find(type_id);
    if (it == manifests_.end()) {
        return nullptr;
    }
    return &it->second;
}

ModulePtr ModuleRegistry::create(const ModuleInstanceConfig &instance, ModuleContext &context) const {
    auto it = factories_.find(instance.type_id);
    if (it == factories_.end()) {
        return nullptr;
    }
    return it->second(instance, context);
}

} // namespace ecosim
