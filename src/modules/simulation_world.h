#pragma once

#include "core/module.h"
#include "modules/world_port.h"

#include <map>
#include <string>
#include <vector>

namespace ecosim {

class SimulationWorld : public IModule, public IWorldPort {
public:
    SimulationWorld(const ModuleInstanceConfig &instance, ModuleContext &context);

    const std::string &typeId() const override { return type_id_; }
    const std::string &instanceId() const override { return instance_id_; }

    void onInit() override;
    void onPreTick() override;
    void onTick() override;

    void enqueueCommand(const std::string &command,
                        const std::map<std::string, std::string> &params) override;

    const ReadModel &readModel() const override { return read_model_; }
    bool shouldStop() const override;
    std::string checksum() const;

private:
    void applyCommand(const std::string &command, const std::map<std::string, std::string> &params);
    void emitTickEvent();

    std::string type_id_;
    std::string instance_id_;
    ModuleContext &context_;
    ReadModel read_model_;
    std::map<std::string, double> params_;
    std::vector<std::string> species_order_;
    std::vector<std::pair<std::string, std::map<std::string, std::string>>> pending_commands_;
    int stop_at_tick_ = -1;
};

} // namespace ecosim
