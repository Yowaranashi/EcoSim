#include "models/model_dynamics_factory.h"

#include "models/glv_dynamics.h"
#include "models/rosenzweig_macarthur_dynamics.h"

#include <stdexcept>

namespace ecosim {

std::unique_ptr<IModelDynamics> createModelDynamics(const std::string &model_id) {
    if (model_dynamics_factory_detail::isGlvModelId(model_id)) {
        return std::make_unique<GlvDynamics>();
    }
    if (model_dynamics_factory_detail::isRosenzweigMacArthurModelId(model_id)) {
        return std::make_unique<RosenzweigMacArthurDynamics>();
    }

    throw std::invalid_argument("Unsupported model_id for model dynamics factory: " + model_id);
}

} // namespace ecosim
