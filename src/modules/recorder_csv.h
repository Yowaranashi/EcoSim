#pragma once

#include "core/module.h"

#include <fstream>
#include <string>
#include <vector>

namespace ecosim {

class RecorderCsv : public IModule {
public:
    RecorderCsv(const ModuleInstanceConfig &instance, ModuleContext &context);

    const std::string &typeId() const override { return type_id_; }
    const std::string &instanceId() const override { return instance_id_; }

    void onStart() override;
    void onStop() override;

    const std::vector<SimulationEvent> &events() const { return events_; }

private:
    void handleEvent(const SimulationEvent &event);

    std::string type_id_;
    std::string instance_id_;
    ModuleContext &context_;
    std::string output_path_;
    bool memory_only_ = false;
    std::ofstream file_;
    std::vector<SimulationEvent> events_;
};

} // namespace ecosim
