#pragma once

#include "core/module.h"

#include <string>

namespace ecosim {

class AgentBehavoir : public IModule {
public:
    AgentBehavoir(const ModuleInstanceConfig &instance, ModuleContext &context);

    const std::string &typeId() const override { return type_id_; }
    const std::string &instanceId() const override { return instance_id_; }

    void onInit() override;
    void onTick() override;

private:
    std::string type_id_;
    std::string instance_id_;
    ModuleContext &context_;
};

} // namespace ecosim
