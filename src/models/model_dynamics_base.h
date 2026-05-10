#pragma once

#include "models/model_dynamics.h"

#include <map>
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
    std::string checksum() const override;

protected:
    static double clampPopulation(double value);
    static std::vector<double> clampState(const std::vector<double> &state);

    double currentTime() const { return time_; }
    const std::map<std::string, double> &parameters() const { return parameters_; }

    std::string model_id_;
    std::vector<std::string> species_names_;
    std::vector<double> state_;
    std::map<std::string, double> parameters_;
    double time_ = 0.0;
};

} // namespace ecosim
