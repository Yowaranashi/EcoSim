#include "models/rosenzweig_macarthur_dynamics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ecosim {

namespace {
constexpr double kDenominatorEpsilon = 1e-12;

std::uint64_t fnv1a64(const std::string &text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string suffixAfterDot(const std::string &name) {
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

bool isRKey(const std::string &name) {
    const auto key = suffixAfterDot(name);
    return key == "r" || key == "growth" || key == "resource_growth";
}

bool isKKey(const std::string &name) {
    const auto key = suffixAfterDot(name);
    return key == "K" || key == "carrying_capacity";
}

bool isAKey(const std::string &name) {
    const auto key = suffixAfterDot(name);
    return key == "a" || key == "attack_rate";
}

bool isHKey(const std::string &name) {
    const auto key = suffixAfterDot(name);
    return key == "h" || key == "handling_time";
}

bool isEKey(const std::string &name) {
    const auto key = suffixAfterDot(name);
    return key == "e" || key == "conversion_efficiency";
}

bool isMKey(const std::string &name) {
    const auto key = suffixAfterDot(name);
    return key == "m" || key == "mortality";
}
} // namespace

void RosenzweigMacArthurDynamics::configure(const ModelConfig &config) {
    model_id_ = config.model_id.empty() ? "rosenzweig_macarthur" : config.model_id;
    if (model_id_ != "rosenzweig_macarthur") {
        throw std::invalid_argument("Rosenzweig-MacArthur model_id must be rosenzweig_macarthur");
    }

    parameters_ = config.parameters;
    time_ = 0.0;
    flags_.clear();

    r_ = 1.0;
    K_ = 100.0;
    a_ = 0.1;
    h_ = 0.2;
    e_ = 0.5;
    m_ = 0.2;

    species_names_.clear();
    state_.clear();
    if (config.species.empty()) {
        species_names_ = {"prey", "predator"};
        auto prey = config.initial_state.find("prey");
        auto predator = config.initial_state.find("predator");
        if (prey == config.initial_state.end() || predator == config.initial_state.end()) {
            throw std::invalid_argument("Rosenzweig-MacArthur initial_state must contain prey and predator");
        }
        state_ = {clampPopulation(prey->second), clampPopulation(predator->second)};
    } else {
        if (config.species.size() != 2) {
            throw std::invalid_argument("Rosenzweig-MacArthur model requires exactly two species");
        }
        for (const auto &species : config.species) {
            species_names_.push_back(species.id);

            double initial_value = species.initial_state;
            if (auto it = config.initial_state.find(species.id); it != config.initial_state.end()) {
                initial_value = it->second;
            }
            state_.push_back(clampPopulation(initial_value));

            for (const auto &param : species.parameters) {
                const auto qualified_name = species.id + "." + param.first;
                parameters_[qualified_name] = param.second;
                applyKnownParameter(qualified_name, param.second);
            }
        }
    }

    for (const auto &param : config.parameters) {
        applyKnownParameter(param.first, param.second);
    }
    validateParameters();
}

std::vector<double> RosenzweigMacArthurDynamics::computeDerivatives(
    const std::vector<double> &state,
    double /*time*/
) const {
    if (state.size() != 2) {
        throw std::runtime_error("Rosenzweig-MacArthur state vector must contain prey and predator");
    }

    const double X = state[0];
    const double Y = state[1];
    const double denominator = 1.0 + a_ * h_ * X;
    if (std::abs(denominator) < kDenominatorEpsilon) {
        throw std::runtime_error("Rosenzweig-MacArthur functional response denominator is zero");
    }

    const double predation = (a_ * X * Y) / denominator;
    const double dXdt = r_ * X * (1.0 - X / K_) - predation;
    const double dYdt = e_ * predation - m_ * Y;
    return {dXdt, dYdt};
}

void RosenzweigMacArthurDynamics::setParameter(const std::string &name, double value) {
    parameters_[name] = value;
    applyKnownParameter(name, value);
    validateParameters();
}

void RosenzweigMacArthurDynamics::applyShock(const std::string &target, double strength) {
    if (matchesPrey(target)) {
        state_[0] = clampPopulation(state_[0] + strength);
        flags_.push_back("shock.prey");
    } else if (matchesPredator(target)) {
        state_[1] = clampPopulation(state_[1] + strength);
        flags_.push_back("shock.predator");
    } else {
        flags_.push_back("shock_target_not_found:" + target);
    }
}

std::map<std::string, double> RosenzweigMacArthurDynamics::getMetrics() const {
    if (state_.size() != 2) {
        throw std::runtime_error("Rosenzweig-MacArthur state vector must contain prey and predator");
    }

    const double denominator = 1.0 + a_ * h_ * state_[0];
    if (std::abs(denominator) < kDenominatorEpsilon) {
        throw std::runtime_error("Rosenzweig-MacArthur functional response denominator is zero");
    }
    const double predation_flow = (a_ * state_[0] * state_[1]) / denominator;

    return {
        {"biomass_total", state_[0] + state_[1]},
        {"phase_x", state_[0]},
        {"phase_y", state_[1]},
        {"predation_flow", predation_flow},
        {"predator", state_[1]},
        {"prey", state_[0]},
    };
}

std::string RosenzweigMacArthurDynamics::checksum() const {
    std::ostringstream canonical;
    canonical << std::setprecision(17);
    canonical << "model=" << model_id_ << ';';
    canonical << "species=";
    for (const auto &name : species_names_) {
        canonical << name << ',';
    }
    canonical << ";state=";
    for (double value : state_) {
        canonical << value << ',';
    }
    canonical << ";params=";
    canonical << "r=" << r_ << ",K=" << K_ << ",a=" << a_ << ",h=" << h_ << ",e=" << e_ << ",m=" << m_;

    std::ostringstream output;
    output << std::hex << fnv1a64(canonical.str());
    return output.str();
}

std::array<std::array<double, 2>, 2> RosenzweigMacArthurDynamics::jacobian2x2() const {
    if (state_.size() != 2) {
        throw std::runtime_error("Rosenzweig-MacArthur state vector must contain prey and predator");
    }

    const double X = state_[0];
    const double Y = state_[1];
    const double denominator = 1.0 + a_ * h_ * X;
    if (std::abs(denominator) < kDenominatorEpsilon) {
        throw std::runtime_error("Rosenzweig-MacArthur functional response denominator is zero");
    }

    const double denominator_squared = denominator * denominator;
    return {{
        {r_ * (1.0 - 2.0 * X / K_) - (a_ * Y) / denominator_squared, -(a_ * X) / denominator},
        {(e_ * a_ * Y) / denominator_squared, (e_ * a_ * X) / denominator - m_},
    }};
}

std::optional<std::vector<double>> RosenzweigMacArthurDynamics::equilibriumCandidate() const {
    const double denominator = a_ * (e_ - m_ * h_);
    if (denominator <= 0.0) {
        return std::nullopt;
    }

    const double X = m_ / denominator;
    if (X <= 0.0) {
        return std::nullopt;
    }

    const double Y = r_ * (1.0 - X / K_) * (1.0 + a_ * h_ * X) / a_;
    if (Y <= 0.0) {
        return std::nullopt;
    }

    return std::vector<double>{X, Y};
}

void RosenzweigMacArthurDynamics::applyKnownParameter(const std::string &name, double value) {
    if (isRKey(name)) {
        r_ = value;
    } else if (isKKey(name)) {
        K_ = value;
    } else if (isAKey(name)) {
        a_ = value;
    } else if (isHKey(name)) {
        h_ = value;
    } else if (isEKey(name)) {
        e_ = value;
    } else if (isMKey(name)) {
        m_ = value;
    }
}

void RosenzweigMacArthurDynamics::validateParameters() const {
    if (!std::isfinite(r_) || !std::isfinite(K_) || !std::isfinite(a_) || !std::isfinite(h_) ||
        !std::isfinite(e_) || !std::isfinite(m_)) {
        throw std::invalid_argument("Rosenzweig-MacArthur parameters must be finite");
    }
    if (K_ <= 0.0) {
        throw std::invalid_argument("Rosenzweig-MacArthur carrying capacity K must be positive");
    }
    if (r_ < 0.0 || a_ < 0.0 || h_ < 0.0 || e_ < 0.0 || m_ < 0.0) {
        throw std::invalid_argument("Rosenzweig-MacArthur rates must be non-negative");
    }
}

bool RosenzweigMacArthurDynamics::matchesPrey(const std::string &target) const {
    return target == "prey" || (!species_names_.empty() && target == species_names_[0]);
}

bool RosenzweigMacArthurDynamics::matchesPredator(const std::string &target) const {
    return target == "predator" || (species_names_.size() > 1 && target == species_names_[1]);
}

} // namespace ecosim
