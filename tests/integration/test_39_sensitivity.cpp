#include "integration/test_framework.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
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

std::vector<std::string> splitCsvSimple(const std::string &line) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(line);
    while (std::getline(stream, current, ',')) {
        parts.push_back(current);
    }
    return parts;
}

std::filesystem::path writeSensitivityAppConfig() {
    auto path = generatedDataDir() / "app_test_50_sensitivity.toml";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    file << "mode = \"console\"\n";
    file << "error_policy = \"fail-fast\"\n";
    file << "modules_dir = \"modules\"\n";
    file << "scenario_path = \"scenario-gfl.toml\"\n";
    file << "output_dir = \"output\"\n";
    file << "max_ticks = 250\n";
    file << "instances = [\n";
    file << "  { type = \"simulation_world\", id = \"default\", enable = true },\n";
    file << "  { type = \"scenario\", id = \"default\", enable = true }\n";
    file << "]\n";
    return path;
}
} // namespace

class SensitivityCsvTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.50 sim.sensitivity CSV";
        const auto output_path = repoRoot() / "output" / "sensitivity_test_h.csv";
        std::filesystem::remove(output_path);

        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);
        auto config = writeSensitivityAppConfig();
        if (!app.initialize(config.string()) || !app.startModules()) {
            return {name, false, "failed to initialize sensitivity app"};
        }

        if (!app.console().execute(
                "sim.sensitivity -s scenario-rm.toml -p h --from 0.01 --to 1.0 --samples 5 --output sensitivity_test_h")) {
            return {name, false, "sim.sensitivity command was not registered"};
        }

        if (!std::filesystem::exists(output_path)) {
            return {name, false, "sensitivity CSV was not created"};
        }
        auto lines = readLines(output_path);
        if (lines.size() != 6) {
            return {name, false, "sensitivity CSV row count does not match samples"};
        }
        const auto first = splitCsvSimple(lines[1]);
        const auto last = splitCsvSimple(lines[5]);
        if (first.size() < 9 || last.size() < 9 || first[2] != "0.01" || last[2] != "1") {
            return {name, false, "sensitivity parameter values do not span --from to --to"};
        }
        if (first.back() == last.back()) {
            return {name, false, "parameter h did not affect final CSV values"};
        }

        app.console().execute("sim.sensitivity -s scenario-rm.toml -p bad --from 0 --to 1 --samples 3 --output bad");
        app.console().execute("sim.sensitivity -s missing.toml -p h --from 0 --to 1 --samples 3 --output bad");
        const auto logs = log_stream.str();
        if (!containsText(logs, "Unsupported Rosenzweig-MacArthur sensitivity parameter") ||
            !containsText(logs, "Scenario file does not exist")) {
            return {name, false, "invalid sensitivity inputs did not produce readable errors"};
        }

        return {name, true, "sim.sensitivity creates an isolated CSV series"};
    }
};

std::unique_ptr<IIntegrationTest> makeSensitivityCsvTest() {
    return std::make_unique<SensitivityCsvTest>();
}

} // namespace ecosim_integration
