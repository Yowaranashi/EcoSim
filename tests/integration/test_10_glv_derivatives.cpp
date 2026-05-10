#include "integration/test_framework.h"

#include "models/glv_dynamics.h"

#include <cmath>
#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeTwoSpeciesConfig() {
    ecosim::ModelConfig config;
    config.model_id = "glv";

    ecosim::SpeciesConfig rabbit;
    rabbit.id = "rabbit";
    rabbit.initial_state = 10.0;
    rabbit.parameters["growth"] = 0.5;
    rabbit.parameters["sensitivity"] = 0.5;
    rabbit.parameters["external_input"] = 0.2;

    ecosim::SpeciesConfig fox;
    fox.id = "fox";
    fox.initial_state = 2.0;
    fox.parameters["growth"] = -0.3;

    config.species = {rabbit, fox};
    config.interaction_matrix = {
        {-0.02, -0.1},
        {0.04, -0.01},
    };
    return config;
}
} // namespace

class GlvDerivativesTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.10 gLV derivatives";
        ecosim::GlvDynamics model;
        model.configure(makeTwoSpeciesConfig());

        const auto derivatives = model.computeDerivatives(model.getStateVector(), 0.0);
        if (derivatives.size() != 2 || std::abs(derivatives[0] - 2.0) > 1e-12 ||
            std::abs(derivatives[1] - 0.16) > 1e-12) {
            return {name, false, "gLV derivatives do not match the manual two-species calculation"};
        }

        return {name, true, "gLV derivatives match the manual calculation"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvDerivativesTest() {
    return std::make_unique<GlvDerivativesTest>();
}

} // namespace ecosim_integration
