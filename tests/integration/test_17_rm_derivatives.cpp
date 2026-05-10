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

class RmDerivativesTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.17 RM derivatives";
        ecosim::RosenzweigMacArthurDynamics model;
        model.configure(makeConfig());

        const auto derivatives = model.computeDerivatives(model.getStateVector(), 0.0);
        if (derivatives.size() != 2 || std::abs(derivatives[0] - 4.833333333333333) > 1e-12 ||
            std::abs(derivatives[1] - 1.0833333333333335) > 1e-12) {
            return {name, false, "RM derivatives do not match the manual calculation"};
        }

        const auto species_names = model.getSpeciesNames();
        if (species_names.size() != 2 || species_names[0] != "prey" || species_names[1] != "predator") {
            return {name, false, "RM default species names are not prey/predator"};
        }

        const auto metrics = model.getMetrics();
        if (std::abs(metrics.at("prey") - 10.0) > 1e-12 || std::abs(metrics.at("predator") - 5.0) > 1e-12 ||
            std::abs(metrics.at("biomass_total") - 15.0) > 1e-12 ||
            std::abs(metrics.at("phase_x") - 10.0) > 1e-12 || std::abs(metrics.at("phase_y") - 5.0) > 1e-12) {
            return {name, false, "RM metrics do not expose prey/predator/phase values"};
        }

        return {name, true, "RM derivatives match the manual calculation"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmDerivativesTest() {
    return std::make_unique<RmDerivativesTest>();
}

} // namespace ecosim_integration
