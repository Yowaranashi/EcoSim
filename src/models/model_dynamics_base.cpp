#include "models/model_dynamics_base.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace ecosim {

namespace {
constexpr double kClampEpsilon = 1e-12;

void ensureDerivativeSize(const std::vector<double> &state,
                          const std::vector<double> &derivatives) {
    if (state.size() != derivatives.size()) {
        throw std::runtime_error("model derivative vector size does not match state vector size");
    }
}

std::vector<double> addScaled(const std::vector<double> &state,
                              const std::vector<double> &derivatives,
                              double scale) {
    ensureDerivativeSize(state, derivatives);
    std::vector<double> result;
    result.reserve(state.size());
    for (std::size_t i = 0; i < state.size(); ++i) {
        result.push_back(state[i] + derivatives[i] * scale);
    }
    return result;
}

std::uint64_t fnv1a64(const std::string &text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}
} // namespace

std::string ModelDynamicsBase::modelId() const {
    return model_id_;
}

void ModelDynamicsBase::configure(const ModelConfig &config) {
    model_id_ = config.model_id;
    species_names_.clear();
    state_.clear();
    parameters_ = config.parameters;
    time_ = 0.0;

    for (const auto &species : config.species) {
        species_names_.push_back(species.id);

        double initial_value = species.initial_state;
        if (auto it = config.initial_state.find(species.id); it != config.initial_state.end()) {
            initial_value = it->second;
        }
        state_.push_back(clampPopulation(initial_value));

        for (const auto &param : species.parameters) {
            parameters_[species.id + "." + param.first] = param.second;
        }
    }

    if (species_names_.empty() && !config.initial_state.empty()) {
        for (const auto &entry : config.initial_state) {
            species_names_.push_back(entry.first);
            state_.push_back(clampPopulation(entry.second));
        }
    }
}

void ModelDynamicsBase::advanceStep(double dt, IntegrationMethod method) {
    if (dt < 0.0) {
        throw std::invalid_argument("dt must be non-negative");
    }

    if (state_.empty() || dt == 0.0) {
        return;
    }

    std::vector<double> next_state;
    if (method == IntegrationMethod::Euler) {
        auto derivatives = computeDerivatives(state_, time_);
        ensureDerivativeSize(state_, derivatives);
        next_state = addScaled(state_, derivatives, dt);
    } else {
        auto k1 = computeDerivatives(state_, time_);
        ensureDerivativeSize(state_, k1);
        auto k2_state = addScaled(state_, k1, dt * 0.5);
        auto k2 = computeDerivatives(k2_state, time_ + dt * 0.5);
        ensureDerivativeSize(state_, k2);
        auto k3_state = addScaled(state_, k2, dt * 0.5);
        auto k3 = computeDerivatives(k3_state, time_ + dt * 0.5);
        ensureDerivativeSize(state_, k3);
        auto k4_state = addScaled(state_, k3, dt);
        auto k4 = computeDerivatives(k4_state, time_ + dt);
        ensureDerivativeSize(state_, k4);

        next_state.reserve(state_.size());
        for (std::size_t i = 0; i < state_.size(); ++i) {
            next_state.push_back(state_[i] + dt * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]) / 6.0);
        }
    }

    state_ = clampState(next_state);
    time_ += dt;
}

void ModelDynamicsBase::setParameter(const std::string &name, double value) {
    parameters_[name] = value;
}

void ModelDynamicsBase::applyShock(const std::string &target, double strength) {
    auto it = std::find(species_names_.begin(), species_names_.end(), target);
    if (it == species_names_.end()) {
        return;
    }

    auto index = static_cast<std::size_t>(std::distance(species_names_.begin(), it));
    state_[index] = clampPopulation(state_[index] + strength);
}

const std::vector<double> &ModelDynamicsBase::getStateVector() const {
    return state_;
}

std::vector<std::string> ModelDynamicsBase::getSpeciesNames() const {
    return species_names_;
}

std::map<std::string, double> ModelDynamicsBase::getMetrics() const {
    std::map<std::string, double> metrics;
    metrics["species_count"] = static_cast<double>(species_names_.size());

    double total = 0.0;
    double min_value = state_.empty() ? 0.0 : std::numeric_limits<double>::max();
    double max_value = state_.empty() ? 0.0 : std::numeric_limits<double>::lowest();
    for (double value : state_) {
        total += value;
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }

    metrics["total_population"] = total;
    metrics["min_population"] = state_.empty() ? 0.0 : min_value;
    metrics["max_population"] = state_.empty() ? 0.0 : max_value;
    return metrics;
}

std::string ModelDynamicsBase::checksum() const {
    std::ostringstream canonical;
    canonical << std::setprecision(17);
    canonical << "model=" << model_id_ << ';';
    canonical << "species=";
    for (const auto &name : species_names_) {
        canonical << name << ',';
    }
    canonical << ";state=";
    for (double value : state_) {
        canonical << clampPopulation(value) << ',';
    }
    canonical << ";params=";
    for (const auto &param : parameters_) {
        canonical << param.first << '=' << param.second << ',';
    }

    std::ostringstream output;
    output << std::hex << fnv1a64(canonical.str());
    return output.str();
}

double ModelDynamicsBase::clampPopulation(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    if (value < 0.0 || std::abs(value) < kClampEpsilon) {
        return 0.0;
    }
    return value;
}

std::vector<double> ModelDynamicsBase::clampState(const std::vector<double> &state) {
    std::vector<double> result;
    result.reserve(state.size());
    for (double value : state) {
        result.push_back(clampPopulation(value));
    }
    return result;
}

} // namespace ecosim
