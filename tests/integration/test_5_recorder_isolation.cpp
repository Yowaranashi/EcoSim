#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

namespace {
struct Snapshot {
    int tick = 0;
    int energy = 0;
    std::size_t species_count = 0;
    std::string checksum;
};

Snapshot runScenario(bool with_recorder) {
    std::ostringstream log_stream;
    ecosim::Logger logger(log_stream);
    ecosim::Application app(logger);

    auto scenario = writeScenarioFile(
        with_recorder ? "scenario_test_5_with_rec.toml" : "scenario_test_5_without_rec.toml", 17, 4,
        {"simulation_world"},
        {{{"tick", "1"}, {"command", "spawn"}, {"species", "fox"}, {"count", "2"}},
         {{"tick", "2"}, {"command", "spawn"}, {"species", "hare"}, {"count", "3"}}});

    std::vector<std::map<std::string, std::string>> instances = {
        {{"type", "simulation_world"}, {"enable", "true"}},
        {{"type", "scenario"}, {"enable", "true"}},
    };
    if (with_recorder) {
        instances.push_back({{"type", "recorder"}, {"id", "csv"}, {"enable", "true"}, {"sink", "memory"}});
    }

    auto config = writeAppConfigFile(with_recorder ? "app_test_5_with_rec.toml" : "app_test_5_without_rec.toml", scenario, 10,
                                     instances);
    if (!app.initialize(config.string()) || !app.startModules()) {
        return {};
    }

    app.runHeadless();
    auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
    if (!world) {
        return {};
    }

    auto state = world->readModel();
    return {state.tick, state.energy_total, state.population_by_species.size(), world->checksum()};
}
} // namespace

class RecorderIsolationTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.5 recorder isolation";
        auto without = runScenario(false);
        auto with = runScenario(true);

        if (without.tick == 0 || with.tick == 0) {
            return {name, false, "один из прогонов не выполнился корректно"};
        }
        if (without.tick != with.tick || without.energy != with.energy || without.species_count != with.species_count ||
            without.checksum != with.checksum) {
            return {name, false, "подключение recorder изменило итоговое состояние мира"};
        }

        return {name, true, "с recorder и без recorder итоговые состояния идентичны"};
    }
};

std::unique_ptr<IIntegrationTest> makeRecorderIsolationTest() {
    return std::make_unique<RecorderIsolationTest>();
}

} // namespace ecosim_integration
