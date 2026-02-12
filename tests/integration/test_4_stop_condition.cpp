#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

class StopConditionTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.4 stop_at_tick";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);

        auto scenario = writeScenarioFile("scenario_test_4.toml", 11, 3, {"simulation_world"}, {});
        auto config = writeAppConfigFile(
            "app_test_4.toml", scenario, 50,
            {{{"type", "simulation_world"}, {"enable", "true"}}, {{"type", "scenario"}, {"enable", "true"}}});

        if (!app.initialize(config.string()) || !app.startModules()) {
            return {name, false, "не удалось инициализировать/запустить модули"};
        }

        auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            return {name, false, "модуль simulation_world не найден"};
        }

        app.runHeadless();
        if (world->readModel().tick != 3) {
            return {name, false, "ожидалось ровно 3 выполненных тика"};
        }

        return {name, true, "выполнение остановлено при достижении stop_at_tick=3"};
    }
};

std::unique_ptr<IIntegrationTest> makeStopConditionTest() {
    return std::make_unique<StopConditionTest>();
}

} // namespace ecosim_integration
