#pragma once

#include "models/model_dynamics.h"

#include <map>
#include <random>
#include <string>
#include <vector>

namespace ecosim {

class ModelDynamicsBase : public IModelDynamics {
public:
    std::string modelId() const override;
    void configure(const ModelConfig &config) override;
    void advanceStep(double dt, IntegrationMethod method) override;
    void setParameter(const std::string &name, double value) override;
    void applyShock(const std::string &target, double strength) override;
    const std::vector<double> &getStateVector() const override;
    std::vector<std::string> getSpeciesNames() const override;
    std::map<std::string, double> getMetrics() const override;
    std::vector<std::string> getFlags() const override;
    std::string checksum() const override;

protected:
    double clampPopulation(double value) const;
    std::vector<double> clampState(const std::vector<double> &state) const;
    virtual void afterStateClamped(const std::vector<double> &previous_state,
                                   const std::vector<double> &next_state);

    double currentTime() const { return time_; }
    const std::map<std::string, double> &parameters() const { return parameters_; }
    double extinctionThreshold() const { return extinction_threshold_; }
    double environmentalNoise() const { return environmental_noise_; }

    std::string model_id_;
    std::vector<std::string> species_names_;
    std::vector<double> state_;
    std::map<std::string, double> parameters_;
    double time_ = 0.0;
    int seed_ = 0;
    double extinction_threshold_ = 0.1;
    double environmental_noise_ = 0.0;
    std::mt19937 rng_;
};

} // namespace ecosim
