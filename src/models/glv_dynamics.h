#pragma once

#include "models/model_dynamics_base.h"

#include <optional>

namespace ecosim {

class GlvDynamics : public ModelDynamicsBase {
public:
    void configure(const ModelConfig &config) override;
    std::vector<double> computeDerivatives(const std::vector<double> &state, double time) const override;
    void setParameter(const std::string &name, double value) override;
    void applyShock(const std::string &target, double strength) override;
    std::map<std::string, double> getMetrics() const override;
    std::vector<std::string> getFlags() const override { return flags_; }
    std::string checksum() const override;

    std::vector<std::vector<double>> jacobian() const;
    std::optional<std::vector<double>> equilibriumCandidate() const;

private:
    std::optional<std::size_t> speciesIndex(const std::string &name) const;
    void applyKnownParameter(const std::string &name, double value);
    void resizeParameters(std::size_t species_count);
    void afterStateClamped(const std::vector<double> &previous_state,
                           const std::vector<double> &next_state) override;
    void markExtinctions(const std::vector<double> &previous_state,
                         const std::vector<double> &next_state);
    void pushFlagOnce(const std::string &flag);

    std::vector<double> growth_;
    std::vector<std::vector<double>> interaction_matrix_;
    std::vector<double> sensitivity_;
    std::vector<double> external_input_;
    std::vector<std::string> flags_;
};

} // namespace ecosim
