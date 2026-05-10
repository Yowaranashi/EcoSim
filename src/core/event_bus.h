#pragma once

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace ecosim {

struct SimulationEvent {
    std::string type;
    int tick = 0;
    std::unordered_map<std::string, std::string> payload;
    std::unordered_map<std::string, double> numeric_payload;
    std::unordered_map<std::string, std::vector<std::string>> string_list_payload;
    std::unordered_map<std::string, std::vector<double>> numeric_vector_payload;
    std::map<std::string, double> metrics;
    std::vector<std::string> flags;
};

class EventBus {
public:
    using Handler = std::function<void(const SimulationEvent &)>;

    void subscribe(const std::string &event_type, Handler handler);
    void emit(const SimulationEvent &event);
    void deliverBuffered();
    void clear();

    std::size_t bufferedCount() const;

private:
    std::unordered_map<std::string, std::vector<Handler>> subscribers_;
    std::vector<SimulationEvent> buffer_;
};

} // namespace ecosim
