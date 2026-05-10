#include "integration/test_cases.h"

#include <memory>

namespace ecosim_integration {

std::unique_ptr<IIntegrationTest> makeDependencyResolutionTest();
std::unique_ptr<IIntegrationTest> makeSpawnPhaseTest();
std::unique_ptr<IIntegrationTest> makeEventBufferingTest();
std::unique_ptr<IIntegrationTest> makeStopConditionTest();
std::unique_ptr<IIntegrationTest> makeRecorderIsolationTest();
std::unique_ptr<IIntegrationTest> makeReproducibilityTest();
std::unique_ptr<IIntegrationTest> makeScenarioParsingTest();
std::unique_ptr<IIntegrationTest> makeWorldTickContractTest();
std::unique_ptr<IIntegrationTest> makeModelDynamicsTest();
std::unique_ptr<IIntegrationTest> makeGlvDerivativesTest();
std::unique_ptr<IIntegrationTest> makeGlvEulerStepTest();
std::unique_ptr<IIntegrationTest> makeGlvRk4StepTest();
std::unique_ptr<IIntegrationTest> makeGlvJacobianTest();
std::unique_ptr<IIntegrationTest> makeGlvSetParamTest();
std::unique_ptr<IIntegrationTest> makeGlvApplyShockTest();
std::unique_ptr<IIntegrationTest> makeGlvChecksumTest();
std::unique_ptr<IIntegrationTest> makeRmDerivativesTest();
std::unique_ptr<IIntegrationTest> makeRmEulerStepTest();
std::unique_ptr<IIntegrationTest> makeRmRk4StepTest();
std::unique_ptr<IIntegrationTest> makeRmSetParamTest();
std::unique_ptr<IIntegrationTest> makeRmApplyShockTest();
std::unique_ptr<IIntegrationTest> makeRmJacobianTest();
std::unique_ptr<IIntegrationTest> makeRmEquilibriumTest();
std::unique_ptr<IIntegrationTest> makeRmChecksumTest();
std::unique_ptr<IIntegrationTest> makeModelDynamicsFactoryTest();
std::unique_ptr<IIntegrationTest> makeWorldGlvUsesDynamicsTest();
std::unique_ptr<IIntegrationTest> makeWorldRmUsesDynamicsTest();
std::unique_ptr<IIntegrationTest> makeWorldSetParamAffectsDynamicsTest();
std::unique_ptr<IIntegrationTest> makeWorldApplyShockAffectsStateTest();
std::unique_ptr<IIntegrationTest> makeWorldSpawnUsesApplyShockTest();
std::unique_ptr<IIntegrationTest> makeWorldStopAtTickTest();
std::unique_ptr<IIntegrationTest> makeWorldDeterminismWithDynamicsTest();
std::unique_ptr<IIntegrationTest> makeScenarioRunnerGlvE2eTest();
std::unique_ptr<IIntegrationTest> makeScenarioRunnerRmE2eTest();
std::unique_ptr<IIntegrationTest> makeScenarioRunnerScheduleSetParamTest();
std::unique_ptr<IIntegrationTest> makeScenarioRunnerScheduleApplyShockTest();
std::unique_ptr<IIntegrationTest> makeScenarioRunnerScheduleOrderTest();
std::unique_ptr<IIntegrationTest> makeRecorderCsvExtendedHeaderTest();
std::unique_ptr<IIntegrationTest> makeRecorderCsvExtendedRowTest();
std::unique_ptr<IIntegrationTest> makeRecorderCsvFlagsTest();
std::unique_ptr<IIntegrationTest> makeRecorderCsvLegacyEventTest();
std::unique_ptr<IIntegrationTest> makeRecorderCsvStableColumnsTest();
std::unique_ptr<IIntegrationTest> makeGlvExampleTomlToCsvTest();
std::unique_ptr<IIntegrationTest> makeRmExampleTomlToCsvTest();

std::vector<std::unique_ptr<IIntegrationTest>> buildIntegrationTests() {
    std::vector<std::unique_ptr<IIntegrationTest>> tests;
    tests.push_back(makeDependencyResolutionTest());
    tests.push_back(makeSpawnPhaseTest());
    tests.push_back(makeEventBufferingTest());
    tests.push_back(makeStopConditionTest());
    tests.push_back(makeRecorderIsolationTest());
    tests.push_back(makeReproducibilityTest());
    tests.push_back(makeScenarioParsingTest());
    tests.push_back(makeWorldTickContractTest());
    tests.push_back(makeModelDynamicsTest());
    tests.push_back(makeGlvDerivativesTest());
    tests.push_back(makeGlvEulerStepTest());
    tests.push_back(makeGlvRk4StepTest());
    tests.push_back(makeGlvJacobianTest());
    tests.push_back(makeGlvSetParamTest());
    tests.push_back(makeGlvApplyShockTest());
    tests.push_back(makeGlvChecksumTest());
    tests.push_back(makeRmDerivativesTest());
    tests.push_back(makeRmEulerStepTest());
    tests.push_back(makeRmRk4StepTest());
    tests.push_back(makeRmSetParamTest());
    tests.push_back(makeRmApplyShockTest());
    tests.push_back(makeRmJacobianTest());
    tests.push_back(makeRmEquilibriumTest());
    tests.push_back(makeRmChecksumTest());
    tests.push_back(makeModelDynamicsFactoryTest());
    tests.push_back(makeWorldGlvUsesDynamicsTest());
    tests.push_back(makeWorldRmUsesDynamicsTest());
    tests.push_back(makeWorldSetParamAffectsDynamicsTest());
    tests.push_back(makeWorldApplyShockAffectsStateTest());
    tests.push_back(makeWorldSpawnUsesApplyShockTest());
    tests.push_back(makeWorldStopAtTickTest());
    tests.push_back(makeWorldDeterminismWithDynamicsTest());
    tests.push_back(makeScenarioRunnerGlvE2eTest());
    tests.push_back(makeScenarioRunnerRmE2eTest());
    tests.push_back(makeScenarioRunnerScheduleSetParamTest());
    tests.push_back(makeScenarioRunnerScheduleApplyShockTest());
    tests.push_back(makeScenarioRunnerScheduleOrderTest());
    tests.push_back(makeRecorderCsvExtendedHeaderTest());
    tests.push_back(makeRecorderCsvExtendedRowTest());
    tests.push_back(makeRecorderCsvFlagsTest());
    tests.push_back(makeRecorderCsvLegacyEventTest());
    tests.push_back(makeRecorderCsvStableColumnsTest());
    tests.push_back(makeGlvExampleTomlToCsvTest());
    tests.push_back(makeRmExampleTomlToCsvTest());
    return tests;
}

} // namespace ecosim_integration
