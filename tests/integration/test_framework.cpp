#include "integration/test_framework.h"

#include <fstream>

namespace ecosim_integration {

namespace {
std::string boolLiteral(const std::string &value) {
    return value == "true" ? "true" : "false";
}
} // namespace

std::filesystem::path repoRoot() {
    auto current = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {
        if (std::filesystem::exists(current / "modules" / "simulation_world" / "manifest.toml") &&
            std::filesystem::exists(current / "src" / "core" / "app.cpp")) {
            return current;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return std::filesystem::current_path();
}

std::filesystem::path writeScenarioFile(const std::string &file_name,
                                        int seed,
                                        int stop_at_tick,
                                        const std::vector<std::string> &requires,
                                        const std::vector<std::map<std::string, std::string>> &schedule) {
    auto root = repoRoot();
    auto runtime_data_dir = root / "build" / "test_runtime_data";
    std::filesystem::create_directories(runtime_data_dir);
    auto path = runtime_data_dir / file_name;
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    file << "seed = " << seed << '\n';
    file << "stop_at_tick = " << stop_at_tick << '\n';

    file << "requires = [";
    for (std::size_t i = 0; i < requires.size(); ++i) {
        if (i != 0) {
            file << ", ";
        }
        file << '"' << requires[i] << '"';
    }
    file << "]\n";

    file << "schedule = [\n";
    for (const auto &entry : schedule) {
        file << "  { ";
        bool first = true;
        for (const auto &pair : entry) {
            if (!first) {
                file << ", ";
            }
            first = false;
            if (pair.first == "tick" || pair.first == "count") {
                file << pair.first << " = " << pair.second;
            } else {
                file << pair.first << " = \"" << pair.second << "\"";
            }
        }
        file << " },\n";
    }
    file << "]\n";
    return path;
}

std::filesystem::path writeAppConfigFile(const std::string &file_name,
                                         const std::filesystem::path &scenario_path,
                                         int max_ticks,
                                         const std::vector<std::map<std::string, std::string>> &instances) {
    auto root = repoRoot();
    auto runtime_data_dir = root / "build" / "test_runtime_data";
    std::filesystem::create_directories(runtime_data_dir);
    auto path = runtime_data_dir / file_name;
    std::ofstream file(path, std::ios::out | std::ios::trunc);

    file << "mode = \"headless\"\n";
    file << "error_policy = \"fail-fast\"\n";
    file << "modules_dir = \"" << (root / "modules").generic_string() << "\"\n";
    file << "scenario_path = \"" << scenario_path.generic_string() << "\"\n";
    file << "output_dir = \"" << (runtime_data_dir / "output").generic_string() << "\"\n";
    file << "dt = 1.0\n";
    file << "max_ticks = " << max_ticks << "\n";

    file << "instances = [\n";
    for (const auto &instance : instances) {
        auto type = instance.at("type");
        auto id = instance.count("id") ? instance.at("id") : "default";
        auto enable = instance.count("enable") ? boolLiteral(instance.at("enable")) : "true";
        file << "  { type = \"" << type << "\", id = \"" << id << "\", enable = " << enable;
        auto sink_it = instance.find("sink");
        if (sink_it != instance.end()) {
            file << ", params = { sink = \"" << sink_it->second << "\" }";
        }
        file << " },\n";
    }
    file << "]\n";

    return path;
}

bool containsText(const std::string &log, const std::string &needle) {
    return log.find(needle) != std::string::npos;
}

} // namespace ecosim_integration
