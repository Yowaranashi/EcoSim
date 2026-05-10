#include "integration/test_framework.h"

#include "models/glv_dynamics.h"

#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeConfig() {
    ecosim::ModelConfig config;
    config.model_id = "glv";
    config.species = {
        {"rabbit", 10.0, {{"growth", 0.5}}},
    };
    config.interaction_matrix = {{-0.02}};
    return config;
}
} // namespace

class GlvApplyShockTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.15 gLV apply_shock";
        ecosim::GlvDynamics model;
        model.configure(makeConfig());

        model.applyShock("rabbit", -3.0);
        if (model.getStateVector().at(0) != 7.0) {
            return {name, false, "negative shock did not decrease rabbit population"};
        }
        model.applyShock("rabbit", -100.0);
        if (model.getStateVector().at(0) != 0.0) {
            return {name, false, "large negative shock was not clamped to zero"};
        }
        model.applyShock("rabbit", 5.0);
        if (model.getStateVector().at(0) != 5.0 || model.getFlags().empty()) {
            return {name, false, "positive shock or shock flag was not applied"};
        }

        return {name, true, "gLV shocks update and clamp species state"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvApplyShockTest() {
    return std::make_unique<GlvApplyShockTest>();
}

} // namespace ecosim_integration
