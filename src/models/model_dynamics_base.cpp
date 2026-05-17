#include "models/model_dynamics_base.h"

#include "core/utils/hash.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <random>
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
    seed_ = config.seed;
    rng_.seed(static_cast<std::mt19937::result_type>(seed_));
    extinction_threshold_ = 0.1;
    environmental_noise_ = 0.0;
    if (auto threshold = parameters_.find("extinction_threshold"); threshold != parameters_.end()) {
        if (std::isfinite(threshold->second) && threshold->second >= 0.0) {
            extinction_threshold_ = threshold->second;
        }
    }
    if (auto noise = parameters_.find("environmental_noise"); noise != parameters_.end()) {
        if (std::isfinite(noise->second) && noise->second >= 0.0) {
            environmental_noise_ = noise->second;
        }
    }

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

    if (environmental_noise_ > 0.0) {
        std::normal_distribution<double> noise(0.0, environmental_noise_);
        for (std::size_t i = 0; i < next_state.size() && i < state_.size(); ++i) {
            if (clampPopulation(state_[i]) > 0.0) {
                next_state[i] += state_[i] * noise(rng_);
            }
        }
    }

    const auto previous_state = state_;
    state_ = clampState(next_state);
    afterStateClamped(previous_state, state_);
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

std::vector<std::string> ModelDynamicsBase::getFlags() const {
    return {};
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
    canonical << ";extinction_threshold=" << extinction_threshold_;
    canonical << ";environmental_noise=" << environmental_noise_;

    std::ostringstream output;
    output << std::hex << utils::fnv1a64(canonical.str());
    return output.str();
}

double ModelDynamicsBase::clampPopulation(double value) const {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    if (value < 0.0 || std::abs(value) < kClampEpsilon) {
        return 0.0;
    }
    if (extinction_threshold_ > 0.0 && value < extinction_threshold_) {
        return 0.0;
    }
    return value;
}

std::vector<double> ModelDynamicsBase::clampState(const std::vector<double> &state) const {
    std::vector<double> result;
    result.reserve(state.size());
    for (double value : state) {
        result.push_back(clampPopulation(value));
    }
    return result;
}

void ModelDynamicsBase::afterStateClamped(const std::vector<double> & /*previous_state*/,
                                          const std::vector<double> & /*next_state*/) {}

} // namespace ecosim
