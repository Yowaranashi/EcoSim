#include "core/console.h"

#include <sstream>

namespace ecosim {

void Console::registerCommand(const std::string &name, CommandHandler handler) {
    handlers_[name] = std::move(handler);
}

bool Console::execute(const std::string &line) {
    std::istringstream stream(line);
    std::string command;
    stream >> command;
    if (command.empty()) {
        return false;
    }
    std::vector<std::string> args;
    std::string arg;
    while (stream >> arg) {
        args.push_back(arg);
    }
    auto it = handlers_.find(command);
    if (it == handlers_.end()) {
        return false;
    }
    it->second(args);
    return true;
}

} // namespace ecosim
