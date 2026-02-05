#include "core/config.h"

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

    std::size_t pos = 0;
    while (pos < content.size()) {
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == ',')) {
            ++pos;
        }
        if (pos >= content.size()) {
            break;
        }
        auto key_end = content.find('=', pos);
        if (key_end == std::string::npos) {
            break;
        }
        auto key = trim(content.substr(pos, key_end - pos));
        pos = key_end + 1;
        bool in_quotes = false;
        std::size_t value_start = pos;
        for (; pos < content.size(); ++pos) {
            char c = content[pos];
            if (c == '"') {
                in_quotes = !in_quotes;
            }
            if (!in_quotes && (c == ',')) {
                break;
            }
        }
        auto value = trim(content.substr(value_start, pos - value_start));
        result[key] = stripQuotes(value);
        ++pos;
    }
    return result;
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
    std::size_t pos = 0;
    while (pos < content.size()) {
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == ',')) {
            ++pos;
        }
        if (pos >= content.size()) {
            break;
        }
        std::size_t end = content.find(',', pos);
        if (end == std::string::npos) {
            end = content.size();
        }
        auto value = trim(content.substr(pos, end - pos));
        result.push_back(stripQuotes(value));
        pos = end + 1;
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
    std::size_t pos = 0;
    while (pos < content.size()) {
        auto open = content.find('{', pos);
        if (open == std::string::npos) {
            break;
        }
        auto close = content.find('}', open);
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
    auto pos = input.find(key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    auto eq = input.find('=', pos + key.size());
    if (eq == std::string::npos) {
        return std::nullopt;
    }
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
        std::size_t end = start;
        for (; end < input.size(); ++end) {
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
    if (auto value = findRawValue(content, "seed")) {
        scenario.seed = std::stoi(*value);
    }
    if (auto value = findRawValue(content, "stop_at_tick")) {
        scenario.stop_at_tick = std::stoi(*value);
    }
    if (auto value = findRawValue(content, "requires")) {
        scenario.requires = parseArrayStrings(*value);
    }
    if (auto value = findRawValue(content, "schedule")) {
        auto tables = parseArrayOfTables(*value);
        for (const auto &table : tables) {
            ScenarioConfig::ScheduledAction action;
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
