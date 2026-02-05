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
    auto repo_root = locateRepoRoot();
    std::filesystem::path config_dir = !repo_root.empty() ? repo_root : safeCurrentPath();
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
    auto modules_dir = normalizePath(
        (!repo_root.empty() ? (repo_root / "modules") : std::filesystem::path("modules")).string());
    auto scenario_path = normalizePath(
        (!repo_root.empty() ? (repo_root / "tests/data/scenario.toml") : std::filesystem::path("tests/data/scenario.toml"))
            .string());
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

} // namespace ecosim_tests
