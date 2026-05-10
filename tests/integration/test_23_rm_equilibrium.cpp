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

class RmEquilibriumTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.23 RM equilibrium";
        ecosim::RosenzweigMacArthurDynamics model;
        model.configure(makeConfig());

        const auto equilibrium = model.equilibriumCandidate();
        if (!equilibrium || equilibrium->size() != 2 ||
            std::abs((*equilibrium)[0] - 4.3478260869565224) > 1e-12 ||
            std::abs((*equilibrium)[1] - 10.396975425330812) > 1e-12) {
            return {name, false, "RM positive equilibrium candidate is incorrect"};
        }

        auto derivatives = model.computeDerivatives(*equilibrium, 0.0);
        if (std::abs(derivatives[0]) > 1e-12 || std::abs(derivatives[1]) > 1e-12) {
            return {name, false, "RM equilibrium candidate does not zero the derivatives"};
        }

        model.setParameter("e", 0.01);
        if (model.equilibriumCandidate()) {
            return {name, false, "RM equilibrium should be absent when e - m*h is non-positive"};
        }

        return {name, true, "RM positive equilibrium candidate is valid when it exists"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmEquilibriumTest() {
    return std::make_unique<RmEquilibriumTest>();
}

} // namespace ecosim_integration
