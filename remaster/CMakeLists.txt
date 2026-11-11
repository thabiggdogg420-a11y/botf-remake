cmake_minimum_required(VERSION 3.20)
project(BotfRemaster LANGUAGES CXX)

# Portable build of the deterministic simulation core and its scenario suites.
# The Unreal client consumes the same sources through SimulationBridge.cpp.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BOTF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(BOTF_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(BOTF_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/game/Source/BotfSim)
set(BOTF_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/scenarios)

if(MSVC)
    set(BOTF_WARN /W4 /permissive- /utf-8)
    if(BOTF_WARNINGS_AS_ERRORS)
        list(APPEND BOTF_WARN /WX)
    endif()
else()
    set(BOTF_WARN -Wall -Wextra)
    if(BOTF_WARNINGS_AS_ERRORS)
        list(APPEND BOTF_WARN -Werror)
    endif()
endif()

if(BOTF_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Schema-13 gameplay layer modules (GAMEPLAY-REMASTER.md). Keep this list in sync
# with game/Source/BOTFRemake/Private/SimulationBridge.cpp and Build-Simulation.ps1.
set(BOTF_CORE_SOURCES
    ${BOTF_CORE_DIR}/Simulation.cpp
    ${BOTF_CORE_DIR}/Rules.cpp
    ${BOTF_CORE_DIR}/Stations.cpp
    ${BOTF_CORE_DIR}/GalaxyFeatures.cpp
    ${BOTF_CORE_DIR}/GalacticEvents.cpp
    ${BOTF_CORE_DIR}/BuildTemplates.cpp
    ${BOTF_CORE_DIR}/Victory.cpp
    ${BOTF_CORE_DIR}/Chronicle.cpp
    ${BOTF_CORE_DIR}/Threats.cpp)
add_library(botf_core STATIC ${BOTF_CORE_SOURCES})
target_include_directories(botf_core PUBLIC ${BOTF_CORE_DIR})
target_compile_options(botf_core PRIVATE ${BOTF_WARN})

add_executable(BotfSim ${BOTF_CORE_DIR}/Main.cpp)
add_library(botf_web_bridge STATIC ${CMAKE_CURRENT_SOURCE_DIR}/web/bridge/WebBridge.cpp) # Browser client bridge (remaster/web); compiled here to catch core API drift.
target_link_libraries(BotfSim PRIVATE botf_core)
target_compile_options(BotfSim PRIVATE ${BOTF_WARN})

enable_testing()

function(botf_suite name source)
    add_executable(${name} ${BOTF_TEST_DIR}/${source})
    target_link_libraries(${name} PRIVATE botf_core)
    target_compile_options(${name} PRIVATE ${BOTF_WARN})
    if(NOT MSVC)
        # Scenario suites keep diagnostic locals for readability.
        target_compile_options(${name} PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable)
    endif()
    # Suites locate optional save fixtures relative to the working directory.
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)
endfunction()

botf_suite(BotfSimTests SimulationTests.cpp)
botf_suite(BotfTacticalTests TacticalCombatTests.cpp)
botf_suite(BotfSessionTests CampaignSessionTests.cpp)
botf_suite(BotfIntelligenceTests IntelligenceTests.cpp)
botf_suite(BotfDiplomacyTests DiplomacyTests.cpp)
botf_suite(BotfRemasterGameplayTests RemasterGameplayTests.cpp)
botf_suite(BotfSoakTests SoakTests.cpp)
set_tests_properties(BotfSoakTests PROPERTIES TIMEOUT 1800)
botf_suite(BotfCampaignDiplomacyTests CampaignDiplomacyTests.cpp)
botf_suite(BotfContentTests ContentTests.cpp) # Lane G: original text + minor-race roster
botf_suite(BotfPresetTests PresetTests.cpp) # M3-PRESETS (MOD-1): overlay packs, presets, lineup validation, schema-17 PACKS section
botf_suite(BotfThreatTests ThreatTests.cpp) # M3-THREATS (MOD-5): threat actors, assimilation, schema-20 THREATS section
botf_suite(BotfGalaxyGenTests GalaxyGenTests.cpp) # M2-GEN: galaxy shapes, minor-race density, difficulty, GALAXY_SETUP save section
# Schema-14 treaty integration, client queries and battle records (Lane H).
botf_suite(BotfIntegrationTests IntegrationTests.cpp)
set_tests_properties(BotfIntegrationTests PROPERTIES TIMEOUT 900)
# Expanded ship roster (docs/SHIP-ROSTER.md): data integrity, Classic isolation, SHIP_ROSTER section, AI adoption.
botf_suite(BotfShipRosterTests ShipRosterTests.cpp)
set_tests_properties(BotfShipRosterTests PROPERTIES TIMEOUT 900)
# Empire rules (docs/design/EMPIRE-RULES.md, schema 15): dilithium, programs, aftermath, minor conquest, guards.
botf_suite(BotfEmpireRulesTests EmpireRulesTests.cpp)
set_tests_properties(BotfEmpireRulesTests PROPERTIES TIMEOUT 900)
# Empire slots (docs/design/EMPIRE-SLOTS.md, schema 17): lineups of two to eight majors, Dominion and Borg as majors, migration.
botf_suite(BotfEmpireSlotsTests EmpireSlotsTests.cpp)
set_tests_properties(BotfEmpireSlotsTests PROPERTIES TIMEOUT 900)
# Campaign eras (docs/design/ERAS.md, schema 22): era gate, timeline year, research levels 11-14, ERAS section, per-era soaks.
botf_suite(BotfEraTests EraTests.cpp)
set_tests_properties(BotfEraTests PROPERTIES TIMEOUT 1800)
