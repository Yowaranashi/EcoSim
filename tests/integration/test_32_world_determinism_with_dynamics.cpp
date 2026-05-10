#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeConfig() {
    ecosim::ModelConfig config;
    config.model_id = "rosenzweig_macarthur";
    config.species = {
        {"prey", 10.0, {}},
        {"predator", 5.0, {}},
    };
    config.parameters = {{"r", 1.0}, {"K", 100.0}, {"a", 0.1}, {"h", 0.2}, {"e", 0.5}, {"m", 0.2}};
    return config;
}

std::string runOnce() {
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
    configure.params["model_id"] = "rosenzweig_macarthur";
    configure.params["integrator"] = "rk4";
    configure.numeric_params["dt"] = 0.1;
    configure.model_config = makeConfig();
    configure.has_model_config = true;

    world.onInit();
    world.enqueueCommand("world.reset", {{"seed", "101"}});
    world.enqueueCommand(configure);
    world.onPreTick();
    for (int i = 0; i < 5; ++i) {
        world.onTick();
    }
    return world.checksum();
}
} // namespace

class WorldDeterminismWithDynamicsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.32 world determinism with dynamics";
        const auto first = runOnce();
        const auto second = runOnce();

        if (first.empty() || first != second) {
            return {name, false, "identical dynamics runs produced different checksums"};
        }

        return {name, true, "dynamics runs are deterministic for identical inputs"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldDeterminismWithDynamicsTest() {
    return std::make_unique<WorldDeterminismWithDynamicsTest>();
}

} // namespace ecosim_integration
