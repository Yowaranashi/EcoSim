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

class RmEulerStepTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.18 RM Euler step";
        ecosim::RosenzweigMacArthurDynamics model;
        model.configure(makeConfig());
        model.advanceStep(0.1, ecosim::IntegrationMethod::Euler);

        const auto &state = model.getStateVector();
        if (state.size() != 2 || std::abs(state[0] - 10.483333333333334) > 1e-12 ||
            std::abs(state[1] - 5.108333333333333) > 1e-12) {
            return {name, false, "one Euler step does not match manual RM update"};
        }

        return {name, true, "one Euler step matches manual RM update"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmEulerStepTest() {
    return std::make_unique<RmEulerStepTest>();
}

} // namespace ecosim_integration
