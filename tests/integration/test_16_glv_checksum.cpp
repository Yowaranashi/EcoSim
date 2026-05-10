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
        {"fox", 2.0, {{"growth", -0.3}}},
    };
    config.interaction_matrix = {
        {-0.02, -0.1},
        {0.04, -0.01},
    };
    return config;
}
} // namespace

class GlvChecksumTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.16 gLV checksum";
        ecosim::GlvDynamics first;
        ecosim::GlvDynamics second;
        first.configure(makeConfig());
        second.configure(makeConfig());

        const auto initial_checksum = first.checksum();
        if (initial_checksum.empty() || initial_checksum != second.checksum()) {
            return {name, false, "identical gLV states do not produce the same checksum"};
        }

        first.applyShock("rabbit", 1.0);
        if (first.checksum() == initial_checksum) {
            return {name, false, "state change did not change gLV checksum"};
        }

        return {name, true, "gLV checksum is deterministic and state-sensitive"};
    }
};

std::unique_ptr<IIntegrationTest> makeGlvChecksumTest() {
    return std::make_unique<GlvChecksumTest>();
}

} // namespace ecosim_integration
