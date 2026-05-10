#pragma once

#include "core/scenario.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ecosim {

enum class Criticality { Critical, Important, Optional };

enum class ErrorPolicy { FailFast, AutoDisable };

struct ModuleManifest {
    std::string type_id;
    std::string version;
    std::vector<std::string> dependencies;
    Criticality criticality = Criticality::Optional;
    std::string library_path;
};

struct ModuleInstanceConfig {
    std::string type_id;
    std::string instance_id = "default";
    bool enabled = true;
    std::map<std::string, std::string> params;
};

struct AppConfig {
    std::string mode = "headless";
    ErrorPolicy error_policy = ErrorPolicy::FailFast;
    std::vector<ModuleInstanceConfig> instances;
    std::string modules_dir = "modules";
    std::string scenario_path = "";
    std::string output_dir = "output";
    double dt = 1.0;
    std::optional<int> max_ticks;
};

class ConfigLoader {
public:
    static AppConfig loadAppConfig(const std::string &path);
    static ModuleManifest loadManifest(const std::string &path);
    static ScenarioConfig loadScenario(const std::string &path);
};

Criticality parseCriticality(const std::string &value);

} // namespace ecosim
