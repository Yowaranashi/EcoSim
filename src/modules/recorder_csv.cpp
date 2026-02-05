#include "modules/recorder_csv.h"
#include "core/logger.h"
#include <filesystem>

namespace ecosim {

RecorderCsv::RecorderCsv(const ModuleInstanceConfig &instance, ModuleContext &context)
    : type_id_(instance.type_id), instance_id_(instance.instance_id), context_(context) {
    auto sink_it = instance.params.find("sink");
    if (sink_it != instance.params.end() && sink_it->second == "memory") {
        memory_only_ = true;
    }
    auto path_it = instance.params.find("path");
    if (path_it != instance.params.end()) {
        output_path_ = path_it->second;
    }
}

void RecorderCsv::onStart() {
    if (!memory_only_) {
        if (output_path_.empty()) {
            output_path_ = context_.config().output_dir + "/simulation.csv";
        }
        std::filesystem::create_directories(std::filesystem::path(output_path_).parent_path());
        file_.open(output_path_, std::ios::out | std::ios::trunc);
        file_ << "tick,seed,energy_total\n";
    }
    context_.eventBus().subscribe("world.tick", [this](const SimulationEvent &event) { handleEvent(event); });
}

void RecorderCsv::onStop() {
    if (file_.is_open()) {
        file_.close();
    }
}

void RecorderCsv::handleEvent(const SimulationEvent &event) {
    events_.push_back(event);
    if (!memory_only_ && file_.is_open()) {
        auto seed_it = event.payload.find("seed");
        auto energy_it = event.payload.find("energy_total");
        file_ << event.tick << ',' << (seed_it != event.payload.end() ? seed_it->second : "") << ','
              << (energy_it != event.payload.end() ? energy_it->second : "") << '\n';
    }
}

} // namespace ecosim
