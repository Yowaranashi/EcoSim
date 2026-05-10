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

class RmJacobianTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.22 RM jacobian";
        ecosim::RosenzweigMacArthurDynamics model;
        model.configure(makeConfig());

        const auto jacobian = model.jacobian2x2();
        if (std::abs(jacobian[0][0] - 0.45277777777777783) > 1e-12 ||
            std::abs(jacobian[0][1] + 0.8333333333333334) > 1e-12 ||
            std::abs(jacobian[1][0] - 0.17361111111111113) > 1e-12 ||
            std::abs(jacobian[1][1] - 0.21666666666666673) > 1e-12) {
            return {name, false, "RM Jacobian does not match the analytical matrix"};
        }

        return {name, true, "RM Jacobian matches the analytical matrix"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmJacobianTest() {
    return std::make_unique<RmJacobianTest>();
}

} // namespace ecosim_integration
