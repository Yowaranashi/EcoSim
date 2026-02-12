#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

class SpawnPhaseTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.2 spawn pre-tick phase";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);

        auto scenario = writeScenarioFile(
            "scenario_test_2.toml", 11, 2, {"simulation_world"},
            {{{"tick", "1"}, {"command", "spawn"}, {"species", "boar"}, {"count", "2"}}});
        auto config = writeAppConfigFile(
            "app_test_2.toml", scenario, 2,
            {{{"type", "simulation_world"}, {"enable", "true"}}, {{"type", "scenario"}, {"enable", "true"}}});

        if (!app.initialize(config.string()) || !app.startModules()) {
            return {name, false, "не удалось инициализировать/запустить модули"};
        }

        auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        if (!world) {
            return {name, false, "модуль simulation_world не найден"};
        }

        if (!world->readModel().population_by_species.empty()) {
            return {name, false, "до тика состояние мира должно быть пустым"};
        }

        app.runHeadless();
        auto state = world->readModel();
        auto it = state.population_by_species.find("boar");
        if (state.tick < 1 || it == state.population_by_species.end() || it->second != 3) {
            return {name, false, "после первого тика ожидается boar=3 (2 spawn + 1 onTick)"};
        }

        return {name, true, "spawn применен в onPreTick без преждевременного изменения"};
    }
};

std::unique_ptr<IIntegrationTest> makeSpawnPhaseTest() {
    return std::make_unique<SpawnPhaseTest>();
}

} // namespace ecosim_integration
