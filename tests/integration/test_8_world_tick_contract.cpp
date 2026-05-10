#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

class WorldTickContractTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.8 world tick contract";

        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        app_config.dt = 0.5;

        ecosim::ModuleContext context(logger, event_bus, app_config);
        ecosim::ModuleInstanceConfig instance;
        instance.type_id = "simulation_world";
        instance.instance_id = "default";
        ecosim::SimulationWorld world(instance, context);

        bool delivered = false;
        ecosim::SimulationEvent received;
        event_bus.subscribe("world.tick", [&](const ecosim::SimulationEvent &event) {
            delivered = true;
            received = event;
        });

        world.onInit();

        ecosim::WorldCommand configure;
        configure.command = "world.configure";
        configure.params["scenario_id"] = "contract-test";
        configure.params["model_id"] = "glv";
        configure.params["integrator"] = "rk4";
        configure.numeric_params["dt"] = 0.5;

        world.enqueueCommand("world.reset", {{"seed", "99"}});
        world.enqueueCommand(configure);
        world.enqueueCommand("spawn", {{"species", "hare"}, {"count", "2"}});
        world.onPreTick();
        world.onTick();

        const auto &state = world.readModel();
        if (state.tick != 1 || state.time != 0.5 || state.dt != 0.5 || state.seed != 99) {
            return {name, false, "basic tick/time/dt/seed fields are incorrect"};
        }
        if (state.scenario_id != "contract-test" || state.model_id != "glv" || state.integrator != "rk4") {
            return {name, false, "scenario/model/integrator metadata was not propagated"};
        }
        if (state.species_names.size() != 1 || state.species_names[0] != "hare" ||
            state.state_vector.size() != 1 || state.state_vector[0] != 3.0) {
            return {name, false, "species/state vector fields are incorrect"};
        }
        if (state.population_by_species.at("hare") != 3 || state.energy_total != 6 ||
            state.metrics.at("total_population") != 3.0 || state.metrics.at("energy_total") != 6.0) {
            return {name, false, "legacy population or aggregate metrics are incorrect"};
        }
        if (state.checksum.empty() || state.checksum != world.checksum()) {
            return {name, false, "checksum is missing or inconsistent"};
        }
        if (state.flags.empty()) {
            return {name, false, "service flags are missing"};
        }
        if (event_bus.bufferedCount() != 1 || delivered) {
            return {name, false, "world.tick must remain buffered until deliverBuffered()"};
        }

        event_bus.deliverBuffered();
        if (!delivered) {
            return {name, false, "world.tick was not delivered after deliverBuffered()"};
        }
        if (received.type != "world.tick" || received.tick != 1 || received.payload.at("tick") != "1" ||
            received.payload.at("scenario_id") != "contract-test" || received.payload.at("model_id") != "glv" ||
            received.payload.at("integrator") != "rk4" || received.payload.at("checksum") != state.checksum) {
            return {name, false, "string payload fields are incomplete"};
        }
        if (received.numeric_payload.at("time") != 0.5 || received.numeric_payload.at("dt") != 0.5 ||
            received.numeric_payload.at("population.hare") != 3.0 ||
            received.numeric_payload.at("metrics.energy_total") != 6.0) {
            return {name, false, "numeric payload fields are incomplete"};
        }
        if (received.string_list_payload.at("species").size() != 1 ||
            received.string_list_payload.at("species")[0] != "hare" ||
            received.numeric_vector_payload.at("state").size() != 1 ||
            received.numeric_vector_payload.at("state")[0] != 3.0 ||
            received.metrics.at("total_population") != 3.0 || received.flags.empty()) {
            return {name, false, "typed event payload fields are incomplete"};
        }

        return {name, true, "ReadModel and buffered world.tick expose the extended contract"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldTickContractTest() {
    return std::make_unique<WorldTickContractTest>();
}

} // namespace ecosim_integration
