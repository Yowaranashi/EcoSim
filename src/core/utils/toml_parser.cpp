#include "core/utils/toml_parser.h"

#include "core/utils/parse_utils.h"
#include "core/utils/string_utils.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ecosim::utils {

namespace {

struct ParseContext {
    std::string source_name;
    int line = 0;
};

[[noreturn]] void throwParseError(const ParseContext &context, const std::string &message) {
    std::ostringstream out;
    out << "TOML parse error in " << context.source_name;
    if (context.line > 0) {
        out << ':' << context.line;
    }
    out << ": " << message;
    throw std::runtime_error(out.str());
}

std::string loadTextFile(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Unable to open file: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool isEscaped(const std::string &text, std::size_t pos) {
    std::size_t slash_count = 0;
    while (pos > slash_count && text[pos - slash_count - 1] == '\\') {
        ++slash_count;
    }
    return slash_count % 2 != 0;
}

std::string stripLineComment(const std::string &line) {
    bool in_quotes = false;
    char quote = '\0';

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if ((c == '"' || c == '\'') && !isEscaped(line, i)) {
            if (!in_quotes) {
                in_quotes = true;
                quote = c;
            } else if (quote == c) {
                in_quotes = false;
            }
            continue;
        }
        if (!in_quotes && c == '#') {
            return line.substr(0, i);
        }
    }

    return line;
}

std::size_t findTopLevelEquals(const std::string &input) {
    bool in_quotes = false;
    char quote = '\0';
    int brace_depth = 0;
    int bracket_depth = 0;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if ((c == '"' || c == '\'') && !isEscaped(input, i)) {
            if (!in_quotes) {
                in_quotes = true;
                quote = c;
            } else if (quote == c) {
                in_quotes = false;
            }
            continue;
        }
        if (in_quotes) {
            continue;
        }
        if (c == '{') {
            ++brace_depth;
        } else if (c == '}') {
            --brace_depth;
        } else if (c == '[') {
            ++bracket_depth;
        } else if (c == ']') {
            --bracket_depth;
        } else if (c == '=' && brace_depth == 0 && bracket_depth == 0) {
            return i;
        }
    }

    return std::string::npos;
}

std::vector<std::string> splitTopLevel(const std::string &input, char delimiter) {
    std::vector<std::string> result;
    bool in_quotes = false;
    char quote = '\0';
    int brace_depth = 0;
    int bracket_depth = 0;
    std::size_t start = 0;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if ((c == '"' || c == '\'') && !isEscaped(input, i)) {
            if (!in_quotes) {
                in_quotes = true;
                quote = c;
            } else if (quote == c) {
                in_quotes = false;
            }
            continue;
        }
        if (in_quotes) {
            continue;
        }
        if (c == '{') {
            ++brace_depth;
        } else if (c == '}') {
            --brace_depth;
        } else if (c == '[') {
            ++bracket_depth;
        } else if (c == ']') {
            --bracket_depth;
        } else if (c == delimiter && brace_depth == 0 && bracket_depth == 0) {
            auto item = trim(input.substr(start, i - start));
            if (!item.empty()) {
                result.push_back(std::move(item));
            }
            start = i + 1;
        }
    }

    auto tail = trim(input.substr(start));
    if (!tail.empty()) {
        result.push_back(std::move(tail));
    }
    return result;
}

int balanceDelta(const std::string &input) {
    bool in_quotes = false;
    char quote = '\0';
    int balance = 0;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if ((c == '"' || c == '\'') && !isEscaped(input, i)) {
            if (!in_quotes) {
                in_quotes = true;
                quote = c;
            } else if (quote == c) {
                in_quotes = false;
            }
            continue;
        }
        if (in_quotes) {
            continue;
        }
        if (c == '[' || c == '{') {
            ++balance;
        } else if (c == ']' || c == '}') {
            --balance;
        }
    }

    return balance;
}

std::string unquote(const std::string &input, const ParseContext &context) {
    auto value = trim(input);
    if (value.size() < 2 || (value.front() != '"' && value.front() != '\'')) {
        return value;
    }
    const char quote = value.front();
    if (value.back() != quote) {
        throwParseError(context, "unterminated string");
    }
    value = value.substr(1, value.size() - 2);
    if (quote == '\'') {
        return value;
    }

    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            result += value[i];
            continue;
        }
        const char escaped = value[++i];
        switch (escaped) {
        case 'n':
            result += '\n';
            break;
        case 'r':
            result += '\r';
            break;
        case 't':
            result += '\t';
            break;
        case '"':
        case '\\':
            result += escaped;
            break;
        default:
            result += escaped;
            break;
        }
    }
    return result;
}

std::string normalizeKey(const std::string &input, const ParseContext &context) {
    auto key = trim(input);
    if (key.empty()) {
        throwParseError(context, "empty key");
    }
    if (key.front() == '"' || key.front() == '\'') {
        return unquote(key, context);
    }
    return key;
}

TomlValue parseValue(const std::string &input, const ParseContext &context);

TomlValue parseArray(const std::string &input, const ParseContext &context) {
    auto content = trim(input);
    if (content.size() < 2 || content.front() != '[' || content.back() != ']') {
        throwParseError(context, "invalid array value");
    }
    content = content.substr(1, content.size() - 2);

    TomlValue::Array values;
    for (const auto &entry : splitTopLevel(content, ',')) {
        values.push_back(parseValue(entry, context));
    }
    return TomlValue::array(std::move(values));
}

TomlValue parseInlineTable(const std::string &input, const ParseContext &context) {
    auto content = trim(input);
    if (content.size() < 2 || content.front() != '{' || content.back() != '}') {
        throwParseError(context, "invalid inline table");
    }
    content = content.substr(1, content.size() - 2);

    TomlValue::Table table;
    for (const auto &entry : splitTopLevel(content, ',')) {
        const auto eq = findTopLevelEquals(entry);
        if (eq == std::string::npos) {
            throwParseError(context, "inline table entry is missing '='");
        }
        table[normalizeKey(entry.substr(0, eq), context)] = parseValue(entry.substr(eq + 1), context);
    }
    return TomlValue::table(std::move(table));
}

TomlValue parseValue(const std::string &input, const ParseContext &context) {
    auto value = trim(input);
    if (value.empty()) {
        throwParseError(context, "empty value");
    }
    if (value.front() == '"' || value.front() == '\'') {
        return TomlValue::string(unquote(value, context));
    }
    if (value == "true") {
        return TomlValue::boolean(true);
    }
    if (value == "false") {
        return TomlValue::boolean(false);
    }
    if (value.front() == '[') {
        return parseArray(value, context);
    }
    if (value.front() == '{') {
        return parseInlineTable(value, context);
    }
    if (auto number = parseDouble(value)) {
        return TomlValue::number(*number, value);
    }
    return TomlValue::string(value);
}

bool isTableHeader(const std::string &line) {
    return line.size() >= 2 && line.front() == '[' && line.back() == ']' &&
           (line.size() < 4 || line[1] != '[');
}

} // namespace

TomlValue TomlValue::string(std::string value) {
    TomlValue result;
    result.type_ = Type::String;
    result.string_ = std::move(value);
    return result;
}

TomlValue TomlValue::boolean(bool value) {
    TomlValue result;
    result.type_ = Type::Boolean;
    result.boolean_ = value;
    result.string_ = value ? "true" : "false";
    return result;
}

TomlValue TomlValue::number(double value, std::string source) {
    TomlValue result;
    result.type_ = Type::Number;
    result.number_ = value;
    result.string_ = std::move(source);
    return result;
}

TomlValue TomlValue::array(Array value) {
    TomlValue result;
    result.type_ = Type::Array;
    result.array_ = std::move(value);
    return result;
}

TomlValue TomlValue::table(Table value) {
    TomlValue result;
    result.type_ = Type::Table;
    result.table_ = std::move(value);
    return result;
}

std::optional<std::string> TomlValue::asString() const {
    if (type_ == Type::String || type_ == Type::Number || type_ == Type::Boolean) {
        return string_;
    }
    return std::nullopt;
}

std::optional<double> TomlValue::asDouble() const {
    if (type_ == Type::Number) {
        return number_;
    }
    if (type_ == Type::String) {
        return parseDouble(string_);
    }
    return std::nullopt;
}

std::optional<int> TomlValue::asInt() const {
    if (type_ == Type::Number || type_ == Type::String) {
        return parseInt(string_);
    }
    return std::nullopt;
}

std::optional<bool> TomlValue::asBool() const {
    if (type_ == Type::Boolean) {
        return boolean_;
    }
    if (type_ == Type::String) {
        if (string_ == "true") {
            return true;
        }
        if (string_ == "false") {
            return false;
        }
    }
    return std::nullopt;
}

const TomlValue::Array *TomlValue::asArray() const {
    return type_ == Type::Array ? &array_ : nullptr;
}

const TomlValue::Table *TomlValue::asTable() const {
    return type_ == Type::Table ? &table_ : nullptr;
}

TomlDocument TomlDocument::parseFile(const std::string &path) {
    return parse(loadTextFile(path), path);
}

TomlDocument TomlDocument::parse(const std::string &content, const std::string &source_name) {
    TomlDocument document;
    std::istringstream stream(content);
    std::string raw_line;
    std::string active_table;
    std::string pending_key;
    std::string pending_value;
    std::string pending_table;
    int pending_balance = 0;
    int line_number = 0;

    auto commitValue = [&](const std::string &key,
                           const std::string &value,
                           const std::string &table_name,
                           const ParseContext &context) {
        auto parsed_value = parseValue(value, context);
        if (table_name.empty()) {
            document.root_[normalizeKey(key, context)] = std::move(parsed_value);
        } else {
            document.tables_[table_name][normalizeKey(key, context)] = std::move(parsed_value);
        }
    };

    while (std::getline(stream, raw_line)) {
        ++line_number;
        ParseContext context{source_name, line_number};
        auto line = trim(stripLineComment(raw_line));
        if (line.empty()) {
            continue;
        }

        if (!pending_key.empty()) {
            pending_value += '\n';
            pending_value += line;
            pending_balance += balanceDelta(line);
            if (pending_balance <= 0) {
                commitValue(pending_key, pending_value, pending_table, context);
                pending_key.clear();
                pending_value.clear();
                pending_table.clear();
                pending_balance = 0;
            }
            continue;
        }

        if (isTableHeader(line)) {
            active_table = normalizeKey(line.substr(1, line.size() - 2), context);
            document.tables_[active_table];
            continue;
        }

        const auto eq = findTopLevelEquals(line);
        if (eq == std::string::npos) {
            throwParseError(context, "line is missing '='");
        }

        const auto key = line.substr(0, eq);
        const auto value = trim(line.substr(eq + 1));
        const auto delta = balanceDelta(value);
        if (delta > 0) {
            pending_key = key;
            pending_value = value;
            pending_table = active_table;
            pending_balance = delta;
            continue;
        }
        if (delta < 0) {
            throwParseError(context, "unbalanced closing bracket or brace");
        }
        commitValue(key, value, active_table, context);
    }

    if (!pending_key.empty()) {
        ParseContext context{source_name, line_number};
        throwParseError(context, "unterminated multiline value");
    }

    return document;
}

const TomlValue *TomlDocument::find(const std::string &key) const {
    auto it = root_.find(key);
    return it == root_.end() ? nullptr : &it->second;
}

const TomlValue::Table *TomlDocument::findTable(const std::string &table_name) const {
    auto it = tables_.find(table_name);
    return it == tables_.end() ? nullptr : &it->second;
}

} // namespace ecosim::utils
