#include "integration/test_framework.h"

#include "models/glv_dynamics.h"

#include <algorithm>
#include <memory>

namespace ecosim_integration {

namespace {
ecosim::ModelConfig makeSingleSpeciesConfig(double initial,
                                            double growth,
                                            double threshold = 0.1,
                                            double noise = 0.0,
                                            int seed = 0) {
    ecosim::ModelConfig config;
    config.model_id = "glv";
    config.seed = seed;
    config.species = {{"rabbit", initial, {{"growth", growth}}}};
    config.interaction_matrix = {{0.0}};
    config.parameters["extinction_threshold"] = threshold;
    config.parameters["environmental_noise"] = noise;
    return config;
}

bool hasFlag(const std::vector<std::string> &flags, const std::string &flag) {
    return std::find(flags.begin(), flags.end(), flag) != flags.end();
}

std::string runNoisyChecksum(int seed) {
    ecosim::GlvDynamics model;
    model.configure(makeSingleSpeciesConfig(10.0, 0.1, 0.1, 0.05, seed));
    for (int i = 0; i < 6; ++i) {
        model.advanceStep(0.1, ecosim::IntegrationMethod::Euler);
    }
    return model.checksum();
}
} // namespace

class GlvExtinctionThresholdTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.48 gLV extinction threshold";

        ecosim::GlvDynamics shock_model;
        shock_model.configure(makeSingleSpeciesConfig(10.0, 0.0));
        shock_model.applyShock("rabbit", -20.0);
        if (shock_model.getStateVector()[0] != 0.0 ||
            !hasFlag(shock_model.getFlags(), "extinction.rabbit")) {
            return {name, false, "strong negative shock did not extinct the species"};
        }

        ecosim::GlvDynamics threshold_model;
        threshold_model.configure(makeSingleSpeciesConfig(0.11, -3.0));
        threshold_model.advanceStep(0.1, ecosim::IntegrationMethod::Euler);
        if (threshold_model.getStateVector()[0] != 0.0) {
            return {name, false, "population below extinction_threshold was not zeroed after tick"};
        }

        ecosim::GlvDynamics extinct_model;
        extinct_model.configure(makeSingleSpeciesConfig(0.0, 10.0));
        extinct_model.advanceStep(1.0, ecosim::IntegrationMethod::Euler);
        if (extinct_model.getStateVector()[0] != 0.0) {
            return {name, false, "extinct species resurrected without explicit spawn/apply_shock"};
        }

        ecosim::GlvDynamics disabled_threshold_model;
        disabled_threshold_model.configure(makeSingleSpeciesConfig(0.00001, 0.0, 0.0));
        disabled_threshold_model.advanceStep(0.1, ecosim::IntegrationMethod::Euler);
        if (disabled_threshold_model.getStateVector()[0] <= 0.0) {
            return {name, false, "extinction_threshold=0.0 did not preserve old clamp-only behavior"};
        }

        return {name, true, "gLV extinction threshold prevents zombie populations"};
    }
};

class EnvironmentalNoiseTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.49 environmental_noise reproducibility";

        ecosim::GlvDynamics deterministic_a;
        ecosim::GlvDynamics deterministic_b;
        deterministic_a.configure(makeSingleSpeciesConfig(10.0, 0.1, 0.1, 0.0, 1));
        deterministic_b.configure(makeSingleSpeciesConfig(10.0, 0.1, 0.1, 0.0, 999));
        for (int i = 0; i < 4; ++i) {
            deterministic_a.advanceStep(0.1, ecosim::IntegrationMethod::Euler);
            deterministic_b.advanceStep(0.1, ecosim::IntegrationMethod::Euler);
        }
        if (deterministic_a.getStateVector()[0] != deterministic_b.getStateVector()[0]) {
            return {name, false, "environmental_noise=0.0 should ignore seed and remain deterministic"};
        }

        const auto same_seed_a = runNoisyChecksum(123);
        const auto same_seed_b = runNoisyChecksum(123);
        const auto different_seed = runNoisyChecksum(124);
        if (same_seed_a.empty() || same_seed_a != same_seed_b || same_seed_a == different_seed) {
            return {name, false, "noise is not reproducible by seed or does not vary by seed"};
        }

        ecosim::GlvDynamics nonnegative;
        nonnegative.configure(makeSingleSpeciesConfig(0.2, -1.0, 0.1, 0.5, 42));
        for (int i = 0; i < 10; ++i) {
            nonnegative.advanceStep(0.1, ecosim::IntegrationMethod::Euler);
            if (nonnegative.getStateVector()[0] < 0.0) {
                return {name, false, "noise produced a negative post-clamp population"};
            }
        }
        return {name, true, "environmental_noise is deterministic by seed and post-clamped"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvExtinctionThresholdTest() {
    return std::make_unique<GlvExtinctionThresholdTest>();
}

std::unique_ptr<IIntegrationTest> makeEnvironmentalNoiseTest() {
    return std::make_unique<EnvironmentalNoiseTest>();
}

} // namespace ecosim_integration
