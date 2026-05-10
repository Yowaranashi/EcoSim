#include "models/glv_dynamics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace ecosim {

namespace {
constexpr double kPivotEpsilon = 1e-12;

std::uint64_t fnv1a64(const std::string &text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::vector<std::string> splitDots(const std::string &value) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        auto end = value.find('.', start);
        if (end == std::string::npos) {
            parts.push_back(value.substr(start));
            break;
        }
        parts.push_back(value.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

bool isGrowthKey(const std::string &key) {
    return key == "growth" || key == "r";
}

bool isSensitivityKey(const std::string &key) {
    return key == "sensitivity" || key == "b";
}

bool isExternalInputKey(const std::string &key) {
    return key == "external_input" || key == "input" || key == "u";
}
} // namespace

void GlvDynamics::configure(const ModelConfig &config) {
    ModelDynamicsBase::configure(config);
    if (model_id_.empty()) {
        model_id_ = "glv";
    }

    resizeParameters(species_names_.size());

    if (config.interaction_matrix.size() == species_names_.size()) {
        interaction_matrix_ = config.interaction_matrix;
        for (auto &row : interaction_matrix_) {
            row.resize(species_names_.size(), 0.0);
        }
    }

    for (const auto &species : config.species) {
        auto index = speciesIndex(species.id);
        if (!index) {
            continue;
        }
        for (const auto &param : species.parameters) {
            if (isGrowthKey(param.first)) {
                growth_[*index] = param.second;
            } else if (isSensitivityKey(param.first)) {
                sensitivity_[*index] = param.second;
            } else if (isExternalInputKey(param.first)) {
                external_input_[*index] = param.second;
            }
        }
    }

    for (const auto &param : config.parameters) {
        applyKnownParameter(param.first, param.second);
    }
}

std::vector<double> GlvDynamics::computeDerivatives(const std::vector<double> &state, double /*time*/) const {
    if (state.size() != species_names_.size()) {
        throw std::runtime_error("gLV state vector size does not match species count");
    }

    std::vector<double> derivatives(state.size(), 0.0);
    for (std::size_t i = 0; i < state.size(); ++i) {
        double rate = growth_[i] + sensitivity_[i] * external_input_[i];
        for (std::size_t j = 0; j < state.size(); ++j) {
            rate += interaction_matrix_[i][j] * state[j];
        }
        derivatives[i] = state[i] * rate;
    }
    return derivatives;
}

void GlvDynamics::setParameter(const std::string &name, double value) {
    parameters_[name] = value;
    applyKnownParameter(name, value);
}

void GlvDynamics::applyShock(const std::string &target, double strength) {
    auto index = speciesIndex(target);
    if (!index) {
        flags_.push_back("shock_target_not_found:" + target);
        return;
    }

    state_[*index] = clampPopulation(state_[*index] + strength);
    flags_.push_back("shock_applied:" + target);
}

std::map<std::string, double> GlvDynamics::getMetrics() const {
    std::map<std::string, double> metrics;
    metrics["biomass_total"] = 0.0;
    metrics["species_count"] = static_cast<double>(species_names_.size());
    metrics["dominant_species_index"] = state_.empty() ? -1.0 : 0.0;
    metrics["min_population"] = state_.empty() ? 0.0 : std::numeric_limits<double>::max();
    metrics["max_population"] = state_.empty() ? 0.0 : std::numeric_limits<double>::lowest();

    for (std::size_t i = 0; i < state_.size(); ++i) {
        metrics["biomass_total"] += state_[i];
        metrics["min_population"] = std::min(metrics["min_population"], state_[i]);
        metrics["max_population"] = std::max(metrics["max_population"], state_[i]);
        if (state_[i] > state_[static_cast<std::size_t>(metrics["dominant_species_index"])]) {
            metrics["dominant_species_index"] = static_cast<double>(i);
        }
    }

    return metrics;
}

std::string GlvDynamics::checksum() const {
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
    canonical << ";growth=";
    for (double value : growth_) {
        canonical << value << ',';
    }
    canonical << ";interaction=";
    for (const auto &row : interaction_matrix_) {
        for (double value : row) {
            canonical << value << ',';
        }
        canonical << '|';
    }
    canonical << ";sensitivity=";
    for (double value : sensitivity_) {
        canonical << value << ',';
    }

    std::ostringstream output;
    output << std::hex << fnv1a64(canonical.str());
    return output.str();
}

std::vector<std::vector<double>> GlvDynamics::jacobian() const {
    std::vector<std::vector<double>> result(state_.size(), std::vector<double>(state_.size(), 0.0));
    for (std::size_t i = 0; i < state_.size(); ++i) {
        double rate = growth_[i] + sensitivity_[i] * external_input_[i];
        for (std::size_t k = 0; k < state_.size(); ++k) {
            rate += interaction_matrix_[i][k] * state_[k];
        }
        for (std::size_t j = 0; j < state_.size(); ++j) {
            result[i][j] = state_[i] * interaction_matrix_[i][j];
            if (i == j) {
                result[i][j] += rate;
            }
        }
    }
    return result;
}

std::optional<std::vector<double>> GlvDynamics::equilibriumCandidate() const {
    const auto n = growth_.size();
    if (n == 0 || interaction_matrix_.size() != n) {
        return std::nullopt;
    }

    std::vector<std::vector<double>> matrix = interaction_matrix_;
    std::vector<double> rhs(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        if (matrix[i].size() != n) {
            return std::nullopt;
        }
        rhs[i] = -growth_[i];
    }

    for (std::size_t column = 0; column < n; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < n; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) {
                pivot = row;
            }
        }
        if (std::abs(matrix[pivot][column]) < kPivotEpsilon) {
            return std::nullopt;
        }
        if (pivot != column) {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(rhs[pivot], rhs[column]);
        }

        const double divisor = matrix[column][column];
        for (std::size_t col = column; col < n; ++col) {
            matrix[column][col] /= divisor;
        }
        rhs[column] /= divisor;

        for (std::size_t row = 0; row < n; ++row) {
            if (row == column) {
                continue;
            }
            const double factor = matrix[row][column];
            for (std::size_t col = column; col < n; ++col) {
                matrix[row][col] -= factor * matrix[column][col];
            }
            rhs[row] -= factor * rhs[column];
        }
    }

    return rhs;
}

std::optional<std::size_t> GlvDynamics::speciesIndex(const std::string &name) const {
    auto it = std::find(species_names_.begin(), species_names_.end(), name);
    if (it == species_names_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(species_names_.begin(), it));
}

void GlvDynamics::applyKnownParameter(const std::string &name, double value) {
    auto parts = splitDots(name);
    if (parts.size() == 2) {
        auto index = speciesIndex(parts[1]);
        if (!index) {
            index = speciesIndex(parts[0]);
        }
        if (!index) {
            return;
        }

        if (isGrowthKey(parts[0]) || isGrowthKey(parts[1])) {
            growth_[*index] = value;
        } else if (isSensitivityKey(parts[0]) || isSensitivityKey(parts[1])) {
            sensitivity_[*index] = value;
        } else if (isExternalInputKey(parts[0]) || isExternalInputKey(parts[1])) {
            external_input_[*index] = value;
        }
    } else if (parts.size() == 3 && parts[0] == "interaction") {
        auto row = speciesIndex(parts[1]);
        auto col = speciesIndex(parts[2]);
        if (row && col) {
            interaction_matrix_[*row][*col] = value;
        }
    } else if (parts.size() == 1 && species_names_.size() == 1) {
        if (isGrowthKey(parts[0])) {
            growth_[0] = value;
        } else if (isSensitivityKey(parts[0])) {
            sensitivity_[0] = value;
        } else if (isExternalInputKey(parts[0])) {
            external_input_[0] = value;
        }
    }
}

void GlvDynamics::resizeParameters(std::size_t species_count) {
    growth_.assign(species_count, 0.0);
    sensitivity_.assign(species_count, 0.0);
    external_input_.assign(species_count, 0.0);
    interaction_matrix_.assign(species_count, std::vector<double>(species_count, 0.0));
    flags_.clear();
}

} // namespace ecosim
