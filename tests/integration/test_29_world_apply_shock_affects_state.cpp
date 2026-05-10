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
        {"rabbit", 10.0, {{"growth", 0.0}}},
    };
    config.interaction_matrix = {{0.0}};
    return config;
}
} // namespace

class WorldApplyShockAffectsStateTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.29 world apply_shock affects state";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        ecosim::ModuleContext context(logger, event_bus, app_config);
        ecosim::ModuleInstanceConfig instance;
        instance.type_id = "simulation_world";
        ecosim::SimulationWorld world(instance, context);

        ecosim::WorldCommand configure;
        configure.command = "world.configure";
        configure.params["model_id"] = "glv";
        configure.model_config = makeConfig();
        configure.has_model_config = true;

        world.onInit();
        world.enqueueCommand(configure);
        world.onPreTick();
        world.enqueueCommand("apply_shock", {{"target", "rabbit"}, {"strength", "-3"}});
        world.onPreTick();

        const auto &state = world.readModel();
        if (state.state_vector.size() != 1 || std::abs(state.state_vector[0] - 7.0) > 1e-12) {
            return {name, false, "apply_shock did not update dynamics state"};
        }

        return {name, true, "apply_shock is forwarded into IModelDynamics"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldApplyShockAffectsStateTest() {
    return std::make_unique<WorldApplyShockAffectsStateTest>();
}

} // namespace ecosim_integration
