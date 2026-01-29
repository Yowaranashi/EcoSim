#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ecosim {

class Console {
public:
    using CommandHandler = std::function<void(const std::vector<std::string> &)>;

    void registerCommand(const std::string &name, CommandHandler handler);
    bool execute(const std::string &line);

private:
    std::map<std::string, CommandHandler> handlers_;
};

} // namespace ecosim
