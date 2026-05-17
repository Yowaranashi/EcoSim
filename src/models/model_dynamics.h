#pragma once

#include "core/scenario.h"
#include "models/integration_method.h"

#include <map>
#include <string>
#include <vector>

namespace ecosim {

class IModelDynamics {
public:
    virtual ~IModelDynamics() = default;

    virtual std::string modelId() const = 0;

    virtual void configure(const ModelConfig &config) = 0;

    virtual std::vector<double> computeDerivatives(
        const std::vector<double> &state,
        double time
    ) const = 0;

    virtual void advanceStep(double dt, IntegrationMethod method) = 0;

    virtual void setParameter(const std::string &name, double value) = 0;

    virtual void applyShock(
        const std::string &target,
        double strength
    ) = 0;

    virtual const std::vector<double> &getStateVector() const = 0;

    virtual std::vector<std::string> getSpeciesNames() const = 0;

    virtual std::map<std::string, double> getMetrics() const = 0;

    virtual std::vector<std::string> getFlags() const { return {}; }

    virtual std::string checksum() const = 0;
};

} // namespace ecosim
