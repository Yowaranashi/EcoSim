#include "integration/test_framework.h"

#include "models/glv_dynamics.h"

#include <cmath>
#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeConfig() {
    ecosim::ModelConfig config;
    config.model_id = "glv";
    config.species = {
        {"rabbit", 10.0, {{"growth", 0.5}, {"sensitivity", 0.5}, {"external_input", 0.2}}},
        {"fox", 2.0, {{"growth", -0.3}}},
    };
    config.interaction_matrix = {
        {-0.02, -0.1},
        {0.04, -0.01},
    };
    return config;
}
} // namespace

class GlvEulerStepTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.11 gLV Euler step";
        ecosim::GlvDynamics model;
        model.configure(makeConfig());
        model.advanceStep(0.1, ecosim::IntegrationMethod::Euler);

        const auto &state = model.getStateVector();
        if (state.size() != 2 || std::abs(state[0] - 10.2) > 1e-12 ||
            std::abs(state[1] - 2.016) > 1e-12) {
            return {name, false, "one Euler step does not match manual gLV update"};
        }

        return {name, true, "one Euler step matches manual gLV update"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvEulerStepTest() {
    return std::make_unique<GlvEulerStepTest>();
}

} // namespace ecosim_integration
