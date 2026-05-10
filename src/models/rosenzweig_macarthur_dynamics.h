#pragma once

#include "models/model_dynamics_base.h"

#include <array>
#include <optional>

namespace ecosim {

class RosenzweigMacArthurDynamics : public ModelDynamicsBase {
public:
    void configure(const ModelConfig &config) override;
    std::vector<double> computeDerivatives(const std::vector<double> &state, double time) const override;
    void setParameter(const std::string &name, double value) override;
    void applyShock(const std::string &target, double strength) override;
    std::map<std::string, double> getMetrics() const override;
    std::string checksum() const override;

    std::array<std::array<double, 2>, 2> jacobian2x2() const;
    std::optional<std::vector<double>> equilibriumCandidate() const;
    const std::vector<std::string> &getFlags() const { return flags_; }

private:
    void applyKnownParameter(const std::string &name, double value);
    void validateParameters() const;
    bool matchesPrey(const std::string &target) const;
    bool matchesPredator(const std::string &target) const;

    double r_ = 1.0;
    double K_ = 100.0;
    double a_ = 0.1;
    double h_ = 0.2;
    double e_ = 0.5;
    double m_ = 0.2;
    std::vector<std::string> flags_;
};

} // namespace ecosim
