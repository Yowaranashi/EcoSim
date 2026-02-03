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
    src/modules/recorder_csv.cpp
    src/modules/scenario_runner.cpp
    src/modules/simulation_world.cpp
)

target_include_directories(ecosim_core PUBLIC src)

add_executable(ecosim src/main.cpp)
target_link_libraries(ecosim PRIVATE ecosim_core)

add_executable(ecosim_tests tests/integration_tests.cpp)
target_link_libraries(ecosim_tests PRIVATE ecosim_core)
target_compile_definitions(ecosim_tests PRIVATE ECOSIM_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")

add_test(NAME ecosim_integration_tests COMMAND ecosim_tests)

include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

install(TARGETS ecosim
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY configs/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ecosim/configs
)

install(DIRECTORY modules/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ecosim/modules
)

install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ecosim/docs
    OPTIONAL
)

install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ecosim
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
