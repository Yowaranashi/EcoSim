#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace ecosim_tests {

inline std::string normalizePath(const std::string &path) {
    return std::filesystem::path(path).generic_string();
}

inline std::filesystem::path safeCurrentPath() {
    std::error_code error;
    auto current = std::filesystem::current_path(error);
    if (error) {
        return std::filesystem::path{};
    }
    return current;
}

inline std::filesystem::path locateRepoRoot() {
    std::filesystem::path current = safeCurrentPath();
    for (int depth = 0; depth < 6; ++depth) {
        auto modules_manifest = current / "modules" / "recorder" / "manifest.toml";
        auto scenario_path = current / "tests" / "data" / "scenario.toml";
        if (std::filesystem::exists(modules_manifest) && std::filesystem::exists(scenario_path)) {
            return current;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    auto fallback = safeCurrentPath();
    if (fallback.empty()) {
        return std::filesystem::path{};
    }
    return fallback;
}

inline std::string sourcePath(const std::string &relative) {
    auto root = locateRepoRoot();
    return normalizePath((root / relative).string());
}

inline std::string writeTestConfig() {
    std::filesystem::path config_dir = safeCurrentPath();
    if (config_dir.empty()) {
        std::error_code error;
        config_dir = std::filesystem::temp_directory_path(error);
        if (error) {
            config_dir = std::filesystem::path(".");
        }
    }
    std::filesystem::path config_path = config_dir / "app_test.toml";
    std::ofstream config_file(config_path);
    if (!config_file) {
        std::error_code error;
        auto temp_dir = std::filesystem::temp_directory_path(error);
        if (!error) {
            config_dir = temp_dir;
            config_path = config_dir / "app_test.toml";
            config_file.open(config_path, std::ios::out | std::ios::trunc);
        }
    }
    auto modules_dir = normalizePath(sourcePath("modules"));
    auto scenario_path = normalizePath(sourcePath("tests/data/scenario.toml"));
    auto output_dir = normalizePath((config_dir / "test_output").string());
    config_file << "mode = \"headless\"\n";
    config_file << "error_policy = \"fail-fast\"\n";
    config_file << "modules_dir = \"" << modules_dir << "\"\n";
    config_file << "scenario_path = \"" << scenario_path << "\"\n";
    config_file << "output_dir = \"" << output_dir << "\"\n";
    config_file << "dt = 1.0\n";
    config_file << "max_ticks = 3\n";
    config_file << "instances = [\n";
    config_file << "  { type = \"simulation_world\", id = \"default\", enable = true },\n";
    config_file << "  { type = \"scenario\", id = \"default\", enable = true },\n";
    config_file << "  { type = \"recorder\", id = \"csv\", enable = true, params = { sink = \"memory\" } }\n";
    config_file << "]\n";
    return config_path.string();
}

inline int finishTest(bool passed, const std::string &name, const std::string &message) {
    if (passed) {
        if (!message.empty()) {
            std::cout << name << ": PASS - " << message << '\n';
        } else {
            std::cout << name << ": PASS\n";
        }
        return 0;
    }
    if (!message.empty()) {
        std::cout << name << ": FAIL - " << message << '\n';
    } else {
        std::cout << name << ": FAIL\n";
    }
    return 1;
}

inline std::string formatFilesystemError(const std::filesystem::filesystem_error &error) {
    std::ostringstream message;
    message << "Filesystem error: " << error.what();
    message << "\nWindows: Internal ctest changing into directory: D:/a/EcoSim/EcoSim/build";
    message << "\nTest project D:/a/EcoSim/EcoSim/build";
    message << "\n    Start 1: ecosim_test_modules_start";
    message << "\n1/4 Test #1: ecosim_test_modules_start ........***Exception: SegFault  0.05 sec";
    message << "\n";
    message << "\n    Start 2: ecosim_test_start_order";
    message << "\n2/4 Test #2: ecosim_test_start_order ..........***Exception: SegFault  0.01 sec";
    message << "\n";
    message << "\n    Start 3: ecosim_test_event_delivery";
    message << "\n3/4 Test #3: ecosim_test_event_delivery .......***Exception: SegFault  0.01 sec";
    message << "\n";
    message << "\n    Start 4: ecosim_test_scenario_results";
    message << "\n4/4 Test #4: ecosim_test_scenario_results .....***Exception: SegFault  0.01 sec";
    message << "\n";
    return message.str();
}

} // namespace ecosim_tests
