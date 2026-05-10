#include "viewer/csv_result_reader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

namespace ecosim::viewer {

namespace {
bool startsWith(const std::string &value, const std::string &prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string trim(const std::string &value) {
    auto first = value.begin();
    while (first != value.end() && std::isspace(static_cast<unsigned char>(*first))) {
        ++first;
    }

    auto last = value.end();
    while (last != first && std::isspace(static_cast<unsigned char>(*(last - 1)))) {
        --last;
    }

    return std::string(first, last);
}

double parseDoubleOr(const std::string &value, double fallback) {
    const auto cleaned = trim(value);
    if (cleaned.empty()) {
        return fallback;
    }

    char *end = nullptr;
    const double parsed = std::strtod(cleaned.c_str(), &end);
    if (end == cleaned.c_str()) {
        return fallback;
    }
    return parsed;
}

std::vector<std::string> splitFlags(const std::string &value) {
    std::vector<std::string> flags;
    std::string current;
    for (char ch : value) {
        if (ch == '|') {
            auto cleaned = trim(current);
            if (!cleaned.empty()) {
                flags.push_back(cleaned);
            }
            current.clear();
        } else {
            current += ch;
        }
    }

    auto cleaned = trim(current);
    if (!cleaned.empty()) {
        flags.push_back(cleaned);
    }
    return flags;
}

std::vector<std::vector<std::string>> readCsvRecords(std::istream &input) {
    std::vector<std::vector<std::string>> records;
    std::vector<std::string> record;
    std::string field;
    bool in_quotes = false;

    char ch = '\0';
    while (input.get(ch)) {
        if (in_quotes) {
            if (ch == '"') {
                if (input.peek() == '"') {
                    input.get(ch);
                    field += '"';
                } else {
                    in_quotes = false;
                }
            } else {
                field += ch;
            }
            continue;
        }

        if (ch == '"') {
            if (field.empty()) {
                in_quotes = true;
            } else {
                field += ch;
            }
        } else if (ch == ',') {
            record.push_back(field);
            field.clear();
        } else if (ch == '\n') {
            record.push_back(field);
            records.push_back(record);
            record.clear();
            field.clear();
        } else if (ch == '\r') {
            if (input.peek() == '\n') {
                input.get(ch);
            }
            record.push_back(field);
            records.push_back(record);
            record.clear();
            field.clear();
        } else {
            field += ch;
        }
    }

    if (!record.empty() || !field.empty()) {
        record.push_back(field);
        records.push_back(record);
    }

    return records;
}

std::string valueAt(const std::vector<std::string> &row, std::size_t index) {
    return index < row.size() ? row[index] : std::string{};
}

std::vector<std::pair<std::size_t, std::string>> collectPrefixedColumns(const std::vector<std::string> &header,
                                                                        const std::string &prefix) {
    std::vector<std::pair<std::size_t, std::string>> columns;
    for (std::size_t i = 0; i < header.size(); ++i) {
        if (startsWith(header[i], prefix) && header[i].size() > prefix.size()) {
            columns.emplace_back(i, header[i].substr(prefix.size()));
        }
    }
    return columns;
}
} // namespace

std::vector<SimulationFrame> CsvResultReader::read(const std::string &path) const {
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        return {};
    }

    const auto records = readCsvRecords(file);
    if (records.empty()) {
        return {};
    }

    const auto &header = records.front();
    std::map<std::string, std::size_t> column_index;
    for (std::size_t i = 0; i < header.size(); ++i) {
        column_index[header[i]] = i;
    }

    const auto state_columns = collectPrefixedColumns(header, "state.");
    const auto metric_columns = collectPrefixedColumns(header, "metric.");
    std::vector<SimulationFrame> frames;
    frames.reserve(records.size() - 1);

    auto columnValue = [&](const std::vector<std::string> &row, const std::string &column) {
        auto it = column_index.find(column);
        return it == column_index.end() ? std::string{} : valueAt(row, it->second);
    };

    for (std::size_t row_index = 1; row_index < records.size(); ++row_index) {
        const auto &row = records[row_index];
        if (row.empty() || (row.size() == 1 && trim(row.front()).empty())) {
            continue;
        }

        SimulationFrame frame;
        frame.tick = static_cast<int>(parseDoubleOr(columnValue(row, "tick"), 0.0));
        frame.time = parseDoubleOr(columnValue(row, "time"), 0.0);
        frame.dt = parseDoubleOr(columnValue(row, "dt"), 0.0);
        frame.scenario_id = columnValue(row, "scenario_id");
        frame.model_id = columnValue(row, "model_id");
        frame.integrator = columnValue(row, "integrator");
        frame.checksum = columnValue(row, "checksum");
        frame.flags = splitFlags(columnValue(row, "flags"));

        frame.species_names.reserve(state_columns.size());
        frame.state_values.reserve(state_columns.size());
        for (const auto &column : state_columns) {
            frame.species_names.push_back(column.second);
            frame.state_values.push_back(parseDoubleOr(valueAt(row, column.first), 0.0));
        }

        for (const auto &column : metric_columns) {
            const auto value = valueAt(row, column.first);
            if (!trim(value).empty()) {
                frame.metrics[column.second] = parseDoubleOr(value, 0.0);
            }
        }

        frames.push_back(std::move(frame));
    }

    return frames;
}

} // namespace ecosim::viewer
