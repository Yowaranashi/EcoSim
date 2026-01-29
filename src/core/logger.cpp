#include "core/logger.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace ecosim {

namespace {
std::string channelLabel(LogChannel channel) {
    switch (channel) {
    case LogChannel::System:
        return "system";
    case LogChannel::Simulation:
        return "simulation";
    }
    return "unknown";
}

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}
} // namespace

Logger::Logger(std::ostream &output) : output_(output) {}

void Logger::log(LogChannel channel, const std::string &message) {
    output_ << '[' << timestamp() << "] [" << channelLabel(channel) << "] " << message << '\n';
}

} // namespace ecosim
