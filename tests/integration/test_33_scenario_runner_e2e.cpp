#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>

namespace ecosim_integration {

namespace {
std::filesystem::path writeRawScenarioFile(const std::string &file_name, const std::string &content) {
    auto data_dir = generatedDataDir();
    std::filesystem::create_directories(data_dir);
    auto path = data_dir / file_name;
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    file << content;
    return path;
}

bool runScenario(const std::string &test_name,
                 const std::string &scenario_file,
                 const std::string &scenario_content,
                 int max_ticks,
                 ecosim::ReadModel &state,
                 std::string &details) {
    std::ostringstream log_stream;
    ecosim::Logger logger(log_stream);
    ecosim::Application app(logger);

    auto scenario = writeRawScenarioFile(scenario_file, scenario_content);
    auto config = writeAppConfigFile(
        "app_" + scenario_file, scenario, max_ticks,
        {{{"type", "simulation_world"}, {"enable", "true"}}, {{"type", "scenario"}, {"enable", "true"}}});

    if (!app.initialize(config.string()) || !app.startModules()) {
        details = test_name + ": failed to initialize/start modules: " + log_stream.str();
        return false;
    }

    auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
    if (!world) {
        details = test_name + ": simulation_world module not found";
        return false;
    }

    app.runHeadless();
    state = world->readModel();
    return true;
}

bool hasFlag(const ecosim::ReadModel &state, const std::string &flag) {
    return std::find(state.flags.begin(), state.flags.end(), flag) != state.flags.end();
}

std::ptrdiff_t flagPosition(const ecosim::ReadModel &state, const std::string &flag) {
    auto it = std::find(state.flags.begin(), state.flags.end(), flag);
    if (it == state.flags.end()) {
        return -1;
    }
    return std::distance(state.flags.begin(), it);
}

const char *singleSpeciesGlvScenarioPrefix() {
    return R"toml(
scenario_id = "runner-single-glv"
model = "glv"
seed = 12
dt = 0.1
integrator = "euler"
requires = ["simulation_world"]
species = [
  { id = "rabbit", initial_state = 10.0, parameters = { growth = 0.0 } }
]
interaction_matrix = [
  [ 0.0 ]
]
)toml";
}
} // namespace

class ScenarioRunnerGlvE2eTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.33 scenario runner gLV e2e";
        ecosim::ReadModel state;
        std::string details;
        if (!runScenario(name, "scenario_test_33_runner_glv.toml", R"toml(
scenario_id = "runner-glv"
model = "glv"
seed = 42
dt = 0.1
stop_at_tick = 3
integrator = "euler"
requires = ["simulation_world"]
species = [
  { id = "rabbit", initial_state = 10.0 },
  { id = "fox", initial_state = 2.0 }
]
growth = { rabbit = 0.0, fox = -0.3 }
sensitivity = { rabbit = 1.0, fox = 0.0 }
parameters = { input.rabbit = 1.0 }
interaction_matrix = [
  [ 0.0, 0.0 ],
  [ 0.0, 0.0 ]
]
schedule = []
)toml",
                         10, state, details)) {
            return {name, false, details};
        }

        if (state.model_id != "glv" || state.scenario_id != "runner-glv" || state.state_vector.size() != 2 ||
            state.species_names.size() != 2 || state.metrics.count("biomass_total") == 0) {
            return {name, false, "gLV scenario did not reach SimulationWorld with extended state"};
        }
        if (state.state_vector[0] <= 10.5 || state.state_vector[1] >= 2.0) {
            return {name, false, "gLV growth/sensitivity fields did not affect dynamics"};
        }

        return {name, true, "ScenarioRunner configured gLV dynamics from TOML"};
    }
};

class ScenarioRunnerRmE2eTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.34 scenario runner RM e2e";
        ecosim::ReadModel state;
        std::string details;
        if (!runScenario(name, "scenario_test_34_runner_rm.toml", R"toml(
scenario_id = "runner-rm"
model = "rosenzweig_macarthur"
seed = 77
dt = 0.1
stop_at_tick = 3
integrator = "rk4"
requires = ["simulation_world"]
species = [
  { id = "prey", initial_state = 10.0 },
  { id = "predator", initial_state = 5.0 }
]
parameters = { r = 1.0, K = 100.0, a = 0.1, h = 0.2, e = 0.5, m = 0.2 }
schedule = []
)toml",
                         10, state, details)) {
            return {name, false, details};
        }

        if (state.model_id != "rosenzweig_macarthur" || state.state_vector.size() != 2 ||
            state.metrics.count("prey") == 0 || state.metrics.count("predator") == 0 ||
            state.metrics.count("biomass_total") == 0) {
            return {name, false, "Rosenzweig-MacArthur metrics/state did not reach ReadModel"};
        }

        return {name, true, "ScenarioRunner configured Rosenzweig-MacArthur dynamics from TOML"};
    }
};

class ScenarioRunnerScheduleSetParamTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.35 scenario runner schedule set_param";
        ecosim::ReadModel state;
        std::string details;
        std::string scenario = std::string(singleSpeciesGlvScenarioPrefix()) + R"toml(
stop_at_tick = 5
schedule = [
  { tick = 2, command = "set_param", name = "growth.rabbit", value = 1.0 }
]
)toml";
        if (!runScenario(name, "scenario_test_35_runner_set_param.toml", scenario, 10, state, details)) {
            return {name, false, details};
        }

        if (!hasFlag(state, "param_changed.growth.rabbit")) {
            return {name, false, "set_param schedule action did not add param_changed flag"};
        }
        if (state.state_vector.size() != 1 || state.state_vector[0] <= 10.5) {
            return {name, false, "set_param schedule action did not affect the trajectory"};
        }

        return {name, true, "scheduled set_param is forwarded to SimulationWorld"};
    }
};

class ScenarioRunnerScheduleApplyShockTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.36 scenario runner schedule apply_shock";
        ecosim::ReadModel state;
        std::string details;
        std::string scenario = std::string(singleSpeciesGlvScenarioPrefix()) + R"toml(
stop_at_tick = 5
schedule = [
  { tick = 3, command = "apply_shock", target = "rabbit", strength = -4.0 }
]
)toml";
        if (!runScenario(name, "scenario_test_36_runner_apply_shock.toml", scenario, 10, state, details)) {
            return {name, false, details};
        }

        if (!hasFlag(state, "shock.rabbit")) {
            return {name, false, "apply_shock schedule action did not add shock flag"};
        }
        if (state.state_vector.size() != 1 || std::abs(state.state_vector[0] - 6.0) > 1e-9) {
            return {name, false, "apply_shock schedule action did not change dynamics state"};
        }

        return {name, true, "scheduled apply_shock is forwarded to SimulationWorld"};
    }
};

class ScenarioRunnerScheduleOrderTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.37 scenario runner schedule order";
        ecosim::ReadModel state;
        std::string details;
        std::string scenario = std::string(singleSpeciesGlvScenarioPrefix()) + R"toml(
stop_at_tick = 3
schedule = [
  { tick = 1, command = "spawn", species = "rabbit", count = 5.0 },
  { tick = 1, command = "apply_shock", target = "rabbit", strength = -3.0 }
]
)toml";
        if (!runScenario(name, "scenario_test_37_runner_order.toml", scenario, 10, state, details)) {
            return {name, false, details};
        }

        const auto spawn = flagPosition(state, "spawn.rabbit");
        const auto shock = flagPosition(state, "shock.rabbit");
        if (spawn < 0 || shock < 0 || spawn >= shock) {
            return {name, false, "same-tick schedule actions were not applied in TOML order"};
        }
        if (state.state_vector.size() != 1 || std::abs(state.state_vector[0] - 12.0) > 1e-9) {
            return {name, false, "same-tick schedule actions produced unexpected state"};
        }

        return {name, true, "same-tick schedule actions preserve TOML order"};
    }
};

std::unique_ptr<IIntegrationTest> makeScenarioRunnerGlvE2eTest() {
    return std::make_unique<ScenarioRunnerGlvE2eTest>();
}

std::unique_ptr<IIntegrationTest> makeScenarioRunnerRmE2eTest() {
    return std::make_unique<ScenarioRunnerRmE2eTest>();
}

std::unique_ptr<IIntegrationTest> makeScenarioRunnerScheduleSetParamTest() {
    return std::make_unique<ScenarioRunnerScheduleSetParamTest>();
}

std::unique_ptr<IIntegrationTest> makeScenarioRunnerScheduleApplyShockTest() {
    return std::make_unique<ScenarioRunnerScheduleApplyShockTest>();
}

std::unique_ptr<IIntegrationTest> makeScenarioRunnerScheduleOrderTest() {
    return std::make_unique<ScenarioRunnerScheduleOrderTest>();
}

} // namespace ecosim_integration
