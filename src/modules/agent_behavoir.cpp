#include "modules/agent_behavoir.h"

#include "core/logger.h"

namespace ecosim {

AgentBehavoir::AgentBehavoir(const ModuleInstanceConfig &instance, ModuleContext &context)
    : type_id_(instance.type_id), instance_id_(instance.instance_id), context_(context) {}

void AgentBehavoir::onInit() {
    context_.logger().log(LogChannel::System, "AgentBehavoir module initialized (stub).");
}

void AgentBehavoir::onTick() {
    context_.logger().log(LogChannel::System, "AgentBehavoir tick (stub).");
}

} // namespace ecosim
