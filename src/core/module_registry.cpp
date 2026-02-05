#include "core/module_registry.h"

#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace ecosim {

namespace {

using RegisterModuleFn = void (*)(ModuleRegistry &);

} // namespace

ModuleRegistry::~ModuleRegistry() {
    for (auto &library : libraries_) {
        if (!library.handle) {
            continue;
        }
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(library.handle));
#else
        dlclose(library.handle);
#endif
    }
    libraries_.clear();
}

void ModuleRegistry::loadManifests(const std::filesystem::path &modules_dir) {
    manifests_.clear();
    for (auto &library : libraries_) {
        if (!library.handle) {
            continue;
        }
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(library.handle));
#else
        dlclose(library.handle);
#endif
    }
    libraries_.clear();
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
            if (!manifest.library_path.empty()) {
                std::filesystem::path library_path = manifest.library_path;
                if (library_path.is_relative()) {
                    library_path = entry.path() / library_path;
                }
                loadLibrary(library_path);
            }
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

void ModuleRegistry::loadLibrary(const std::filesystem::path &path) {
#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(path.string().c_str());
    if (!handle) {
        return;
    }
    auto register_fn = reinterpret_cast<RegisterModuleFn>(GetProcAddress(handle, "ecosimRegisterModule"));
#else
    void *handle = dlopen(path.string().c_str(), RTLD_NOW);
    if (!handle) {
        return;
    }
    auto register_fn = reinterpret_cast<RegisterModuleFn>(dlsym(handle, "ecosimRegisterModule"));
#endif
    if (!register_fn) {
#if defined(_WIN32)
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return;
    }
    register_fn(*this);
    libraries_.push_back(ModuleLibrary{handle, path.string()});
}

} // namespace ecosim
