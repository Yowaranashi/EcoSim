#include "integration/test_framework.h"

#include "models/glv_dynamics.h"
#include "models/model_dynamics_factory.h"
#include "models/rosenzweig_macarthur_dynamics.h"

#include <memory>
#include <stdexcept>

namespace ecosim_integration {

namespace {
template <typename T>
bool createsModelType(const std::string &model_id) {
    auto model = ecosim::createModelDynamics(model_id);
    return model && dynamic_cast<T *>(model.get()) != nullptr;
}
} // namespace

class ModelDynamicsFactoryTest : public IIntegrationTest {
public:
    TestResult run() override {
        const std::string name = "5.4.25 model dynamics factory";

        if (!ecosim::isSupportedModelId("glv") || !createsModelType<ecosim::GlvDynamics>("glv")) {
            return {name, false, "factory did not create GlvDynamics for glv"};
        }
        if (!ecosim::isSupportedModelId("generalized_lotka_volterra") ||
            !createsModelType<ecosim::GlvDynamics>("generalized_lotka_volterra")) {
            return {name, false, "factory did not create GlvDynamics for generalized_lotka_volterra"};
        }
        if (!ecosim::isSupportedModelId("rosenzweig_macarthur") ||
            !createsModelType<ecosim::RosenzweigMacArthurDynamics>("rosenzweig_macarthur")) {
            return {name, false, "factory did not create RosenzweigMacArthurDynamics for rosenzweig_macarthur"};
        }
        if (!ecosim::isSupportedModelId("rm") ||
            !createsModelType<ecosim::RosenzweigMacArthurDynamics>("rm")) {
            return {name, false, "factory did not create RosenzweigMacArthurDynamics for rm"};
        }

        try {
            (void)ecosim::createModelDynamics("unknown_model");
            return {name, false, "unknown model_id did not throw"};
        } catch (const std::invalid_argument &) {
        }

        if (ecosim::isSupportedModelId("unknown_model")) {
            return {name, false, "unknown model_id is reported as supported"};
        }

        return {name, true, "model dynamics factory creates all supported models and rejects unknown ids"};
    }
};

std::unique_ptr<IIntegrationTest> makeModelDynamicsFactoryTest() {
    return std::make_unique<ModelDynamicsFactoryTest>();
}

} // namespace ecosim_integration
