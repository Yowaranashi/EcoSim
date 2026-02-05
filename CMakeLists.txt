cmake_minimum_required(VERSION 3.16)
project(EcoSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_library(ecosim_core
    src/core/app.cpp
    src/core/console.cpp
    src/core/event_bus.cpp
    src/core/logger.cpp
    src/core/module.cpp
    src/core/module_manager.cpp
    src/core/module_registry.cpp
    src/core/config.cpp
    src/core/scenario.cpp
    src/modules/scenario_runner.cpp
    src/modules/simulation_world.cpp
)

target_include_directories(ecosim_core PUBLIC src)
set_target_properties(ecosim_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(recorder_csv SHARED src/modules/recorder_csv.cpp)
target_include_directories(recorder_csv PUBLIC src)
target_link_libraries(recorder_csv PRIVATE ecosim_core)
set_target_properties(recorder_csv PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/modules/recorder
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/modules/recorder
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/modules/recorder
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/modules/recorder
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/modules/recorder
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_SOURCE_DIR}/modules/recorder
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/modules/recorder
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/modules/recorder
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/modules/recorder
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_SOURCE_DIR}/modules/recorder
)

add_executable(ecosim src/main.cpp)
target_link_libraries(ecosim PRIVATE ecosim_core)

add_executable(ecosim_test_modules_start tests/test_modules_start.cpp)
target_link_libraries(ecosim_test_modules_start PRIVATE ecosim_core)
target_include_directories(ecosim_test_modules_start PRIVATE ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME ecosim_test_modules_start COMMAND ecosim_test_modules_start)

add_executable(ecosim_test_start_order tests/test_start_order.cpp)
target_link_libraries(ecosim_test_start_order PRIVATE ecosim_core)
target_include_directories(ecosim_test_start_order PRIVATE ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME ecosim_test_start_order COMMAND ecosim_test_start_order)

add_executable(ecosim_test_event_delivery tests/test_event_delivery.cpp)
target_link_libraries(ecosim_test_event_delivery PRIVATE ecosim_core)
target_include_directories(ecosim_test_event_delivery PRIVATE ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME ecosim_test_event_delivery COMMAND ecosim_test_event_delivery)

add_executable(ecosim_test_scenario_results tests/test_scenario_results.cpp)
target_link_libraries(ecosim_test_scenario_results PRIVATE ecosim_core)
target_include_directories(ecosim_test_scenario_results PRIVATE ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME ecosim_test_scenario_results COMMAND ecosim_test_scenario_results)

include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Multi-config generator detected; use --config <cfg> for ctest and install (e.g. Debug).")
endif()

install(TARGETS ecosim
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY configs/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/configs
)

install(DIRECTORY modules/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/modules
)

install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/docs
    OPTIONAL
)

install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(CPACK_PACKAGE_NAME "EcoSim")
set(CPACK_PACKAGE_VENDOR "EcoSim")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_CONTACT "ecosim@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "EcoSim headless simulation prototype")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_PACKAGE_FILE_NAME "EcoSim-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")
include(CPack)
