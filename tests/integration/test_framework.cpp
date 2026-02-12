#include "integration/test_framework.h"

#include <fstream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace ecosim_integration {

namespace {
std::filesystem::path executableDir() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size == buffer.size()) {
        return {};
    }
    buffer.resize(size);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__linux__)
    std::vector<char> buffer(4096, '\0');
    auto size = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (size <= 0) {
        return {};
    }
    buffer[static_cast<std::size_t>(size)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
#else
    return {};
#endif
}

std::filesystem::path findRuntimeBaseFrom(std::filesystem::path current) {
    if (current.empty()) {
        return {};
    }
    for (int i = 0; i < 16; ++i) {
        if (std::filesystem::exists(current / "modules" / "simulation_world" / "manifest.toml")) {
            return current;
        }
        if (!current.has_parent_path()) {
            break;
        }
        auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

std::string boolLiteral(const std::string &value) {
    return value == "true" ? "true" : "false";
}

std::filesystem::path findRuntimeBase() {
    if (auto from_exe = findRuntimeBaseFrom(executableDir()); !from_exe.empty()) {
        return from_exe;
    }
    if (auto from_cwd = findRuntimeBaseFrom(std::filesystem::current_path()); !from_cwd.empty()) {
        return from_cwd;
    }
    return std::filesystem::current_path();
}

std::filesystem::path findDataDir() {
    auto base = findRuntimeBase();
    if (std::filesystem::exists(base / "data")) {
        return base / "data";
    }
    if (std::filesystem::exists(base / "tests" / "data")) {
        return base / "tests" / "data";
    }

    for (auto start : {executableDir(), std::filesystem::current_path()}) {
        auto current = start;
        if (current.empty()) {
            continue;
        }
        for (int i = 0; i < 16; ++i) {
            if (std::filesystem::exists(current / "data")) {
                return current / "data";
            }
            if (std::filesystem::exists(current / "tests" / "data")) {
                return current / "tests" / "data";
            }
            if (!current.has_parent_path()) {
                break;
            }
            auto parent = current.parent_path();
            if (parent == current) {
                break;
            }
            current = parent;
        }
    }

    return base / "data";
}
} // namespace

std::filesystem::path repoRoot() {
    return findRuntimeBase();
}

std::filesystem::path writeScenarioFile(const std::string &file_name,
                                        int seed,
                                        int stop_at_tick,
                                        const std::vector<std::string> &requires,
                                        const std::vector<std::map<std::string, std::string>> &schedule) {
    auto data_dir = findDataDir();
    std::filesystem::create_directories(data_dir);
    auto path = data_dir / file_name;
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
    auto base = findRuntimeBase();
    auto data_dir = findDataDir();
    std::filesystem::create_directories(data_dir);
    auto path = data_dir / file_name;
    std::ofstream file(path, std::ios::out | std::ios::trunc);

    file << "mode = \"headless\"\n";
    file << "error_policy = \"fail-fast\"\n";
    file << "modules_dir = \"" << (base / "modules").generic_string() << "\"\n";
    file << "scenario_path = \"" << scenario_path.generic_string() << "\"\n";
    file << "output_dir = \"" << (base / "output").generic_string() << "\"\n";
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
