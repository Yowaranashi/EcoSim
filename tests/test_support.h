#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace ecosim_tests {

inline std::string normalizePath(const std::string &path) {
    return std::filesystem::path(path).generic_string();
}

inline std::filesystem::path locateRepoRoot() {
    std::filesystem::path current = std::filesystem::current_path();
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
    return std::filesystem::current_path();
}

inline std::string sourcePath(const std::string &relative) {
    auto root = locateRepoRoot();
    return normalizePath((root / relative).string());
}

inline std::string writeTestConfig() {
    std::filesystem::path config_path = std::filesystem::current_path() / "app_test.toml";
    std::ofstream config_file(config_path);
    auto modules_dir = sourcePath("modules");
    auto scenario_path = sourcePath("tests/data/scenario.toml");
    auto output_dir = normalizePath((std::filesystem::current_path() / "test_output").string());
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

} // namespace ecosim_tests
