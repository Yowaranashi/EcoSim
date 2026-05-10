#include "core/config.h"

#include "models/model_dynamics_factory.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ecosim {

namespace {
std::string trim(const std::string &value) {
    const char *spaces = " \t\n\r";
    auto start = value.find_first_not_of(spaces);
    if (start == std::string::npos) {
        return "";
    }
    auto end = value.find_last_not_of(spaces);
    return value.substr(start, end - start + 1);
}

std::string stripQuotes(const std::string &value) {
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool isKeyChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}

std::vector<std::string> splitTopLevel(const std::string &input, char delimiter) {
    std::vector<std::string> result;
    bool in_quotes = false;
    char quote = '\0';
    int brace_depth = 0;
    int bracket_depth = 0;
    std::size_t start = 0;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if ((c == '"' || c == '\'') && (i == 0 || input[i - 1] != '\\')) {
            if (!in_quotes) {
                in_quotes = true;
                quote = c;
            } else if (quote == c) {
                in_quotes = false;
            }
            continue;
        }
        if (in_quotes) {
            continue;
        }
        if (c == '{') {
            ++brace_depth;
        } else if (c == '}') {
            --brace_depth;
        } else if (c == '[') {
            ++bracket_depth;
        } else if (c == ']') {
            --bracket_depth;
        } else if (c == delimiter && brace_depth == 0 && bracket_depth == 0) {
            result.push_back(trim(input.substr(start, i - start)));
            start = i + 1;
        }
    }

    auto tail = trim(input.substr(start));
    if (!tail.empty()) {
        result.push_back(tail);
    }
    return result;
}

std::size_t findTopLevelEquals(const std::string &input) {
    bool in_quotes = false;
    char quote = '\0';
    int brace_depth = 0;
    int bracket_depth = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if ((c == '"' || c == '\'') && (i == 0 || input[i - 1] != '\\')) {
            if (!in_quotes) {
                in_quotes = true;
                quote = c;
            } else if (quote == c) {
                in_quotes = false;
            }
            continue;
        }
        if (in_quotes) {
            continue;
        }
        if (c == '{') {
            ++brace_depth;
        } else if (c == '}') {
            --brace_depth;
        } else if (c == '[') {
            ++bracket_depth;
        } else if (c == ']') {
            --bracket_depth;
        } else if (c == '=' && brace_depth == 0 && bracket_depth == 0) {
            return i;
        }
    }
    return std::string::npos;
}

std::string loadFile(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Unable to open file: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string removeComments(const std::string &input) {
    std::ostringstream out;
    std::istringstream stream(input);
    std::string line;
    while (std::getline(stream, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        out << line << '\n';
    }
    return out.str();
}

std::map<std::string, std::string> parseInlineMap(const std::string &input) {
    std::map<std::string, std::string> result;
    std::string content = trim(input);
    if (!content.empty() && content.front() == '{') {
        content.erase(content.begin());
    }
    if (!content.empty() && content.back() == '}') {
        content.pop_back();
    }

    for (const auto &entry : splitTopLevel(content, ',')) {
        auto eq = findTopLevelEquals(entry);
        if (eq == std::string::npos) {
            continue;
        }
        auto key = trim(entry.substr(0, eq));
        auto value = trim(entry.substr(eq + 1));
        result[key] = stripQuotes(value);
    }
    return result;
}

std::map<std::string, double> parseNumberMap(const std::string &input) {
    std::map<std::string, double> result;
    for (const auto &pair : parseInlineMap(input)) {
        result[pair.first] = std::stod(pair.second);
    }
    return result;
}

std::map<std::string, double> parseNumberTable(const std::string &input, const std::string &table_name) {
    std::map<std::string, double> result;
    std::istringstream stream(input);
    std::string line;
    bool active = false;
    const std::string header = "[" + table_name + "]";

    while (std::getline(stream, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.front() == '[' && trimmed.back() == ']' && findTopLevelEquals(trimmed) == std::string::npos) {
            if (trimmed == header) {
                active = true;
                continue;
            }
            if (active) {
                break;
            }
        }
        if (!active) {
            continue;
        }

        auto eq = findTopLevelEquals(trimmed);
        if (eq == std::string::npos) {
            continue;
        }
        auto key = trim(trimmed.substr(0, eq));
        auto value = trim(trimmed.substr(eq + 1));
        auto scalar = stripQuotes(value);
        if (scalar.empty() || scalar.front() == '[' || scalar.front() == '{' || scalar.front() == '"') {
            continue;
        }
        try {
            result[key] = std::stod(scalar);
        } catch (const std::invalid_argument &) {
        }
    }

    return result;
}

void mergeQualifiedNumberMap(std::map<std::string, double> &target,
                             const std::string &prefix,
                             const std::map<std::string, double> &values) {
    for (const auto &pair : values) {
        target[prefix + "." + pair.first] = pair.second;
    }
}

std::vector<std::string> parseArrayStrings(const std::string &input) {
    std::vector<std::string> result;
    std::string content = trim(input);
    if (!content.empty() && content.front() == '[') {
        content.erase(content.begin());
    }
    if (!content.empty() && content.back() == ']') {
        content.pop_back();
    }
    for (const auto &entry : splitTopLevel(content, ',')) {
        auto value = trim(entry);
        if (value.empty()) {
            continue;
        }
        result.push_back(stripQuotes(value));
    }
    return result;
}

std::vector<double> parseArrayNumbers(const std::string &input) {
    std::vector<double> result;
    std::string content = trim(input);
    if (!content.empty() && content.front() == '[') {
        content.erase(content.begin());
    }
    if (!content.empty() && content.back() == ']') {
        content.pop_back();
    }
    for (const auto &entry : splitTopLevel(content, ',')) {
        auto value = trim(entry);
        if (!value.empty()) {
            result.push_back(std::stod(value));
        }
    }
    return result;
}

std::vector<std::vector<double>> parseMatrix(const std::string &input) {
    std::vector<std::vector<double>> result;
    std::string content = trim(input);
    if (!content.empty() && content.front() == '[') {
        content.erase(content.begin());
    }
    if (!content.empty() && content.back() == ']') {
        content.pop_back();
    }
    for (const auto &entry : splitTopLevel(content, ',')) {
        auto row = trim(entry);
        if (!row.empty()) {
            result.push_back(parseArrayNumbers(row));
        }
    }
    return result;
}

std::vector<std::map<std::string, std::string>> parseArrayOfTables(const std::string &input) {
    std::vector<std::map<std::string, std::string>> result;
    std::string content = trim(input);
    if (!content.empty() && content.front() == '[') {
        content.erase(content.begin());
    }
    if (!content.empty() && content.back() == ']') {
        content.pop_back();
    }
    bool in_quotes = false;
    char quote = '\0';
    for (std::size_t pos = 0; pos < content.size();) {
        auto open = content.find('{', pos);
        if (open == std::string::npos) {
            break;
        }
        int depth = 0;
        std::size_t close = std::string::npos;
        for (std::size_t i = open; i < content.size(); ++i) {
            char c = content[i];
            if ((c == '"' || c == '\'') && (i == 0 || content[i - 1] != '\\')) {
                if (!in_quotes) {
                    in_quotes = true;
                    quote = c;
                } else if (quote == c) {
                    in_quotes = false;
                }
                continue;
            }
            if (in_quotes) {
                continue;
            }
            if (c == '{') {
                ++depth;
            } else if (c == '}') {
                --depth;
                if (depth == 0) {
                    close = i;
                    break;
                }
            }
        }
        if (close == std::string::npos) {
            break;
        }
        auto table = content.substr(open, close - open + 1);
        result.push_back(parseInlineMap(table));
        pos = close + 1;
    }
    return result;
}

std::optional<std::string> findRawValue(const std::string &input, const std::string &key) {
    bool in_quotes = false;
    char quote = '\0';
    int brace_depth = 0;
    int bracket_depth = 0;

    for (std::size_t pos = 0; pos < input.size(); ++pos) {
        char c = input[pos];
        if ((c == '"' || c == '\'') && (pos == 0 || input[pos - 1] != '\\')) {
            if (!in_quotes) {
                in_quotes = true;
                quote = c;
            } else if (quote == c) {
                in_quotes = false;
            }
            continue;
        }
        if (in_quotes) {
            continue;
        }
        if (c == '{') {
            ++brace_depth;
            continue;
        }
        if (c == '}') {
            --brace_depth;
            continue;
        }
        if (c == '[') {
            ++bracket_depth;
            continue;
        }
        if (c == ']') {
            --bracket_depth;
            continue;
        }
        if (brace_depth != 0 || bracket_depth != 0 || input.compare(pos, key.size(), key) != 0) {
            continue;
        }

        bool left_ok = pos == 0 || !isKeyChar(input[pos - 1]);
        std::size_t after_key = pos + key.size();
        bool right_ok = after_key >= input.size() || !isKeyChar(input[after_key]);
        if (left_ok && right_ok) {
            std::size_t cursor = after_key;
            while (cursor < input.size() && (input[cursor] == ' ' || input[cursor] == '\t')) {
                ++cursor;
            }
            if (cursor < input.size() && input[cursor] == '=') {
                auto eq = cursor;
                auto start = eq + 1;
                while (start < input.size() && (input[start] == ' ' || input[start] == '\t')) {
                    ++start;
                }
                if (start >= input.size()) {
                    return std::nullopt;
                }
                char opener = input[start];
                if (opener == '[' || opener == '{') {
                    char closer = (opener == '[') ? ']' : '}';
                    int depth = 0;
                    bool in_quotes = false;
                    char quote = '\0';
                    std::size_t end = start;
                    for (; end < input.size(); ++end) {
                        char c = input[end];
                        if ((c == '"' || c == '\'') && (end == 0 || input[end - 1] != '\\')) {
                            if (!in_quotes) {
                                in_quotes = true;
                                quote = c;
                            } else if (quote == c) {
                                in_quotes = false;
                            }
                            continue;
                        }
                        if (in_quotes) {
                            continue;
                        }
                        if (input[end] == opener) {
                            depth++;
                        } else if (input[end] == closer) {
                            depth--;
                            if (depth == 0) {
                                ++end;
                                break;
                            }
                        }
                    }
                    return trim(input.substr(start, end - start));
                }
                auto end = input.find('\n', start);
                if (end == std::string::npos) {
                    end = input.size();
                }
                return trim(input.substr(start, end - start));
            }
        }
    }
    return std::nullopt;
}

bool isSupportedIntegrator(const std::string &integrator) {
    return integrator == "euler" || integrator == "rk4";
}

bool startsWithArray(const std::string &value) {
    auto trimmed = trim(value);
    return !trimmed.empty() && trimmed.front() == '[';
}

bool startsWithInlineMap(const std::string &value) {
    auto trimmed = trim(value);
    return !trimmed.empty() && trimmed.front() == '{';
}
}

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
    auto content = removeComments(loadFile(path));
    AppConfig config;

    if (auto value = findRawValue(content, "mode")) {
        config.mode = stripQuotes(*value);
    }
    if (auto value = findRawValue(content, "error_policy")) {
        auto policy = stripQuotes(*value);
        config.error_policy = (policy == "auto-disable") ? ErrorPolicy::AutoDisable : ErrorPolicy::FailFast;
    }
    if (auto value = findRawValue(content, "modules_dir")) {
        config.modules_dir = stripQuotes(*value);
    }
    if (auto value = findRawValue(content, "scenario_path")) {
        config.scenario_path = stripQuotes(*value);
    }
    if (auto value = findRawValue(content, "output_dir")) {
        config.output_dir = stripQuotes(*value);
    }
    if (auto value = findRawValue(content, "dt")) {
        config.dt = std::stod(*value);
    }
    if (auto value = findRawValue(content, "max_ticks")) {
        config.max_ticks = std::stoi(*value);
    }
    if (auto value = findRawValue(content, "instances")) {
        auto tables = parseArrayOfTables(*value);
        for (const auto &table : tables) {
            ModuleInstanceConfig instance;
            auto type_it = table.find("type");
            if (type_it != table.end()) {
                instance.type_id = type_it->second;
            }
            auto id_it = table.find("id");
            if (id_it != table.end()) {
                instance.instance_id = id_it->second;
            }
            auto enable_it = table.find("enable");
            if (enable_it != table.end()) {
                instance.enabled = (enable_it->second == "true");
            }
            auto params_it = table.find("params");
            if (params_it != table.end()) {
                instance.params = parseInlineMap(params_it->second);
            }
            if (!instance.type_id.empty()) {
                config.instances.push_back(instance);
            }
        }
    }

    return config;
}

ModuleManifest ConfigLoader::loadManifest(const std::string &path) {
    auto content = removeComments(loadFile(path));
    ModuleManifest manifest;
    if (auto value = findRawValue(content, "id")) {
        manifest.type_id = stripQuotes(*value);
    }
    if (auto value = findRawValue(content, "version")) {
        manifest.version = stripQuotes(*value);
    }
    if (auto value = findRawValue(content, "dependencies")) {
        manifest.dependencies = parseArrayStrings(*value);
    }
    if (auto value = findRawValue(content, "criticality")) {
        manifest.criticality = parseCriticality(stripQuotes(*value));
    }
    if (auto value = findRawValue(content, "library")) {
        manifest.library_path = stripQuotes(*value);
    }
    return manifest;
}

ScenarioConfig ConfigLoader::loadScenario(const std::string &path) {
    auto content = removeComments(loadFile(path));
    ScenarioConfig scenario;
    if (auto value = findRawValue(content, "scenario_id")) {
        scenario.scenario_id = stripQuotes(*value);
    }
    std::optional<std::string> model_id;
    if (auto value = findRawValue(content, "model")) {
        model_id = stripQuotes(*value);
    } else if (auto value = findRawValue(content, "model_id")) {
        model_id = stripQuotes(*value);
    }
    if (model_id) {
        if (!isSupportedModelId(*model_id)) {
            throw std::runtime_error("Unsupported scenario model: " + *model_id);
        }
        scenario.model.model_id = *model_id;
    }
    if (auto value = findRawValue(content, "seed")) {
        scenario.seed = std::stoi(*value);
    }
    if (auto value = findRawValue(content, "dt")) {
        scenario.integrator.dt = std::stod(*value);
    }
    if (auto value = findRawValue(content, "stop_at_tick")) {
        scenario.stop_at_tick = std::stoi(*value);
    }
    if (auto value = findRawValue(content, "integrator")) {
        auto integrator = stripQuotes(*value);
        if (!isSupportedIntegrator(integrator)) {
            throw std::runtime_error("Unsupported scenario integrator: " + integrator);
        }
        scenario.integrator.type = integrator;
    }
    if (auto value = findRawValue(content, "requires")) {
        scenario.requires = parseArrayStrings(*value);
    }

    std::vector<double> positional_initial_state;
    std::vector<double> positional_growth;
    std::vector<double> positional_sensitivity;

    if (auto value = findRawValue(content, "initial_state")) {
        if (startsWithArray(*value)) {
            positional_initial_state = parseArrayNumbers(*value);
        } else {
            scenario.model.initial_state = parseNumberMap(*value);
        }
    }
    if (auto value = findRawValue(content, "parameters")) {
        if (startsWithInlineMap(*value)) {
            scenario.model.parameters = parseNumberMap(*value);
        }
    }
    for (const auto &param : parseNumberTable(content, "parameters")) {
        scenario.model.parameters[param.first] = param.second;
    }
    if (auto value = findRawValue(content, "growth")) {
        if (startsWithArray(*value)) {
            positional_growth = parseArrayNumbers(*value);
        } else {
            mergeQualifiedNumberMap(scenario.model.parameters, "growth", parseNumberMap(*value));
        }
    }
    if (auto value = findRawValue(content, "sensitivity")) {
        if (startsWithArray(*value)) {
            positional_sensitivity = parseArrayNumbers(*value);
        } else {
            mergeQualifiedNumberMap(scenario.model.parameters, "sensitivity", parseNumberMap(*value));
        }
    }
    if (auto value = findRawValue(content, "interaction_matrix")) {
        scenario.model.interaction_matrix = parseMatrix(*value);
    }
    if (auto value = findRawValue(content, "species")) {
        if (value->find('{') == std::string::npos) {
            for (const auto &name : parseArrayStrings(*value)) {
                SpeciesConfig species;
                species.id = name;
                scenario.model.species.push_back(species);
            }
        } else {
            auto tables = parseArrayOfTables(*value);
            for (const auto &table : tables) {
                SpeciesConfig species;
                auto id_it = table.find("id");
                if (id_it == table.end()) {
                    id_it = table.find("species");
                }
                if (id_it != table.end()) {
                    species.id = id_it->second;
                }
                auto initial_it = table.find("initial_state");
                if (initial_it == table.end()) {
                    initial_it = table.find("initial");
                }
                if (initial_it != table.end()) {
                    species.initial_state = std::stod(initial_it->second);
                    if (!species.id.empty()) {
                        scenario.model.initial_state[species.id] = species.initial_state;
                    }
                }
                auto params_it = table.find("parameters");
                if (params_it != table.end()) {
                    species.parameters = parseNumberMap(params_it->second);
                }
                if (!species.id.empty()) {
                    scenario.model.species.push_back(species);
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
    if (auto value = findRawValue(content, "schedule")) {
        auto tables = parseArrayOfTables(*value);
        for (const auto &table : tables) {
            ScheduledAction action;
            auto tick_it = table.find("tick");
            if (tick_it != table.end()) {
                action.tick = std::stoi(tick_it->second);
            }
            auto cmd_it = table.find("command");
            if (cmd_it != table.end()) {
                action.command = cmd_it->second;
            }
            for (const auto &pair : table) {
                if (pair.first == "tick" || pair.first == "command") {
                    continue;
                }
                action.params[pair.first] = pair.second;
            }
            scenario.schedule.push_back(action);
        }
    }
    return scenario;
}

} // namespace ecosim
