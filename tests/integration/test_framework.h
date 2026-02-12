#pragma once

#include "core/app.h"
#include "core/logger.h"

#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ecosim_integration {

struct TestResult {
    std::string name;
    bool passed = false;
    std::string details;
};

class IIntegrationTest {
public:
    virtual ~IIntegrationTest() = default;
    virtual TestResult run() = 0;
};

std::filesystem::path repoRoot();
std::filesystem::path writeScenarioFile(const std::string &file_name,
                                        int seed,
                                        int stop_at_tick,
                                        const std::vector<std::string> &requires,
                                        const std::vector<std::map<std::string, std::string>> &schedule);
std::filesystem::path writeAppConfigFile(const std::string &file_name,
                                         const std::filesystem::path &scenario_path,
                                         int max_ticks,
                                         const std::vector<std::map<std::string, std::string>> &instances);

bool containsText(const std::string &log, const std::string &needle);

} // namespace ecosim_integration
