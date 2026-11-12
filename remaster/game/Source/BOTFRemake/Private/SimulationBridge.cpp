// Compile the SAME engine-independent implementation as the standalone runner.
// No PCH/unity includes here: Unreal macros must not change the core's semantics.
#include "../../BotfSim/Simulation.cpp"
// Schema-13 gameplay layer (keep in sync with BOTF_CORE_SOURCES in remaster/CMakeLists.txt).
#include "../../BotfSim/Rules.cpp"
#include "../../BotfSim/Stations.cpp"
#include "../../BotfSim/GalaxyFeatures.cpp"
#include "../../BotfSim/GalacticEvents.cpp"
#include "../../BotfSim/BuildTemplates.cpp"
#include "../../BotfSim/Victory.cpp"
#include "../../BotfSim/Chronicle.cpp"
#include "../../BotfSim/Threats.cpp"
#include "../../BotfSim/Scenarios.cpp"
