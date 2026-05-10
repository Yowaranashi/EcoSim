#include "integration/test_framework.h"

#include "models/model_dynamics_base.h"

#include <cmath>
#include <memory>

namespace ecosim_integration {

namespace {
class ExponentialTestModel : public ecosim::ModelDynamicsBase {
public:
    std::vector<double> computeDerivatives(const std::vector<double> &state, double /*time*/) const override {
        return state;
    }
};

ecosim::ModelConfig makeConfig(double initial_state) {
    ecosim::ModelConfig config;
    config.model_id = "test.exponential";
    ecosim::SpeciesConfig species;
    species.id = "x";
    species.initial_state = initial_state;
    config.species.push_back(species);
    config.parameters["rate"] = 1.0;
    return config;
}
} // namespace

class ModelDynamicsTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.9 model dynamics";
        const double expected = std::exp(0.1);

        ExponentialTestModel euler_model;
        euler_model.configure(makeConfig(1.0));
        euler_model.advanceStep(0.1, ecosim::IntegrationMethod::Euler);
        const double euler_value = euler_model.getStateVector().at(0);
        if (std::abs(euler_value - 1.1) > 1e-12) {
            return {name, false, "Euler step for dx/dt=x did not produce x=1.1"};
        }

        ExponentialTestModel rk4_model;
        rk4_model.configure(makeConfig(1.0));
        rk4_model.advanceStep(0.1, ecosim::IntegrationMethod::RK4);
        const double rk4_value = rk4_model.getStateVector().at(0);
        if (std::abs(rk4_value - expected) > 1e-6) {
            return {name, false, "RK4 step for dx/dt=x is not close to exp(0.1)"};
        }
        if (std::abs(rk4_value - expected) >= std::abs(euler_value - expected)) {
            return {name, false, "RK4 is not more accurate than Euler for dx/dt=x"};
        }

        ExponentialTestModel clamp_model;
        clamp_model.configure(makeConfig(1.0));
        clamp_model.applyShock("x", -100.0);
        if (clamp_model.getStateVector().at(0) < 0.0) {
            return {name, false, "clamp left a negative population in the state vector"};
        }

        ExponentialTestModel checksum_a;
        ExponentialTestModel checksum_b;
        checksum_a.configure(makeConfig(1.0));
        checksum_b.configure(makeConfig(1.0));
        checksum_a.advanceStep(0.1, ecosim::IntegrationMethod::RK4);
        checksum_b.advanceStep(0.1, ecosim::IntegrationMethod::RK4);
        if (checksum_a.checksum().empty() || checksum_a.checksum() != checksum_b.checksum()) {
            return {name, false, "checksum is not deterministic for identical model state"};
        }

        auto metrics = checksum_a.getMetrics();
        if (metrics.at("species_count") != 1.0 || metrics.at("total_population") <= 0.0) {
            return {name, false, "basic model metrics are missing"};
        }

        return {name, true, "Euler, RK4, clamp and deterministic checksum passed"};
    }
};

std::unique_ptr<IIntegrationTest> makeModelDynamicsTest() {
    return std::make_unique<ModelDynamicsTest>();
}

} // namespace ecosim_integration
