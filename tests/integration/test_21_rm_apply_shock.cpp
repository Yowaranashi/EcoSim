#include "integration/test_framework.h"

#include "models/rosenzweig_macarthur_dynamics.h"

#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeConfig() {
    ecosim::ModelConfig config;
    config.model_id = "rosenzweig_macarthur";
    config.species = {
        {"resource", 10.0, {}},
        {"consumer", 5.0, {}},
    };
    config.parameters = {{"r", 1.0}, {"K", 100.0}, {"a", 0.1}, {"h", 0.2}, {"e", 0.5}, {"m", 0.2}};
    return config;
}
} // namespace

class RmApplyShockTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.21 RM apply_shock";
        ecosim::RosenzweigMacArthurDynamics model;
        model.configure(makeConfig());

        model.applyShock("prey", -3.0);
        if (model.getStateVector().at(0) != 7.0) {
            return {name, false, "prey shock did not decrease prey state"};
        }

        model.applyShock("consumer", -100.0);
        if (model.getStateVector().at(1) != 0.0) {
            return {name, false, "species-name predator shock was not clamped to zero"};
        }

        model.applyShock("predator", 4.0);
        if (model.getStateVector().at(1) != 4.0 || model.getFlags().size() < 3) {
            return {name, false, "predator shock or shock flags were not applied"};
        }

        return {name, true, "RM shocks update aliases, species names, and clamped state"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmApplyShockTest() {
    return std::make_unique<RmApplyShockTest>();
}

} // namespace ecosim_integration
