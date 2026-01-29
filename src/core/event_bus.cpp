#include "core/event_bus.h"

namespace ecosim {

void EventBus::subscribe(const std::string &event_type, Handler handler) {
    subscribers_[event_type].push_back(std::move(handler));
}

void EventBus::emit(const SimulationEvent &event) {
    buffer_.push_back(event);
}

void EventBus::deliverBuffered() {
    auto to_deliver = buffer_;
    buffer_.clear();
    for (const auto &event : to_deliver) {
        auto it = subscribers_.find(event.type);
        if (it == subscribers_.end()) {
            continue;
        }
        for (const auto &handler : it->second) {
            handler(event);
        }
    }
}

void EventBus::clear() {
    buffer_.clear();
}

std::size_t EventBus::bufferedCount() const {
    return buffer_.size();
}

} // namespace ecosim
