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

class GlvJacobianTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.13 gLV jacobian";
        ecosim::GlvDynamics model;
        model.configure(makeConfig());

        const auto jacobian = model.jacobian();
        if (jacobian.size() != 2 || jacobian[0].size() != 2 || jacobian[1].size() != 2 ||
            std::abs(jacobian[0][0] - 0.0) > 1e-12 ||
            std::abs(jacobian[0][1] + 1.0) > 1e-12 ||
            std::abs(jacobian[1][0] - 0.08) > 1e-12 ||
            std::abs(jacobian[1][1] - 0.06) > 1e-12) {
            return {name, false, "gLV Jacobian does not match the manual matrix"};
        }

        ecosim::ModelConfig equilibrium_config;
        equilibrium_config.model_id = "glv";
        equilibrium_config.species = {
            {"rabbit", 1.0, {{"growth", 1.0}}},
            {"fox", 1.0, {{"growth", 2.0}}},
        };
        equilibrium_config.interaction_matrix = {
            {-0.5, 0.0},
            {0.0, -0.5},
        };
        ecosim::GlvDynamics equilibrium_model;
        equilibrium_model.configure(equilibrium_config);
        const auto equilibrium = equilibrium_model.equilibriumCandidate();
        if (!equilibrium || equilibrium->size() != 2 ||
            std::abs((*equilibrium)[0] - 2.0) > 1e-12 ||
            std::abs((*equilibrium)[1] - 4.0) > 1e-12) {
            return {name, false, "gLV equilibrium candidate does not solve r + A*N = 0"};
        }

        return {name, true, "gLV Jacobian matches the manual matrix"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvJacobianTest() {
    return std::make_unique<GlvJacobianTest>();
}

} // namespace ecosim_integration
