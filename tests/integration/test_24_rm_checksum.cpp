#include "integration/test_framework.h"

#include "models/rosenzweig_macarthur_dynamics.h"

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

class RmChecksumTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.24 RM checksum";
        ecosim::RosenzweigMacArthurDynamics first;
        ecosim::RosenzweigMacArthurDynamics second;
        first.configure(makeConfig());
        second.configure(makeConfig());

        const auto initial_checksum = first.checksum();
        if (initial_checksum.empty() || initial_checksum != second.checksum()) {
            return {name, false, "identical RM states do not produce the same checksum"};
        }

        first.applyShock("prey", 1.0);
        if (first.checksum() == initial_checksum) {
            return {name, false, "state change did not change RM checksum"};
        }

        second.setParameter("m", 0.3);
        if (second.checksum() == initial_checksum) {
            return {name, false, "parameter change did not change RM checksum"};
        }

        return {name, true, "RM checksum is deterministic and sensitive to state and parameters"};
    }
};

std::unique_ptr<IIntegrationTest> makeRmChecksumTest() {
    return std::make_unique<RmChecksumTest>();
}

} // namespace ecosim_integration
