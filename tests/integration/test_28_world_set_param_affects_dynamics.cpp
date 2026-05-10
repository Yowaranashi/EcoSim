#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <cmath>
#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeConfig() {
    ecosim::ModelConfig config;
    config.model_id = "glv";
    config.species = {
        {"rabbit", 10.0, {{"growth", 0.5}}},
        {"fox", 2.0, {{"growth", -0.3}}},
    };
    config.interaction_matrix = {
        {-0.02, -0.1},
        {0.04, -0.01},
    };
    return config;
}

void configureWorld(ecosim::SimulationWorld &world) {
    ecosim::WorldCommand configure;
    configure.command = "world.configure";
    configure.params["model_id"] = "glv";
    configure.params["integrator"] = "euler";
    configure.numeric_params["dt"] = 0.1;
    configure.model_config = makeConfig();
    configure.has_model_config = true;
    world.enqueueCommand(configure);
    world.onPreTick();
}
} // namespace

class WorldSetParamAffectsDynamicsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.28 world set_param affects dynamics";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        ecosim::ModuleContext context(logger, event_bus, app_config);
        ecosim::ModuleInstanceConfig instance;
        instance.type_id = "simulation_world";

        ecosim::SimulationWorld baseline(instance, context);
        baseline.onInit();
        configureWorld(baseline);
        baseline.onTick();
        baseline.onTick();
        const auto baseline_state = baseline.readModel().state_vector;

        ecosim::SimulationWorld changed(instance, context);
        changed.onInit();
        configureWorld(changed);
        changed.onTick();
        changed.enqueueCommand("set_param", {{"name", "growth.rabbit"}, {"value", "1.0"}});
        changed.onPreTick();
        changed.onTick();
        const auto changed_state = changed.readModel().state_vector;

        if (baseline_state.size() != 2 || changed_state.size() != 2 ||
            std::abs(baseline_state[0] - changed_state[0]) < 1e-9) {
            return {name, false, "set_param did not alter the next gLV trajectory"};
        }

        return {name, true, "set_param is forwarded into IModelDynamics"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldSetParamAffectsDynamicsTest() {
    return std::make_unique<WorldSetParamAffectsDynamicsTest>();
}

} // namespace ecosim_integration
