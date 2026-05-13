#pragma once

#include <cstdint>
#include <string_view>

namespace ecosim::utils {

inline std::uint64_t fnv1a64(std::string_view text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace ecosim::utils
