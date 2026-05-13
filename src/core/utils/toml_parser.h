#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ecosim::utils {

class TomlValue {
public:
    enum class Type { String, Boolean, Number, Array, Table };
    using Array = std::vector<TomlValue>;
    using Table = std::map<std::string, TomlValue>;

    static TomlValue string(std::string value);
    static TomlValue boolean(bool value);
    static TomlValue number(double value, std::string source);
    static TomlValue array(Array value);
    static TomlValue table(Table value);

    Type type() const { return type_; }
    bool isString() const { return type_ == Type::String; }
    bool isBoolean() const { return type_ == Type::Boolean; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isArray() const { return type_ == Type::Array; }
    bool isTable() const { return type_ == Type::Table; }

    std::optional<std::string> asString() const;
    std::optional<double> asDouble() const;
    std::optional<int> asInt() const;
    std::optional<bool> asBool() const;
    const Array *asArray() const;
    const Table *asTable() const;

private:
    Type type_ = Type::String;
    std::string string_;
    bool boolean_ = false;
    double number_ = 0.0;
    Array array_;
    Table table_;
};

class TomlDocument {
public:
    static TomlDocument parseFile(const std::string &path);
    static TomlDocument parse(const std::string &content, const std::string &source_name);

    const TomlValue *find(const std::string &key) const;
    const TomlValue::Table *findTable(const std::string &table_name) const;

private:
    TomlValue::Table root_;
    std::map<std::string, TomlValue::Table> tables_;
};

} // namespace ecosim::utils
