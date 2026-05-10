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

class RmSetParamTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.20 RM set_param";
        ecosim::RosenzweigMacArthurDynamics model;
        model.configure(makeConfig());

        model.setParameter("a", 0.2);
        auto derivatives = model.computeDerivatives(model.getStateVector(), 0.0);
        if (std::abs(derivatives[0] - 1.8571428571428568) > 1e-12 ||
            std::abs(derivatives[1] - 2.5714285714285716) > 1e-12) {
            return {name, false, "a did not update the RM functional response"};
        }

        model.setParameter("predator.m", 0.4);
        derivatives = model.computeDerivatives(model.getStateVector(), 0.0);
        if (std::abs(derivatives[1] - 1.5714285714285716) > 1e-12) {
            return {name, false, "predator.m did not update predator mortality"};
        }

        return {name, true, "RM set_param updates simple and qualified parameter names"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmSetParamTest() {
    return std::make_unique<RmSetParamTest>();
}

} // namespace ecosim_integration
