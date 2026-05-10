#include "integration/test_framework.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace ecosim_integration {

namespace {
std::vector<std::string> readLines(const std::filesystem::path &path) {
    std::ifstream file(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

bool containsColumn(const std::string &header, const std::string &column) {
    return header == column || header.find(column + ",") == 0 ||
           header.find("," + column + ",") != std::string::npos ||
           (header.size() >= column.size() && header.rfind("," + column) == header.size() - column.size() - 1);
}

bool runExampleApp(const std::string &app_config_name,
                   std::vector<std::string> &csv_lines,
                   std::string &details) {
    const auto root = repoRoot();
    const auto config_path = root / "configs" / "examples" / app_config_name;
    const auto csv_path = root / "output" / "examples" / "simulation.csv";
    std::filesystem::remove(csv_path);

    std::ostringstream log_stream;
    ecosim::Logger logger(log_stream);
    ecosim::Application app(logger);
    if (!app.initialize(config_path.string()) || !app.startModules()) {
        details = "failed to initialize/start example app: " + log_stream.str();
        return false;
    }

    app.runHeadless();
    app.shutdown();

    if (!std::filesystem::exists(csv_path)) {
        details = "CSV was not created at " + csv_path.string();
        return false;
    }

    csv_lines = readLines(csv_path);
    if (csv_lines.empty()) {
        details = "CSV file is empty";
        return false;
    }
    return true;
}
} // namespace

class GlvExampleTomlToCsvTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.43 example gLV TOML to CSV";
        std::vector<std::string> lines;
        std::string details;
        if (!runExampleApp("app_glv.toml", lines, details)) {
            return {name, false, details};
        }

        const auto &header = lines.front();
        if (lines.size() < 100 || lines.size() > 102) {
            return {name, false, "CSV line count is not close to stop_at_tick=100"};
        }
        for (const auto &column : {"tick", "time", "dt", "scenario_id", "model_id", "checksum",
                                   "state.grass", "state.rabbit", "state.fox", "metric.biomass_total"}) {
            if (!containsColumn(header, column)) {
                return {name, false, std::string("CSV header is missing column ") + column};
            }
        }

        return {name, true, "gLV example runs from TOML and writes extended CSV"};
    }
};

class RmExampleTomlToCsvTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.44 example RM TOML to CSV";
        std::vector<std::string> lines;
        std::string details;
        if (!runExampleApp("app_rm.toml", lines, details)) {
            return {name, false, details};
        }

        const auto &header = lines.front();
        if (lines.size() < 200 || lines.size() > 202) {
            return {name, false, "CSV line count is not close to stop_at_tick=200"};
        }
        for (const auto &column : {"state.prey", "state.predator"}) {
            if (!containsColumn(header, column)) {
                return {name, false, std::string("CSV header is missing column ") + column};
            }
        }
        if (!containsColumn(header, "metric.phase_x") && !containsColumn(header, "metric.phase_y") &&
            !containsColumn(header, "metric.prey") && !containsColumn(header, "metric.predator")) {
            return {name, false, "CSV header is missing RM phase or species metrics"};
        }

        return {name, true, "Rosenzweig-MacArthur example runs from TOML and writes extended CSV"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvExampleTomlToCsvTest() {
    return std::make_unique<GlvExampleTomlToCsvTest>();
}

std::unique_ptr<IIntegrationTest> makeRmExampleTomlToCsvTest() {
    return std::make_unique<RmExampleTomlToCsvTest>();
}

} // namespace ecosim_integration
