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

class GlvRk4StepTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.12 gLV RK4 step";
        ecosim::GlvDynamics model;
        model.configure(makeConfig());
        model.advanceStep(0.1, ecosim::IntegrationMethod::RK4);

        for (double value : model.getStateVector()) {
            if (!std::isfinite(value) || value <= 0.0) {
                return {name, false, "RK4 step produced a non-finite or non-positive population"};
            }
        }

        return {name, true, "RK4 step produced finite positive gLV state values"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvRk4StepTest() {
    return std::make_unique<GlvRk4StepTest>();
}

} // namespace ecosim_integration
