#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <cmath>
#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeGlvConfig() {
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

ecosim::WorldCommand makeConfigureCommand() {
    ecosim::WorldCommand command;
    command.command = "world.configure";
    command.params["scenario_id"] = "world-glv";
    command.params["model_id"] = "glv";
    command.params["integrator"] = "euler";
    command.numeric_params["dt"] = 0.1;
    command.model_config = makeGlvConfig();
    command.has_model_config = true;
    return command;
}
} // namespace

class WorldGlvUsesDynamicsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.26 world gLV uses dynamics";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        ecosim::ModuleContext context(logger, event_bus, app_config);
        ecosim::ModuleInstanceConfig instance;
        instance.type_id = "simulation_world";
        ecosim::SimulationWorld world(instance, context);

        ecosim::SimulationEvent received;
        bool delivered = false;
        event_bus.subscribe("world.tick", [&](const ecosim::SimulationEvent &event) {
            received = event;
            delivered = true;
        });

        world.onInit();
        world.enqueueCommand("world.reset", {{"seed", "42"}});
        world.enqueueCommand(makeConfigureCommand());
        world.onPreTick();

        const auto initial = world.readModel().state_vector;
        for (int i = 0; i < 3; ++i) {
            world.onTick();
        }
        const auto &state = world.readModel();
        if (state.model_id != "glv" || state.state_vector.size() != 2 || state.metrics.count("biomass_total") == 0) {
            return {name, false, "gLV dynamics state or metrics are missing"};
        }
        if (std::abs(state.state_vector[0] - initial[0]) < 1e-9 &&
            std::abs(state.state_vector[1] - initial[1]) < 1e-9) {
            return {name, false, "gLV state did not change after ticks"};
        }

        event_bus.deliverBuffered();
        if (!delivered || received.numeric_vector_payload["state_vector"].size() != 2 ||
            received.string_list_payload["species_names"].size() != 2 ||
            received.payload["model_id"] != "glv" || received.metrics.count("biomass_total") == 0) {
            return {name, false, "world.tick does not expose typed dynamics payload"};
        }

        return {name, true, "SimulationWorld advances gLV through IModelDynamics"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldGlvUsesDynamicsTest() {
    return std::make_unique<WorldGlvUsesDynamicsTest>();
}

} // namespace ecosim_integration
