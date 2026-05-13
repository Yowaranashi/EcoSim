#pragma once

#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace ecosim::utils {

inline std::string trim(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

inline bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

inline std::vector<std::string> splitTrimmed(std::string_view value, char delimiter = ',') {
    std::vector<std::string> result;
    std::string text(value);
    std::istringstream stream(text);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        auto cleaned = trim(item);
        if (!cleaned.empty()) {
            result.push_back(std::move(cleaned));
        }
    }
    return result;
}

inline std::string join(const std::vector<std::string> &values, char delimiter = ',') {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << delimiter;
        }
        out << values[i];
    }
    return out.str();
}

} // namespace ecosim::utils
