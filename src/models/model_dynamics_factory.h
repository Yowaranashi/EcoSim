#pragma once

#include "models/model_dynamics.h"

#include <memory>
#include <string>

namespace ecosim {

namespace model_dynamics_factory_detail {
inline bool isGlvModelId(const std::string &model_id) {
    return model_id == "glv" || model_id == "generalized_lotka_volterra";
}

inline bool isRosenzweigMacArthurModelId(const std::string &model_id) {
    return model_id == "rosenzweig_macarthur" || model_id == "rm";
}
} // namespace model_dynamics_factory_detail

inline bool isSupportedModelId(const std::string &model_id) {
    return model_dynamics_factory_detail::isGlvModelId(model_id) ||
           model_dynamics_factory_detail::isRosenzweigMacArthurModelId(model_id);
}

std::unique_ptr<IModelDynamics> createModelDynamics(const std::string &model_id);

} // namespace ecosim
