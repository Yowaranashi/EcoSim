#pragma once

#include "core/module.h"
#include "modules/world_port.h"
#include "models/integration_method.h"
#include "models/model_dynamics.h"

#include <map>
#include <memory>
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
    void enqueueCommand(const WorldCommand &command) override;

    const ReadModel &readModel() const override { return read_model_; }
    bool shouldStop() const override;
    std::string checksum() const;

private:
    void applyCommand(const WorldCommand &command);
    void updateReadModel();
    void syncLegacyDerivedState();
    void emitTickEvent();
    void logSimulationStartIfNeeded();
    void logTickProgress() const;
    std::string legacyChecksum() const;

    std::string type_id_;
    std::string instance_id_;
    ModuleContext &context_;
    ReadModel read_model_;
    std::map<std::string, double> params_;
    std::vector<std::string> species_order_;
    std::vector<WorldCommand> pending_commands_;
    std::unique_ptr<IModelDynamics> dynamics_;
    IntegrationMethod integration_method_ = IntegrationMethod::Euler;
    double dt_ = 1.0;
    int seed_ = 0;
    int stop_at_tick_ = -1;
    int log_tick_interval_ = 50;
    bool log_tick_details_ = false;
    bool simulation_configured_ = false;
    bool start_logged_ = false;
    std::string scenario_id_;
    std::string model_id_;
    std::string integrator_;
    std::vector<std::string> flags_;
};

} // namespace ecosim
