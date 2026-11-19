// F5 round 2 regression (20 Nov 2026): the Dominion War memory runaway.
// The fixture tests/fixtures/dominion-war-7655-turn61.sav is the AI soak's dominion-war campaign (seed 7655) saved before
// turn 62. Its advance used to grow past 19 GB: threats::battles picked the same empire for a threat-actor duel again and
// again when that empire's ships neither died nor withdrew (the duel ran to its round limit). Each empire now meets an actor
// once a turn. This suite bounds time, battles and state size per turn on that save, both with the player seat passive (as the
// soak ran) and in observer mode (the AI plays every seat) out past turn 100.
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "Simulation.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << std::endl; }
}

// Peak working set in MB where the platform reports it; 0 elsewhere (the battle and size bounds still apply).
std::uint64_t peakMegabytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return pmc.PeakWorkingSetSize / (1024 * 1024);
#endif
    return 0;
}

constexpr double MaxTurnSeconds = 5.0;      // The fixed turns take ~0.03 s in Release; debug builds stay well inside this.
constexpr std::size_t MaxBattlesPerTurn = 64;
constexpr std::size_t MaxFleets = 2000;
constexpr std::uint64_t MaxPeakMegabytes = 1024;

// A runaway grows inside one advance(), where the per-turn checks never get control: a watchdog thread fails the suite
// the moment the process passes the memory bound or one advance runs over 30 s.
std::atomic<long long> turnStartMs{0};
std::atomic<bool> done{false};
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void watchdog() {
    while (!done) {
        const auto peak = peakMegabytes();
        const long long started = turnStartMs;
        if (peak > MaxPeakMegabytes || (started && nowMs() - started > 30000)) {
            std::cerr << "FAIL: runaway inside advance(): peak " << peak << " MB, turn running " << (started ? (nowMs() - started) / 1000 : 0) << " s" << std::endl;
            std::_Exit(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void runFrom(const botf::GameState& saved, bool observer, int untilTurn, const char* label) {
    botf::GameState state = saved;
    state.observerMode = observer;
    int last = state.turn;
    while (state.turn < untilTurn && state.outcome == botf::CampaignOutcome::Ongoing) {
        const auto t0 = std::chrono::steady_clock::now();
        turnStartMs = nowMs();
        const auto report = botf::advance(state);
        turnStartMs = 0;
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const std::string at = std::string(label) + " turn " + std::to_string(state.turn);
        check(secs < MaxTurnSeconds, at + ": advance took " + std::to_string(secs) + " s");
        check(report.battles.size() <= MaxBattlesPerTurn, at + ": " + std::to_string(report.battles.size()) + " battles in one turn");
        check(state.fleets.size() <= MaxFleets, at + ": " + std::to_string(state.fleets.size()) + " fleets");
        check(botf::serialize(state).size() <= botf::MaxSaveBytes, at + ": save over MaxSaveBytes");
        const auto peak = peakMegabytes();
        check(peak <= MaxPeakMegabytes, at + ": peak working set " + std::to_string(peak) + " MB");
        if (failures) return;
        last = state.turn;
    }
    std::cout << label << ": reached turn " << last << ", outcome " << static_cast<int>(state.outcome) << ", peak " << peakMegabytes() << " MB" << std::endl;
    if (observer) check(last >= 100, std::string(label) + ": stopped at turn " + std::to_string(last) + " (want 100+)");
}
}

int main() {
    std::ifstream in("dominion-war-7655-turn61.sav", std::ios::binary);
    if (!in) { std::cerr << "FAIL: fixture dominion-war-7655-turn61.sav not found (run from tests/fixtures)" << std::endl; return 1; }
    std::stringstream text;
    text << in.rdbuf();
    botf::GameState saved;
    try {
        saved = botf::deserialize(text.str());
    } catch (const std::exception& e) {
        std::cerr << "FAIL: fixture does not load: " << e.what() << std::endl;
        return 1;
    }
    check(saved.turn == 61, "fixture is turn 61");
    std::thread guard(watchdog);
    try {
        runFrom(saved, false, 400, "passive seat");
        runFrom(saved, true, 120, "observer");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: advance threw: " << e.what() << std::endl;
        std::_Exit(1);
    }
    done = true;
    guard.join();
    if (failures) { std::cerr << failures << " failure(s)" << std::endl; return 1; }
    std::cout << "BotfRunawayTests: all passed" << std::endl;
    return 0;
}
