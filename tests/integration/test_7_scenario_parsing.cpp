#include "integration/test_framework.h"

#include "core/config.h"

#include <fstream>
#include <memory>
#include <stdexcept>

namespace ecosim_integration {

namespace {
std::filesystem::path writeRawScenarioFile(const std::string &file_name, const std::string &content) {
    auto data_dir = repoRoot() / "tests" / "data";
    if (!std::filesystem::exists(data_dir)) {
        data_dir = repoRoot() / "data";
    }
    std::filesystem::create_directories(data_dir);
    auto path = data_dir / file_name;
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    file << content;
    return path;
}

bool testLegacyScenario(std::string &details) {
    auto path = writeScenarioFile(
        "scenario_test_7_legacy.toml", 11, 3, {"simulation_world", "recorder"},
        {{{"tick", "1"}, {"command", "spawn"}, {"species", "boar"}, {"count", "2"}},
         {{"tick", "2"}, {"command", "spawn"}, {"species", "deer"}, {"count", "1"}}});

    auto scenario = ecosim::ConfigLoader::loadScenario(path.string());
    if (scenario.seed != 11 || scenario.stop_at_tick != 3 || scenario.requires.size() != 2 ||
        scenario.schedule.size() != 2 || scenario.schedule[0].command != "spawn" ||
        scenario.schedule[0].params.at("species") != "boar" || !scenario.model.model_id.empty()) {
        details = "legacy scenario parsed with unexpected values";
        return false;
    }
    return true;
}

bool testGlvScenario(std::string &details) {
    auto path = writeRawScenarioFile("scenario_test_7_glv.toml", R"toml(
scenario_id = "glv-basic"
model = "glv"
seed = 123
dt = 0.25
stop_at_tick = 10
integrator = "rk4"
species = [
  { id = "prey", initial_state = 20.0, parameters = { growth_rate = 1.1 } },
  { id = "predator", initial_state = 5.0, parameters = { mortality = 0.4 } }
]
parameters = { carrying_capacity = 100.0 }
interaction_matrix = [
  [ 0.2, -0.1 ],
  [ 0.05, -0.3 ]
]
schedule = [
  { tick = 2, command = "set_param", species = "prey", name = "growth_rate", value = 1.2 },
  { tick = 3, command = "apply_shock", species = "predator", amount = -1.0 },
  { tick = 4, command = "spawn", species = "prey", count = 7 }
]
)toml");

    auto scenario = ecosim::ConfigLoader::loadScenario(path.string());
    if (scenario.scenario_id != "glv-basic" || scenario.model.model_id != "glv" ||
        scenario.integrator.type != "rk4" || scenario.integrator.dt != 0.25 || scenario.model.species.size() != 2 ||
        scenario.model.initial_state.at("prey") != 20.0 ||
        scenario.model.species[0].parameters.at("growth_rate") != 1.1 ||
        scenario.model.parameters.at("carrying_capacity") != 100.0 ||
        scenario.model.interaction_matrix.size() != 2 || scenario.model.interaction_matrix[1][0] != 0.05 ||
        scenario.schedule.size() != 3 || scenario.schedule[0].command != "set_param" ||
        scenario.schedule[2].params.at("count") != "7") {
        details = "gLV scenario parsed with unexpected values";
        return false;
    }
    return true;
}

bool testRosenzweigMacarthurScenario(std::string &details) {
    auto path = writeRawScenarioFile("scenario_test_7_rosenzweig_macarthur.toml", R"toml(
scenario_id = "rm-basic"
model = "rosenzweig_macarthur"
seed = 77
dt = 0.1
stop_at_tick = 20
integrator = "euler"
initial_state = { resource = 30.0, consumer = 4.0 }
parameters = { resource_growth = 1.0, carrying_capacity = 50.0, attack_rate = 0.2, handling_time = 0.1 }
species = [
  { id = "resource", initial_state = 30.0 },
  { id = "consumer", initial_state = 4.0, parameters = { mortality = 0.3, conversion_efficiency = 0.5 } }
]
schedule = [
  { tick = 5, command = "set_param", name = "attack_rate", value = 0.25 },
  { tick = 6, command = "apply_shock", species = "resource", amount = -3.0 }
]
)toml");

    auto scenario = ecosim::ConfigLoader::loadScenario(path.string());
    if (scenario.model.model_id != "rosenzweig_macarthur" || scenario.integrator.type != "euler" ||
        scenario.integrator.dt != 0.1 || scenario.model.initial_state.at("consumer") != 4.0 ||
        scenario.model.parameters.at("handling_time") != 0.1 ||
        scenario.model.species[1].parameters.at("conversion_efficiency") != 0.5 ||
        scenario.schedule.size() != 2 || scenario.schedule[1].command != "apply_shock") {
        details = "Rosenzweig-MacArthur scenario parsed with unexpected values";
        return false;
    }
    return true;
}

bool testInvalidModel(std::string &details) {
    auto path = writeRawScenarioFile("scenario_test_7_invalid_model.toml", R"toml(
model = "unsupported_model"
seed = 1
stop_at_tick = 1
)toml");

    try {
        (void)ecosim::ConfigLoader::loadScenario(path.string());
    } catch (const std::runtime_error &) {
        return true;
    }

    details = "invalid model did not throw";
    return false;
}
} // namespace

class ScenarioParsingTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.7 scenario parsing";
        std::string details;
        if (!testLegacyScenario(details) || !testGlvScenario(details) || !testRosenzweigMacarthurScenario(details) ||
            !testInvalidModel(details)) {
            return {name, false, details};
        }
        return {name, true, "legacy, gLV, Rosenzweig-MacArthur and invalid model cases passed"};
    }
};

std::unique_ptr<IIntegrationTest> makeScenarioParsingTest() {
    return std::make_unique<ScenarioParsingTest>();
}

} // namespace ecosim_integration
