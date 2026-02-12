#pragma once

#include "integration/test_framework.h"

#include <memory>
#include <vector>

namespace ecosim_integration {

std::vector<std::unique_ptr<IIntegrationTest>> buildIntegrationTests();

} // namespace ecosim_integration
