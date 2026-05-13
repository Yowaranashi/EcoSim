#include "core/config.h"

#include "core/utils/toml_parser.h"
#include "models/model_dynamics_factory.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ecosim {

namespace {

using utils::TomlDocument;
using utils::TomlValue;

std::string valueToString(const TomlValue &value, const std::string &fallback = "") {
    auto text = value.asString();
    return text ? *text : fallback;
}

std::string getString(const TomlDocument &document, const std::string &key, const std::string &fallback = "") {
    auto value = document.find(key);
    return value ? valueToString(*value, fallback) : fallback;
}

std::optional<int> getInt(const TomlDocument &document, const std::string &key) {
    auto value = document.find(key);
    return value ? value->asInt() : std::nullopt;
}

std::optional<double> getDouble(const TomlDocument &document, const std::string &key) {
    auto value = document.find(key);
    return value ? value->asDouble() : std::nullopt;
}

std::vector<std::string> stringArray(const TomlValue *value) {
    std::vector<std::string> result;
    if (!value) {
        return result;
    }
    auto array = value->asArray();
    if (!array) {
        return result;
    }
    result.reserve(array->size());
    for (const auto &entry : *array) {
        if (auto text = entry.asString()) {
            result.push_back(*text);
        }
    }
    return result;
}

std::vector<double> numberArray(const TomlValue *value) {
    std::vector<double> result;
    if (!value) {
        return result;
    }
    auto array = value->asArray();
    if (!array) {
        return result;
    }
    result.reserve(array->size());
    for (const auto &entry : *array) {
        if (auto number = entry.asDouble()) {
            result.push_back(*number);
        }
    }
    return result;
}

std::map<std::string, std::string> stringMap(const TomlValue::Table *table) {
    std::map<std::string, std::string> result;
    if (!table) {
        return result;
    }
    for (const auto &entry : *table) {
        if (auto text = entry.second.asString()) {
            result[entry.first] = *text;
        }
    }
    return result;
}

std::map<std::string, double> numberMap(const TomlValue::Table *table) {
    std::map<std::string, double> result;
    if (!table) {
        return result;
    }
    for (const auto &entry : *table) {
        if (auto number = entry.second.asDouble()) {
            result[entry.first] = *number;
        }
    }
    return result;
}

std::map<std::string, double> numberMap(const TomlValue *value) {
    return value ? numberMap(value->asTable()) : std::map<std::string, double>{};
}

std::vector<std::vector<double>> numberMatrix(const TomlValue *value) {
    std::vector<std::vector<double>> result;
    if (!value) {
        return result;
    }
    auto rows = value->asArray();
    if (!rows) {
        return result;
    }
    result.reserve(rows->size());
    for (const auto &row_value : *rows) {
        auto row_items = row_value.asArray();
        if (!row_items) {
            continue;
        }
        std::vector<double> row;
        row.reserve(row_items->size());
        for (const auto &cell : *row_items) {
            if (auto number = cell.asDouble()) {
                row.push_back(*number);
            }
        }
        if (!row.empty()) {
            result.push_back(std::move(row));
        }
    }
    return result;
}

void mergeNumberMap(std::map<std::string, double> &target, const std::map<std::string, double> &values) {
    for (const auto &entry : values) {
        target[entry.first] = entry.second;
    }
}

void mergeQualifiedNumberMap(std::map<std::string, double> &target,
                             const std::string &prefix,
                             const std::map<std::string, double> &values) {
    for (const auto &entry : values) {
        target[prefix + "." + entry.first] = entry.second;
    }
}

bool isSupportedIntegrator(const std::string &integrator) {
    return integrator == "euler" || integrator == "rk4";
}

void applyRootOrTableNumberMap(ScenarioConfig &scenario,
                               const TomlDocument &document,
                               const std::string &name) {
    mergeNumberMap(scenario.model.parameters, numberMap(document.find(name)));
    mergeNumberMap(scenario.model.parameters, numberMap(document.findTable(name)));
}

void applyMaybeQualifiedMap(std::map<std::string, double> &parameters,
                            std::vector<double> &positional,
                            const TomlValue *value,
                            const std::string &prefix) {
    if (!value) {
        return;
    }
    if (value->isArray()) {
        positional = numberArray(value);
    } else {
        mergeQualifiedNumberMap(parameters, prefix, numberMap(value));
    }
}

SpeciesConfig parseSpeciesTable(const TomlValue::Table &table, ModelConfig &model) {
    SpeciesConfig species;
    if (auto id = table.find("id"); id != table.end()) {
        species.id = valueToString(id->second);
    } else if (auto id = table.find("species"); id != table.end()) {
        species.id = valueToString(id->second);
    }

    const auto initial_it = table.find("initial_state") != table.end() ? table.find("initial_state")
                                                                       : table.find("initial");
    if (initial_it != table.end()) {
        if (auto initial = initial_it->second.asDouble()) {
            species.initial_state = *initial;
            if (!species.id.empty()) {
                model.initial_state[species.id] = *initial;
            }
        }
    }

    if (auto params = table.find("parameters"); params != table.end()) {
        species.parameters = numberMap(params->second.asTable());
    }
    return species;
}

} // namespace

Criticality parseCriticality(const std::string &value) {
    if (value == "Critical") {
        return Criticality::Critical;
    }
    if (value == "Important") {
        return Criticality::Important;
    }
    return Criticality::Optional;
}

AppConfig ConfigLoader::loadAppConfig(const std::string &path) {
    const auto document = TomlDocument::parseFile(path);
    AppConfig config;

    config.mode = getString(document, "mode", config.mode);
    const auto policy = getString(document, "error_policy");
    if (!policy.empty()) {
        config.error_policy = (policy == "auto-disable") ? ErrorPolicy::AutoDisable : ErrorPolicy::FailFast;
    }
    config.modules_dir = getString(document, "modules_dir", config.modules_dir);
    config.scenario_path = getString(document, "scenario_path", config.scenario_path);
    config.output_dir = getString(document, "output_dir", config.output_dir);
    if (auto dt = getDouble(document, "dt")) {
        config.dt = *dt;
    }
    if (auto max_ticks = getInt(document, "max_ticks")) {
        config.max_ticks = *max_ticks;
    }

    if (auto instances = document.find("instances")) {
        if (auto array = instances->asArray()) {
            for (const auto &entry : *array) {
                auto table = entry.asTable();
                if (!table) {
                    continue;
                }

                ModuleInstanceConfig instance;
                if (auto type = table->find("type"); type != table->end()) {
                    instance.type_id = valueToString(type->second);
                }
                if (auto id = table->find("id"); id != table->end()) {
                    instance.instance_id = valueToString(id->second);
                }
                if (auto enabled = table->find("enable"); enabled != table->end()) {
                    if (auto parsed = enabled->second.asBool()) {
                        instance.enabled = *parsed;
                    }
                }
                if (auto params = table->find("params"); params != table->end()) {
                    instance.params = stringMap(params->second.asTable());
                }
                if (!instance.type_id.empty()) {
                    config.instances.push_back(std::move(instance));
                }
            }
        }
    }

    return config;
}

ModuleManifest ConfigLoader::loadManifest(const std::string &path) {
    const auto document = TomlDocument::parseFile(path);
    ModuleManifest manifest;
    manifest.type_id = getString(document, "id");
    manifest.version = getString(document, "version");
    manifest.dependencies = stringArray(document.find("dependencies"));
    manifest.library_path = getString(document, "library");
    if (auto criticality = document.find("criticality")) {
        manifest.criticality = parseCriticality(valueToString(*criticality));
    }
    return manifest;
}

ScenarioConfig ConfigLoader::loadScenario(const std::string &path) {
    const auto document = TomlDocument::parseFile(path);
    ScenarioConfig scenario;

    scenario.scenario_id = getString(document, "scenario_id");
    if (scenario.scenario_id.empty()) {
        scenario.scenario_id = std::filesystem::path(path).stem().string();
    }

    auto model_id = getString(document, "model");
    if (model_id.empty()) {
        model_id = getString(document, "model_id");
    }
    if (!model_id.empty()) {
        if (!isSupportedModelId(model_id)) {
            throw std::runtime_error("Unsupported scenario model: " + model_id);
        }
        scenario.model.model_id = model_id;
    }
    if (auto seed = getInt(document, "seed")) {
        scenario.seed = *seed;
    }
    if (auto dt = getDouble(document, "dt")) {
        scenario.integrator.dt = *dt;
    }
    if (auto stop_at_tick = getInt(document, "stop_at_tick")) {
        scenario.stop_at_tick = *stop_at_tick;
    }
    auto integrator = getString(document, "integrator");
    if (!integrator.empty()) {
        if (!isSupportedIntegrator(integrator)) {
            throw std::runtime_error("Unsupported scenario integrator: " + integrator);
        }
        scenario.integrator.type = integrator;
    }
    scenario.requires = stringArray(document.find("requires"));

    std::vector<double> positional_initial_state;
    std::vector<double> positional_growth;
    std::vector<double> positional_sensitivity;

    if (auto initial_state = document.find("initial_state")) {
        if (initial_state->isArray()) {
            positional_initial_state = numberArray(initial_state);
        } else {
            scenario.model.initial_state = numberMap(initial_state);
        }
    }
    applyRootOrTableNumberMap(scenario, document, "parameters");
    applyRootOrTableNumberMap(scenario, document, "params");

    applyMaybeQualifiedMap(scenario.model.parameters, positional_growth, document.find("growth"), "growth");
    applyMaybeQualifiedMap(scenario.model.parameters, positional_growth, document.find("growth_rates"), "growth");
    applyMaybeQualifiedMap(scenario.model.parameters, positional_sensitivity, document.find("sensitivity"),
                           "sensitivity");

    scenario.model.interaction_matrix = numberMatrix(document.find("interaction_matrix"));

    if (auto species_value = document.find("species")) {
        if (auto species_array = species_value->asArray()) {
            for (const auto &entry : *species_array) {
                if (auto text = entry.asString()) {
                    SpeciesConfig species;
                    species.id = *text;
                    scenario.model.species.push_back(std::move(species));
                } else if (auto table = entry.asTable()) {
                    auto species = parseSpeciesTable(*table, scenario.model);
                    if (!species.id.empty()) {
                        scenario.model.species.push_back(std::move(species));
                    }
                }
            }
        }
    }

    for (std::size_t i = 0; i < scenario.model.species.size(); ++i) {
        auto &species = scenario.model.species[i];
        if (i < positional_initial_state.size()) {
            species.initial_state = positional_initial_state[i];
            scenario.model.initial_state[species.id] = positional_initial_state[i];
        }
        if (i < positional_growth.size()) {
            scenario.model.parameters["growth." + species.id] = positional_growth[i];
        }
        if (i < positional_sensitivity.size()) {
            scenario.model.parameters["sensitivity." + species.id] = positional_sensitivity[i];
        }
    }

    if (auto schedule = document.find("schedule")) {
        if (auto schedule_array = schedule->asArray()) {
            for (const auto &entry : *schedule_array) {
                auto table = entry.asTable();
                if (!table) {
                    continue;
                }

                ScheduledAction action;
                if (auto tick = table->find("tick"); tick != table->end()) {
                    if (auto parsed_tick = tick->second.asInt()) {
                        action.tick = *parsed_tick;
                    }
                }
                if (auto command = table->find("command"); command != table->end()) {
                    action.command = valueToString(command->second);
                }
                for (const auto &param : *table) {
                    if (param.first == "tick" || param.first == "command") {
                        continue;
                    }
                    action.params[param.first] = valueToString(param.second);
                }
                scenario.schedule.push_back(std::move(action));
            }
        }
    }

    return scenario;
}

} // namespace ecosim
