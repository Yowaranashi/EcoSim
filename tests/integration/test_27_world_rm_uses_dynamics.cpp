#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeRmConfig() {
    ecosim::ModelConfig config;
    config.model_id = "rosenzweig_macarthur";
    config.species = {
        {"prey", 10.0, {}},
        {"predator", 5.0, {}},
    };
    config.parameters = {{"r", 1.0}, {"K", 100.0}, {"a", 0.1}, {"h", 0.2}, {"e", 0.5}, {"m", 0.2}};
    return config;
}
} // namespace

class WorldRmUsesDynamicsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.27 world RM uses dynamics";
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
        configure.model_config = makeRmConfig();
        configure.has_model_config = true;

        world.onInit();
        world.enqueueCommand(configure);
        world.onPreTick();
        for (int i = 0; i < 3; ++i) {
            world.onTick();
        }

        const auto &state = world.readModel();
        if (state.model_id != "rosenzweig_macarthur" || state.state_vector.size() != 2 ||
            state.metrics.count("prey") == 0 || state.metrics.count("predator") == 0 ||
            state.metrics.count("biomass_total") == 0) {
            return {name, false, "RM dynamics did not populate state and metrics"};
        }

        return {name, true, "SimulationWorld advances Rosenzweig-MacArthur dynamics"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldRmUsesDynamicsTest() {
    return std::make_unique<WorldRmUsesDynamicsTest>();
}

} // namespace ecosim_integration
