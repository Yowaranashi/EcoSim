#include "integration/test_framework.h"

#include <memory>

namespace ecosim_integration {

class DependencyResolutionTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.1 dependency resolution";
        std::ostringstream log_stream;
        ecosim::Logger logger(log_stream);
        ecosim::Application app(logger);

        auto scenario = writeScenarioFile("scenario_test_1.toml", 7, 5, {"simulation_world"}, {});
        auto config = writeAppConfigFile(
            "app_test_1.toml", scenario, 2,
            {{{"type", "simulation_world"}, {"enable", "false"}}, {{"type", "scenario"}, {"enable", "true"}}});

        if (!app.initialize(config.string())) {
            return {name, false, "initialize() завершился ошибкой"};
        }

        bool started = app.startModules();
        auto order = app.moduleManager().startOrder();
        auto logs = log_stream.str();

        if (started) {
            return {name, false, "startModules() должен прерваться при недоступной зависимости"};
        }
        if (!order.empty()) {
            return {name, false, "startOrder должен быть пустым при провале запуска"};
        }
        if (!containsText(logs, "Missing dependency simulation_world for module type scenario")) {
            return {name, false, "в логе отсутствует сообщение о недостающей зависимости"};
        }

        return {name, true, "запуск прерван, сценарий не стартовал, tick-цикл не начат"};
    }
};

std::unique_ptr<IIntegrationTest> makeDependencyResolutionTest() {
    return std::make_unique<DependencyResolutionTest>();
}

} // namespace ecosim_integration
