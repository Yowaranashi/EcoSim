#include "integration/test_cases.h"

#include <memory>

namespace ecosim_integration {

std::unique_ptr<IIntegrationTest> makeDependencyResolutionTest();
std::unique_ptr<IIntegrationTest> makeSpawnPhaseTest();
std::unique_ptr<IIntegrationTest> makeEventBufferingTest();
std::unique_ptr<IIntegrationTest> makeStopConditionTest();
std::unique_ptr<IIntegrationTest> makeRecorderIsolationTest();
std::unique_ptr<IIntegrationTest> makeReproducibilityTest();

std::vector<std::unique_ptr<IIntegrationTest>> buildIntegrationTests() {
    std::vector<std::unique_ptr<IIntegrationTest>> tests;
    tests.push_back(makeDependencyResolutionTest());
    tests.push_back(makeSpawnPhaseTest());
    tests.push_back(makeEventBufferingTest());
    tests.push_back(makeStopConditionTest());
    tests.push_back(makeRecorderIsolationTest());
    tests.push_back(makeReproducibilityTest());
    return tests;
}

} // namespace ecosim_integration
