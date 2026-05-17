cmake_minimum_required(VERSION 3.16)
project(EcoSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_library(ecosim_models
    src/models/glv_dynamics.cpp
    src/models/model_dynamics_factory.cpp
    src/models/model_dynamics_base.cpp
    src/models/rosenzweig_macarthur_dynamics.cpp
)
target_include_directories(ecosim_models PUBLIC src)

add_library(ecosim_viewer_csv STATIC
    src/viewer/csv_result_reader.cpp
)
target_include_directories(ecosim_viewer_csv PUBLIC src)

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
    src/core/utils/toml_parser.cpp
    src/modules/agent_behavoir.cpp
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

add_library(recorder_csv_static STATIC src/modules/recorder_csv.cpp)
target_include_directories(recorder_csv_static PUBLIC src)
target_link_libraries(recorder_csv_static PRIVATE ecosim_core)

add_executable(ecosim src/main.cpp)
target_link_libraries(ecosim PRIVATE ecosim_core ecosim_models)

add_executable(ecosim_integration_tests
    tests/integration/run_integration_tests.cpp
    tests/integration/test_framework.cpp
    tests/integration/test_cases.cpp
    tests/integration/test_1_dependency_resolution.cpp
    tests/integration/test_2_spawn_phase.cpp
    tests/integration/test_3_event_buffering.cpp
    tests/integration/test_4_stop_condition.cpp
    tests/integration/test_5_recorder_isolation.cpp
    tests/integration/test_6_reproducibility.cpp
    tests/integration/test_7_scenario_parsing.cpp
    tests/integration/test_8_world_tick_contract.cpp
    tests/integration/test_9_model_dynamics.cpp
    tests/integration/test_10_glv_derivatives.cpp
    tests/integration/test_11_glv_euler_step.cpp
    tests/integration/test_12_glv_rk4_step.cpp
    tests/integration/test_13_glv_jacobian.cpp
    tests/integration/test_14_glv_set_param.cpp
    tests/integration/test_15_glv_apply_shock.cpp
    tests/integration/test_16_glv_checksum.cpp
    tests/integration/test_17_rm_derivatives.cpp
    tests/integration/test_18_rm_euler_step.cpp
    tests/integration/test_19_rm_rk4_step.cpp
    tests/integration/test_20_rm_set_param.cpp
    tests/integration/test_21_rm_apply_shock.cpp
    tests/integration/test_22_rm_jacobian.cpp
    tests/integration/test_23_rm_equilibrium.cpp
    tests/integration/test_24_rm_checksum.cpp
    tests/integration/test_25_model_dynamics_factory.cpp
    tests/integration/test_26_world_glv_uses_dynamics.cpp
    tests/integration/test_27_world_rm_uses_dynamics.cpp
    tests/integration/test_28_world_set_param_affects_dynamics.cpp
    tests/integration/test_29_world_apply_shock_affects_state.cpp
    tests/integration/test_30_world_spawn_uses_apply_shock.cpp
    tests/integration/test_31_world_stop_at_tick.cpp
    tests/integration/test_32_world_determinism_with_dynamics.cpp
    tests/integration/test_33_scenario_runner_e2e.cpp
    tests/integration/test_34_recorder_csv_extended.cpp
    tests/integration/test_35_examples_toml_to_csv.cpp
    tests/integration/test_36_csv_result_reader.cpp
    tests/integration/test_37_app_console_scenarios.cpp
    tests/integration/test_38_extinction_noise.cpp
    tests/integration/test_39_sensitivity.cpp
)
target_link_libraries(ecosim_integration_tests PRIVATE ecosim_core recorder_csv_static ecosim_models ecosim_viewer_csv)
target_include_directories(ecosim_integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME ecosim_integration_tests COMMAND $<TARGET_FILE:ecosim_integration_tests>)
set_tests_properties(ecosim_integration_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

option(ECOSIM_BUILD_OGRE_VIEWER "Build OGRE result viewer" OFF)

if(ECOSIM_BUILD_OGRE_VIEWER)
    find_package(OGRE QUIET)

    if(OGRE_FOUND)
        target_compile_definitions(ecosim_core PUBLIC ECOSIM_OGRE_VIEWER_AVAILABLE=1)

        add_executable(ecosim_ogre_viewer
            src/viewer/main_ogre_viewer.cpp
            src/viewer/ogre_ecosystem_viewer.cpp
        )

        target_include_directories(ecosim_ogre_viewer PRIVATE src)
        target_link_libraries(ecosim_ogre_viewer PRIVATE ecosim_viewer_csv)

        if(TARGET OGRE::OgreMain)
            target_link_libraries(ecosim_ogre_viewer PRIVATE OGRE::OgreMain)
        elseif(TARGET OgreMain)
            target_link_libraries(ecosim_ogre_viewer PRIVATE OgreMain)
        elseif(TARGET OGRE::Main)
            target_link_libraries(ecosim_ogre_viewer PRIVATE OGRE::Main)
        endif()

        if(OGRE_INCLUDE_DIRS)
            target_include_directories(ecosim_ogre_viewer PRIVATE ${OGRE_INCLUDE_DIRS})
        endif()
        if(OGRE_LIBRARIES)
            target_link_libraries(ecosim_ogre_viewer PRIVATE ${OGRE_LIBRARIES})
        endif()

        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/ogre_plugins.cfg.in"
            "${CMAKE_BINARY_DIR}/ogre_plugins.cfg"
            COPYONLY
        )

        if(OGRE_PLUGIN_DIR)
            add_custom_command(TARGET ecosim_ogre_viewer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${OGRE_PLUGIN_DIR}"
                    "$<TARGET_FILE_DIR:ecosim_ogre_viewer>/plugins/ogre"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_BINARY_DIR}/ogre_plugins.cfg"
                    "$<TARGET_FILE_DIR:ecosim_ogre_viewer>/plugins.cfg"
                VERBATIM
            )
        endif()

        if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
            set(ECOSIM_VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
            if(EXISTS "${ECOSIM_VCPKG_BIN_DIR}")
                add_custom_command(TARGET ecosim_ogre_viewer POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${ECOSIM_VCPKG_BIN_DIR}"
                        "$<TARGET_FILE_DIR:ecosim_ogre_viewer>"
                    VERBATIM
                )
            endif()
        endif()
    else()
        message(WARNING "OGRE not found: ecosim_ogre_viewer will not be built")
    endif()
endif()

include(GNUInstallDirs)
include(InstallRequiredSystemLibraries)

if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Multi-config generator detected; use --config <cfg> for ctest and install (e.g. Debug).")
endif()

function(ecosim_copy_runtime_payload target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/configs"
            "$<TARGET_FILE_DIR:${target_name}>/configs"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/scenarios"
            "$<TARGET_FILE_DIR:${target_name}>/scenarios"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/modules"
            "$<TARGET_FILE_DIR:${target_name}>/modules"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/docs"
            "$<TARGET_FILE_DIR:${target_name}>/docs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/README.md"
            "$<TARGET_FILE_DIR:${target_name}>/README.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE"
            "$<TARGET_FILE_DIR:${target_name}>/LICENSE"
        VERBATIM
    )

    foreach(runtime_file IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${runtime_file}"
                "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM
        )
    endforeach()
endfunction()

add_dependencies(ecosim recorder_csv)
add_dependencies(ecosim_integration_tests recorder_csv)
ecosim_copy_runtime_payload(ecosim)
ecosim_copy_runtime_payload(ecosim_integration_tests)

if(TARGET ecosim_ogre_viewer)
    ecosim_copy_runtime_payload(ecosim_ogre_viewer)
endif()

install(TARGETS ecosim
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET ecosim_ogre_viewer)
    install(TARGETS ecosim_ogre_viewer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    if(OGRE_PLUGIN_DIR)
        install(DIRECTORY "${OGRE_PLUGIN_DIR}/"
            DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins/ogre
            FILES_MATCHING PATTERN "*.dll"
        )
        install(FILES "${CMAKE_BINARY_DIR}/ogre_plugins.cfg"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            RENAME plugins.cfg
        )
    endif()
    if(DEFINED ECOSIM_VCPKG_BIN_DIR AND EXISTS "${ECOSIM_VCPKG_BIN_DIR}")
        install(DIRECTORY "${ECOSIM_VCPKG_BIN_DIR}/"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            FILES_MATCHING PATTERN "*.dll"
        )
    endif()
endif()

install(DIRECTORY configs/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/configs
)

install(DIRECTORY scenarios/
    DESTINATION ${CMAKE_INSTALL_BINDIR}/scenarios
    OPTIONAL
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
