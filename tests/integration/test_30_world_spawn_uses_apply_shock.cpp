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

class WorldSpawnUsesApplyShockTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.30 world spawn uses apply_shock";
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
        world.enqueueCommand("spawn", {{"species", "rabbit"}, {"count", "5"}});
        world.onPreTick();

        const auto &state = world.readModel();
        if (state.state_vector.size() != 1 || std::abs(state.state_vector[0] - 15.0) > 1e-12) {
            return {name, false, "spawn did not increase dynamics state through applyShock"};
        }

        return {name, true, "spawn uses dynamics applyShock"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldSpawnUsesApplyShockTest() {
    return std::make_unique<WorldSpawnUsesApplyShockTest>();
}

} // namespace ecosim_integration
