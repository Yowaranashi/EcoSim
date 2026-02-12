#include "integration/test_framework.h"

#include "modules/recorder_csv.h"
#include "modules/simulation_world.h"

#include <memory>

namespace ecosim_integration {

class EventBufferingTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.3 event buffering";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);

        auto scenario = writeScenarioFile("scenario_test_3.toml", 3, 1, {"simulation_world"}, {});
        auto config = writeAppConfigFile("app_test_3.toml", scenario, 1,
                                         {{{"type", "simulation_world"}, {"enable", "true"}},
                                          {{"type", "recorder"}, {"id", "csv"}, {"enable", "true"}, {"sink", "memory"}}});

        if (!app.initialize(config.string()) || !app.startModules()) {
            return {name, false, "не удалось инициализировать/запустить модули"};
        }

        auto *world = dynamic_cast<ecosim::SimulationWorld *>(app.moduleManager().findModule("simulation_world"));
        auto *recorder = dynamic_cast<ecosim::RecorderCsv *>(app.moduleManager().findModule("recorder", "csv"));
        if (!world || !recorder) {
            return {name, false, "не найдены simulation_world/recorder"};
        }

        for (auto *module : app.moduleManager().modules()) {
            module->onPreTick();
        }
        for (auto *module : app.moduleManager().modules()) {
            module->onTick();
        }
        for (auto *module : app.moduleManager().modules()) {
            module->onPostTick();
        }

        if (app.eventBus().bufferedCount() == 0) {
            return {name, false, "событие world.tick должно быть в буфере до deliverBuffered()"};
        }
        if (!recorder->events().empty()) {
            return {name, false, "до deliverBuffered() recorder не должен получать события"};
        }

        app.eventBus().deliverBuffered();
        for (auto *module : app.moduleManager().modules()) {
            module->onDeliverBufferedEvents();
        }

        if (recorder->events().size() != 1 || recorder->events().front().tick != world->readModel().tick) {
            return {name, false, "после deliverBuffered() recorder должен получить 1 событие текущего тика"};
        }

        return {name, true, "emit() буферизует событие, доставка происходит только в фазе deliverBuffered()"};
    }
};

std::unique_ptr<IIntegrationTest> makeEventBufferingTest() {
    return std::make_unique<EventBufferingTest>();
}

} // namespace ecosim_integration
