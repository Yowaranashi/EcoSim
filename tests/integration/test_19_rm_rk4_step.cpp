#include "integration/test_framework.h"

#include "models/rosenzweig_macarthur_dynamics.h"

#include <cmath>
#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeConfig() {
    ecosim::ModelConfig config;
    config.model_id = "rosenzweig_macarthur";
    config.initial_state = {{"prey", 10.0}, {"predator", 5.0}};
    config.parameters = {{"r", 1.0}, {"K", 100.0}, {"a", 0.1}, {"h", 0.2}, {"e", 0.5}, {"m", 0.2}};
    return config;
}
} // namespace

class RmRk4StepTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.19 RM RK4 step";
        ecosim::RosenzweigMacArthurDynamics model;
        model.configure(makeConfig());
        model.advanceStep(0.1, ecosim::IntegrationMethod::RK4);

        const auto &state = model.getStateVector();
        if (state.size() != 2 || std::abs(state[0] - 10.489544656228627) > 1e-12 ||
            std::abs(state[1] - 5.1138185229929665) > 1e-12) {
            return {name, false, "one RK4 step does not match reference RM update"};
        }

        return {name, true, "one RK4 step matches reference RM update"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmRk4StepTest() {
    return std::make_unique<RmRk4StepTest>();
}

} // namespace ecosim_integration
