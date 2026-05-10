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

class GlvSetParamTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.14 gLV set_param";
        ecosim::GlvDynamics model;
        model.configure(makeConfig());

        const double before = model.computeDerivatives(model.getStateVector(), 0.0)[0];
        model.setParameter("growth.rabbit", 0.7);
        const double after = model.computeDerivatives(model.getStateVector(), 0.0)[0];
        if (std::abs(before - 2.0) > 1e-12 || std::abs(after - 4.0) > 1e-12) {
            return {name, false, "growth.rabbit did not update the rabbit derivative"};
        }

        return {name, true, "growth.rabbit changes the rabbit derivative"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvSetParamTest() {
    return std::make_unique<GlvSetParamTest>();
}

} // namespace ecosim_integration
