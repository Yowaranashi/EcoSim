#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

class WorldStopAtTickTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.31 world stop_at_tick with dynamics";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::EventBus event_bus;
        ecosim::AppConfig app_config;
        ecosim::ModuleContext context(logger, event_bus, app_config);
        ecosim::ModuleInstanceConfig instance;
        instance.type_id = "simulation_world";
        ecosim::SimulationWorld world(instance, context);

        world.onInit();
        world.enqueueCommand("stop_at_tick", {{"tick", "5"}});
        world.onPreTick();
        for (int i = 0; i < 5; ++i) {
            world.onTick();
        }

        if (world.readModel().tick != 5 || !world.shouldStop()) {
            return {name, false, "shouldStop was not true at tick 5"};
        }

        return {name, true, "SimulationWorld stops at configured tick"};
    }
};

std::unique_ptr<IIntegrationTest> makeWorldStopAtTickTest() {
    return std::make_unique<WorldStopAtTickTest>();
}

} // namespace ecosim_integration
