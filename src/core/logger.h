#pragma once

#include <ostream>
#include <string>

namespace ecosim {

enum class LogChannel {
    System,
    Simulation
};

class Logger {
public:
    explicit Logger(std::ostream &output);

    void log(LogChannel channel, const std::string &message);

private:
    std::ostream &output_;
};

} // namespace ecosim
