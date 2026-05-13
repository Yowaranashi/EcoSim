#pragma once

#include "core/utils/string_utils.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace ecosim::utils {

inline std::optional<double> parseDouble(std::string_view value) {
    try {
        std::size_t parsed = 0;
        const auto text = trim(value);
        const double number = std::stod(text, &parsed);
        if (parsed == text.size()) {
            return number;
        }
    } catch (const std::invalid_argument &) {
    } catch (const std::out_of_range &) {
    }
    return std::nullopt;
}

inline std::optional<int> parseInt(std::string_view value) {
    try {
        std::size_t parsed = 0;
        const auto text = trim(value);
        const int number = std::stoi(text, &parsed);
        if (parsed == text.size()) {
            return number;
        }
    } catch (const std::invalid_argument &) {
    } catch (const std::out_of_range &) {
    }
    return std::nullopt;
}

inline double parseDoubleOr(std::string_view value, double fallback = 0.0) {
    auto parsed = parseDouble(value);
    return parsed ? *parsed : fallback;
}

inline int parseIntOr(std::string_view value, int fallback = 0) {
    auto parsed = parseInt(value);
    return parsed ? *parsed : fallback;
}

} // namespace ecosim::utils
