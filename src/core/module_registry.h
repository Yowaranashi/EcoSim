#pragma once

#include "core/config.h"
#include "core/module.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ecosim {

class ModuleRegistry {
public:
    using Factory = std::function<ModulePtr(const ModuleInstanceConfig &, ModuleContext &)>;

    ~ModuleRegistry();

    void loadManifests(const std::filesystem::path &modules_dir);
    void registerFactory(const std::string &type_id, Factory factory);

    const std::map<std::string, ModuleManifest> &manifests() const { return manifests_; }
    const ModuleManifest *findManifest(const std::string &type_id) const;

    ModulePtr create(const ModuleInstanceConfig &instance, ModuleContext &context) const;

private:
    struct ModuleLibrary {
        void *handle = nullptr;
        std::string path;
    };

    void loadLibrary(const std::filesystem::path &path);

    std::map<std::string, ModuleManifest> manifests_;
    std::map<std::string, Factory> factories_;
    std::vector<ModuleLibrary> libraries_;
};

} // namespace ecosim
