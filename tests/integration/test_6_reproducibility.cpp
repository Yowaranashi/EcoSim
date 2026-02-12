#include "integration/test_framework.h"

#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

namespace {
struct Snapshot {
    int tick = 0;
    int energy = 0;
    int boar = 0;
    int deer = 0;
    std::string checksum;
};

Snapshot runOnce(const std::string &suffix) {
    std::ostringstream log_stream;
    ecosim::Logger logger(log_stream);
    ecosim::Application app(logger);

    auto scenario = writeScenarioFile(
        "scenario_test_6_" + suffix + ".toml", 23, 4, {"simulation_world"},
        {{{"tick", "1"}, {"command", "spawn"}, {"species", "boar"}, {"count", "4"}},
         {{"tick", "2"}, {"command", "spawn"}, {"species", "deer"}, {"count", "1"}}});
    auto config = writeAppConfigFile(
        "app_test_6_" + suffix + ".toml", scenario, 10,
        {{{"type", "simulation_world"}, {"enable", "true"}}, {{"type", "scenario"}, {"enable", "true"}}});

    if (!app.initialize(config.string()) || !app.startModules()) {
        return {};
    }

    app.runHeadless();
    auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
    if (!world) {
        return {};
    }

    auto state = world->readModel();
    return {state.tick,
            state.energy_total,
            state.population_by_species.count("boar") ? state.population_by_species.at("boar") : 0,
            state.population_by_species.count("deer") ? state.population_by_species.at("deer") : 0,
            world->checksum()};
}
} // namespace

class ReproducibilityTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.6 reproducibility";
        auto first = runOnce("first");
        auto second = runOnce("second");

        if (first.tick == 0 || second.tick == 0) {
            return {name, false, "один из запусков не выполнился"};
        }
        if (first.tick != second.tick || first.energy != second.energy || first.boar != second.boar ||
            first.deer != second.deer || first.checksum != second.checksum) {
            return {name, false, "результаты двух запусков с одинаковым seed не совпали"};
        }

        return {name, true, "детерминированность подтверждена: все агрегаты и checksum совпали"};
    }
};

std::unique_ptr<IIntegrationTest> makeReproducibilityTest() {
    return std::make_unique<ReproducibilityTest>();
}

} // namespace ecosim_integration
