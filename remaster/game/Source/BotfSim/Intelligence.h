#pragma once

// =============================================================================
// BOTF Intelligence Subsystem (Modern Reconstruction / Beta Implementation)
// =============================================================================
//
// Documentation & Fidelity References:
// - research/BETA-FIDELITY.md (§ Intelligence allocation, internal security, and attack process)
// - Original BOTF Tutorial (1999), pp. 29–31 (9 allocations, department focus odds)
// - Community Reverse Engineering: Spocks-cuddly-tribble & Gowron (Armada Fleet Command S16)
//
// Mechanistic Classification:
// - VERIFIED (S16 / Manual): 0..100 Internal Security + per-rival Espionage/Sabotage totaling 100.
//   Maximum 4 rivals in a standard 5-empire galaxy.
// - VERIFIED (S16 / Manual): Foreign sliders lockable; Internal Security adjusted directly;
//   foreign redistribution.
// - VERIFIED (S16): Attack frequency (stockpile vs threshold) strictly separated from attack success.
//   Stockpile cleared upon attempt.
// - VERIFIED (S16): Attack success combines up to the last 20 turns of Espionage + Sabotage output
//   against the target divided by the defender counter-intelligence defense denominator:
//   defenseDenominator = ((last 20 security outputs / 20) + (securityStockpile * 0.02)) * defenderMorale.
// - MODERN REBALANCE: the attack numerator is the same 20-turn window divided by 20 (an average, like
//   the defense) times a 2x concentration factor, so both sides are compared per turn of investment.
//   The raw S16 sum made a 20% attacker beat an 80% defender 11 times in 12; now parity needs a
//   per-rival share of half the defender's security share, and security investment decides.
// - MODERN: every outcome is computed in integer fixed point (strengths in milli-points, ratios and
//   rolls in permille), so results are exact and identical on every compiler.
// - MODERN: counter-intelligence exposes failed operations with a chance that grows with the
//   defense/attack ratio (not only catastrophic fiascos). Exposed failures are reported as Fiasco.
// - VERIFIED (S16): Zero-allocation areas contribute -2% of total intelligence output to rolling history
//   for success calculation, and decay stockpile by 2% of total output (clamped to 0).
// - VERIFIED (S16): Internal Security stockpile is a running weighted average across turns:
//   (oldStockpile * turnCounter + currentSecurityOutput) / (turnCounter + 1).
// - VERIFIED (S16): Proportional bonus bug fix vs legacy parity (1000 base * 40% * 1.5 = 600).
// - VERIFIED (S16): Department preference counters range 1..30; General focus adds +2 to ALL 4 departments;
//   targeted focus adds +2 to selected and -2 to others; weighted draw uses actual counter sum.
// - PROVISIONAL / MODERN RECONSTRUCTION: Base readiness threshold (250), attack delay (5 turns),
//   treaty modifiers, credit/sabotage scaling, and exact morale mapping (default 100%).
//
// Architectural Invariants:
// - Self-contained header; independent of Simulation.h (no circular dependencies).
// - Bounded arithmetic adhering to MaxIntelBalance (10^12) and MaxIntelTurns (10^6).
// - Returns immutable OperationRecord objects for atomic external application; no GameState mutation.
// =============================================================================

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace botf::intelligence {

// -----------------------------------------------------------------------------
// Core Bounded Constants (Consistent with Simulation.h / Simulation.cpp)
// -----------------------------------------------------------------------------

constexpr int64_t MaxIntelBalance = 1000000000000LL; // 1 trillion (10^12)
constexpr int MaxIntelTurns = 1000000;               // 1 million turns
constexpr int MinLastAttackTurn = -100;              // S16 unattacked sentinel (-100)

// -----------------------------------------------------------------------------
// Enums & Basic Types
// -----------------------------------------------------------------------------

enum class IntelArea : int {
    InternalSecurity = 0,
    Espionage = 1,
    Sabotage = 2
};

inline const char* intelAreaName(IntelArea area) {
    switch (area) {
        case IntelArea::InternalSecurity: return "Internal Security";
        case IntelArea::Espionage: return "Espionage";
        case IntelArea::Sabotage: return "Sabotage";
        default: return "Unknown";
    }
}

enum class IntelDepartment : int {
    General = 0,
    Economic = 1,
    Science = 2,
    Military = 3
};

inline const char* departmentName(IntelDepartment dept) {
    switch (dept) {
        case IntelDepartment::General: return "General";
        case IntelDepartment::Economic: return "Economic";
        case IntelDepartment::Science: return "Science";
        case IntelDepartment::Military: return "Military";
        default: return "Unknown";
    }
}

enum class IntelOutcome : int {
    UndetectedFailure = 0, // Infiltration halted by counter-intelligence; defender unaware
    Fiasco = 1,            // Operation catastrophically failed; operatives captured/exposed
    MinorSuccess = 2,      // Low-level intel or minor disruption
    StandardSuccess = 3,   // Objectives achieved (credits, data, sabotage)
    MajorSuccess = 4       // Critical breach, high yield
};

inline const char* outcomeName(IntelOutcome outcome) {
    switch (outcome) {
        case IntelOutcome::UndetectedFailure: return "Failure (Undetected)";
        case IntelOutcome::Fiasco: return "Fiasco (Detected)";
        case IntelOutcome::MinorSuccess: return "Minor Success";
        case IntelOutcome::StandardSuccess: return "Standard Success";
        case IntelOutcome::MajorSuccess: return "Major Success";
        default: return "Unknown";
    }
}

enum class IntelOpType : int {
    None = 0,
    GatherIntel = 1,
    StealCredits = 2,
    StealResearch = 3,
    SabotageFacility = 4
};

enum class BonusModel : int {
    // Corrected proportional behavior: base * (alloc / 100.0) * (1.0 + bonus / 100.0)
    ProportionalCorrected = 0,
    // Vanilla bug reproduction: nonzero slider receives bonus as if allocated 100%
    LegacyAdditiveParity = 1
};

enum class TreatyLevel : int {
    War = 0,
    None = 1,
    NonAggression = 2,
    Friendship = 3,
    Affiliation = 4,
    Alliance = 5
};

// -----------------------------------------------------------------------------
// Rules Configuration
// -----------------------------------------------------------------------------

struct IntelligenceRules {
    // Model for building/tech bonus application
    BonusModel bonusModel = BonusModel::ProportionalCorrected;

    // Attack frequency thresholds (PROVISIONAL)
    int64_t baseAttackThreshold = 250;
    int attackDelayTurns = 5;
    int64_t delayMultiplier = 30;

    // Verified decay: 2% of total intelligence output per turn for zero-allocated areas
    int decayRatePercent = 2;

    // S16 Source: defense strength denominator uses (last 20 security outputs / 20) + (securityStockpile * 0.02)
    int securityHistoryDivisor = 20;
    int securityStockpilePermille = 20; // S16: 0.02 multiplier on security stockpile, in permille

    // PROVISIONAL: Default defender morale when unspecified, in percent (100 = 1.0x)
    int defaultDefenderMoralePercent = 100;

    // MODERN: counter-intelligence exposure odds for failed operations (percent).
    int minFailureDetectionPercent = 10;
    int maxFailureDetectionPercent = 90;

    // MODERN REBALANCE: a concentrated operation against one rival counts double
    // against a security service that must cover the whole empire. Parity is thus
    // reached when the attacker's per-rival share is half the defender's security share.
    int attackConcentrationPercent = 200;

    // History depth (VERIFIED: community reverse-engineering S16)
    int maxHistoryTurns = 20;

    // Sourced department training parameters (S16: range 1..30)
    int minDepartmentCounter = 1;
    int maxDepartmentCounter = 30;
    int departmentFocusBonus = 2;     // Selected +2 (or all +2 for General)
    int departmentOtherDeduction = 2; // Others -2 for targeted focus

    // Treaty threshold multipliers (PROVISIONAL)
    int warThresholdModifier = -50;       // Easier to justify attacks during war
    int nonAggressionThresholdModifier = 50;
    int friendshipThresholdModifier = 100;
    int allianceThresholdModifier = 200;
};

// -----------------------------------------------------------------------------
// Budget Allocation Structures & Rebalancer
// -----------------------------------------------------------------------------

struct AllocationEntry {
    int percentage = 0;
    bool isLocked = false;

    constexpr bool operator==(const AllocationEntry& other) const = default;
};

struct RivalAllocation {
    int rivalId = 0;
    AllocationEntry espionage{0, false};
    AllocationEntry sabotage{0, false};

    constexpr bool operator==(const RivalAllocation& other) const = default;
};

class BudgetAllocations {
public:
    AllocationEntry internalSecurity{100, false};
    std::vector<RivalAllocation> rivalAllocations;

    constexpr bool operator==(const BudgetAllocations& other) const = default;

    int totalPercentage() const {
        int sum = internalSecurity.percentage;
        for (const auto& r : rivalAllocations) {
            sum += r.espionage.percentage;
            sum += r.sabotage.percentage;
        }
        return sum;
    }

    bool isValid(int ownEmpireId = 0) const {
        if (internalSecurity.percentage < 0 || internalSecurity.percentage > 100) return false;
        if (rivalAllocations.size() > 4) return false; // Maximum 4 rivals
        int sum = internalSecurity.percentage;
        std::vector<int> seenIds;
        for (const auto& r : rivalAllocations) {
            if (r.rivalId <= 0) return false;
            if (ownEmpireId > 0 && r.rivalId == ownEmpireId) return false;
            if (std::find(seenIds.begin(), seenIds.end(), r.rivalId) != seenIds.end()) return false;
            seenIds.push_back(r.rivalId);

            if (r.espionage.percentage < 0 || r.espionage.percentage > 100) return false;
            if (r.sabotage.percentage < 0 || r.sabotage.percentage > 100) return false;
            sum += r.espionage.percentage;
            sum += r.sabotage.percentage;
        }
        return sum == 100;
    }

    // Default sensible allocation: 80% Internal Security, remaining 20% distributed
    // across known valid rivals (espionage & sabotage), or 100% internal if no rivals.
    void setDefault(const std::vector<int>& knownRivals, int ownEmpireId = 0) {
        rivalAllocations.clear();
        std::vector<int> validRivals;
        for (int id : knownRivals) {
            if (id > 0 && (ownEmpireId == 0 || id != ownEmpireId) &&
                std::find(validRivals.begin(), validRivals.end(), id) == validRivals.end()) {
                validRivals.push_back(id);
                if (validRivals.size() >= 4) break; // Maximum 4 rivals
            }
        }
        std::sort(validRivals.begin(), validRivals.end());

        if (validRivals.empty()) {
            internalSecurity.percentage = 100;
            internalSecurity.isLocked = false;
            return;
        }

        internalSecurity.percentage = 80;
        internalSecurity.isLocked = false;

        int foreignSlots = static_cast<int>(validRivals.size()) * 2;
        int remaining = 20;
        int basePerSlot = remaining / foreignSlots;
        int remainder = remaining % foreignSlots;

        for (int id : validRivals) {
            RivalAllocation r;
            r.rivalId = id;
            r.espionage.percentage = basePerSlot;
            r.espionage.isLocked = false;
            r.sabotage.percentage = basePerSlot;
            r.sabotage.isLocked = false;
            rivalAllocations.push_back(r);
        }

        // Distribute remainder deterministically
        int slotIdx = 0;
        while (remainder > 0) {
            int rivalIdx = slotIdx / 2;
            if (slotIdx % 2 == 0) {
                rivalAllocations[rivalIdx].espionage.percentage += 1;
            } else {
                rivalAllocations[rivalIdx].sabotage.percentage += 1;
            }
            remainder--;
            slotIdx++;
        }
    }

    // Directly adjust Internal Security percentage.
    // Predictable redistribution: foreign unlocked allocations absorb the delta.
    // If budget cannot be balanced, returns false and leaves state unmodified.
    bool setInternalSecurity(int newPct) {
        if (newPct < 0 || newPct > 100) return false;
        if (newPct == internalSecurity.percentage) return true;

        int delta = newPct - internalSecurity.percentage;
        auto copy = *this;
        copy.internalSecurity.percentage = newPct;

        std::vector<AllocationEntry*> unlockedSlots;
        for (auto& r : copy.rivalAllocations) {
            if (!r.espionage.isLocked) unlockedSlots.push_back(&r.espionage);
            if (!r.sabotage.isLocked) unlockedSlots.push_back(&r.sabotage);
        }

        if (unlockedSlots.empty()) {
            return false;
        }

        if (!distributeDelta(unlockedSlots, -delta)) {
            return false;
        }

        if (!copy.isValid()) return false;
        *this = copy;
        return true;
    }

    // Adjust a specific foreign allocation (Espionage or Sabotage).
    // Per tutorial: Internal Security is left unchanged; other unlocked foreign sliders absorb the delta.
    bool setRivalAllocation(int rivalId, IntelArea area, int newPct) {
        if (area == IntelArea::InternalSecurity) {
            return setInternalSecurity(newPct);
        }
        if (newPct < 0 || newPct > 100) return false;

        auto copy = *this;
        AllocationEntry* targetEntry = nullptr;
        for (auto& r : copy.rivalAllocations) {
            if (r.rivalId == rivalId) {
                if (area == IntelArea::Espionage) targetEntry = &r.espionage;
                else if (area == IntelArea::Sabotage) targetEntry = &r.sabotage;
                break;
            }
        }
        if (!targetEntry) return false;
        if (targetEntry->percentage == newPct) return true;

        int delta = newPct - targetEntry->percentage;
        targetEntry->percentage = newPct;

        std::vector<AllocationEntry*> otherUnlocked;
        for (auto& r : copy.rivalAllocations) {
            if (r.rivalId == rivalId) {
                if (area == IntelArea::Espionage && !r.sabotage.isLocked) {
                    otherUnlocked.push_back(&r.sabotage);
                } else if (area == IntelArea::Sabotage && !r.espionage.isLocked) {
                    otherUnlocked.push_back(&r.espionage);
                }
            } else {
                if (!r.espionage.isLocked) otherUnlocked.push_back(&r.espionage);
                if (!r.sabotage.isLocked) otherUnlocked.push_back(&r.sabotage);
            }
        }

        if (otherUnlocked.empty()) {
            return false;
        }

        if (!distributeDelta(otherUnlocked, -delta)) {
            return false;
        }

        if (!copy.isValid()) return false;
        *this = copy;
        return true;
    }

    bool setLock(int rivalId, IntelArea area, bool locked) {
        if (area == IntelArea::InternalSecurity) {
            internalSecurity.isLocked = locked;
            return true;
        }
        for (auto& r : rivalAllocations) {
            if (r.rivalId == rivalId) {
                if (area == IntelArea::Espionage) r.espionage.isLocked = locked;
                else if (area == IntelArea::Sabotage) r.sabotage.isLocked = locked;
                return true;
            }
        }
        return false;
    }

private:
    static bool distributeDelta(std::vector<AllocationEntry*>& entries, int deltaToDistribute) {
        if (deltaToDistribute == 0) return true;

        int step = (deltaToDistribute > 0) ? 1 : -1;
        int remaining = std::abs(deltaToDistribute);

        while (remaining > 0) {
            bool changed = false;
            for (auto* entry : entries) {
                if (step > 0 && entry->percentage < 100) {
                    entry->percentage += 1;
                    remaining--;
                    changed = true;
                    if (remaining == 0) break;
                } else if (step < 0 && entry->percentage > 0) {
                    entry->percentage -= 1;
                    remaining--;
                    changed = true;
                    if (remaining == 0) break;
                }
            }
            if (!changed) {
                return false;
            }
        }
        return true;
    }
};

// -----------------------------------------------------------------------------
// Intel State Structures & Rolling History
// -----------------------------------------------------------------------------

struct RivalIntelState {
    int rivalId = 0;
    int64_t espionageStockpile = 0;
    int64_t sabotageStockpile = 0;
    int lastEspionageAttackTurn = MinLastAttackTurn;
    int lastSabotageAttackTurn = MinLastAttackTurn;

    // Rolling output history (up to 20 turns, signed for negative decay contributions)
    std::vector<int64_t> espionageHistory;
    std::vector<int64_t> sabotageHistory;

    // S16 reverse engineering: Department training counters range 1..30 (default 10)
    std::array<int, 4> departmentWeights{{10, 10, 10, 10}};
    IntelDepartment focusDepartment = IntelDepartment::General;

    constexpr bool operator==(const RivalIntelState& other) const = default;

    int64_t getRollingEspionageSum() const {
        return std::accumulate(espionageHistory.begin(), espionageHistory.end(), int64_t{0});
    }

    int64_t getRollingSabotageSum() const {
        return std::accumulate(sabotageHistory.begin(), sabotageHistory.end(), int64_t{0});
    }

    // VERIFIED S16: Combined attack strength against target combines both Espionage and Sabotage histories.
    // Signed history sum is strictly preserved; probability clamping belongs at resolution.
    int64_t getCombinedRollingAttackSum() const {
        return getRollingEspionageSum() + getRollingSabotageSum();
    }

    // MODERN REBALANCE: per-turn average of the combined window in milli-points
    // (sum * 1000 / window), directly comparable with calculateDefenseStrength().
    int64_t getAverageAttackStrengthMilli(int window) const {
        const int64_t w = std::max(1, window);
        return (getCombinedRollingAttackSum() * 1000) / w;
    }

    void recordEspionageOutput(int64_t output, int maxHistory) {
        espionageHistory.push_back(output);
        if (static_cast<int>(espionageHistory.size()) > maxHistory) {
            espionageHistory.erase(espionageHistory.begin());
        }
    }

    void recordSabotageOutput(int64_t output, int maxHistory) {
        sabotageHistory.push_back(output);
        if (static_cast<int>(sabotageHistory.size()) > maxHistory) {
            sabotageHistory.erase(sabotageHistory.begin());
        }
    }
};

struct EmpireIntelState {
    int empireId = 1;
    BudgetAllocations budget;

    // Running weighted average for internal security stockpile
    int64_t internalSecurityStockpile = 0;
    int securityTurnCounter = 0; // Number of turns contributing to internalSecurityStockpile average
    std::vector<int64_t> internalSecurityHistory; // Up to 20 turns

    std::vector<RivalIntelState> rivalStates;

    constexpr bool operator==(const EmpireIntelState& other) const = default;

    int64_t getRollingSecuritySum() const {
        return std::accumulate(internalSecurityHistory.begin(), internalSecurityHistory.end(), int64_t{0});
    }

    void recordSecurityOutput(int64_t output, int maxHistory) {
        internalSecurityHistory.push_back(output);
        if (static_cast<int>(internalSecurityHistory.size()) > maxHistory) {
            internalSecurityHistory.erase(internalSecurityHistory.begin());
        }
    }

    RivalIntelState* getRivalState(int rivalId) {
        for (auto& r : rivalStates) {
            if (r.rivalId == rivalId) return &r;
        }
        return nullptr;
    }

    const RivalIntelState* getRivalState(int rivalId) const {
        for (const auto& r : rivalStates) {
            if (r.rivalId == rivalId) return &r;
        }
        return nullptr;
    }

    RivalIntelState& getOrCreateRivalState(int rivalId) {
        for (auto& r : rivalStates) {
            if (r.rivalId == rivalId) return r;
        }
        RivalIntelState s;
        s.rivalId = rivalId;
        rivalStates.push_back(s);
        return rivalStates.back();
    }

    void initializeDefault(const std::vector<int>& knownRivals) {
        budget.setDefault(knownRivals, empireId);
        rivalStates.clear();
        for (const auto& rAlloc : budget.rivalAllocations) {
            getOrCreateRivalState(rAlloc.rivalId);
        }
    }
};

// -----------------------------------------------------------------------------
// Operation Record (Result returned atomically to caller)
// -----------------------------------------------------------------------------

struct OperationRecord {
    int attackerEmpireId = 0;
    int defenderEmpireId = 0;
    IntelArea area = IntelArea::Espionage;
    IntelDepartment department = IntelDepartment::General;
    IntelOutcome outcome = IntelOutcome::UndetectedFailure;
    IntelOpType opType = IntelOpType::None;
    int turn = 0;

    // Outcomes data
    int64_t creditsStolen = 0;
    int64_t researchStolen = 0;
    int sabotageTargetId = 0; // Facility or system reference ID
    int sabotageSeverity = 0; // 0=none, 1=minor, 2=standard, 3=severe

    // Diplomatic & attribution impact
    bool detectedByDefender = false;
    int diplomaticIncidentPenalty = 0; // Negative trust delta if fiasco/detected

    std::string details;

    constexpr bool operator==(const OperationRecord& other) const = default;
};

// -----------------------------------------------------------------------------
// Turn Resolution Context
// -----------------------------------------------------------------------------

struct RivalContext {
    int rivalId = 0;
    int computerTechLevel = 0; // Fallback if DefenderSecurityContext is not provided
    TreatyLevel treaty = TreatyLevel::None;
    int difficultyModifier = 0;
    int64_t currentCredits = 0;
    int64_t currentResearchReserve = 0; // Research points a theft can reach (reserve + unspent progress)
};

// Authoritative defender counter-intelligence state
struct DefenderSecurityContext {
    int64_t rollingSecuritySum = 0;
    int64_t securityStockpile = 0;
    int computerTechLevel = 0;
    // Sourced defense morale multiplier in percent. Exact curve is PROVISIONAL (default 100 = 1.0x).
    int defenderMoralePercent = 100;
};

struct TurnContext {
    int currentTurn = 1;
    int attackerComputerTechLevel = 0;
    int64_t totalEmpireBaseIntelOutput = 0;
    int buildingBonusPercent = 0; // e.g. 50 for +50%
    std::vector<RivalContext> knownRivals;
};

// -----------------------------------------------------------------------------
// Engine Logic & Calculations
// -----------------------------------------------------------------------------

// Calculate area output given total base intelligence, allocation percentage, and building bonus.
// Proportional Corrected: base * alloc * (100 + bonus) / 10000   (exact integer floor)
// Example: 1000 base * 40% * 1.5 = 600; 100 base * 29% = 29 (the old double path gave 28).
// Legacy Additive Bug: base * min(100, alloc + bonus) / 100 = 900.
inline int64_t calculateAreaOutput(int64_t totalBaseOutput, int allocPct, int bonusPct, BonusModel model) {
    if (totalBaseOutput <= 0 || allocPct <= 0) return 0;
    const int64_t base = std::min<int64_t>(totalBaseOutput, MaxIntelBalance);
    const int64_t alloc = std::min(allocPct, 100);
    const int64_t bonus = std::clamp(bonusPct, -100, 1000);

    if (model == BonusModel::ProportionalCorrected) {
        // base <= 1e12, alloc <= 100, (100 + bonus) <= 1100: product <= 1.1e17 fits in int64.
        return std::min<int64_t>(MaxIntelBalance, (base * alloc * (100 + bonus)) / 10000);
    } else {
        const int64_t combinedPct = std::clamp<int64_t>(alloc + bonus, 0, 100);
        return (base * combinedPct) / 100;
    }
}

// Compute attack readiness threshold with safe bounded arithmetic.
inline int64_t computeAttackThreshold(
    const IntelligenceRules& rules,
    int currentTurn,
    int lastAttackTurn,
    TreatyLevel treaty,
    int difficultyModifier
) {
    int64_t turnsSinceLast = static_cast<int64_t>(currentTurn) - static_cast<int64_t>(lastAttackTurn);
    int64_t threshold = rules.baseAttackThreshold;

    if (turnsSinceLast >= 0 && turnsSinceLast < rules.attackDelayTurns) {
        threshold += (static_cast<int64_t>(rules.attackDelayTurns) - turnsSinceLast) * rules.delayMultiplier;
    }

    switch (treaty) {
        case TreatyLevel::War:
            threshold += rules.warThresholdModifier;
            break;
        case TreatyLevel::NonAggression:
            threshold += rules.nonAggressionThresholdModifier;
            break;
        case TreatyLevel::Friendship:
            threshold += rules.friendshipThresholdModifier;
            break;
        case TreatyLevel::Affiliation:
        case TreatyLevel::Alliance:
            threshold += rules.allianceThresholdModifier;
            break;
        default:
            break;
    }

    threshold += difficultyModifier;
    return std::clamp<int64_t>(threshold, 50, MaxIntelBalance);
}

// S16 Department Preference Counters (lines 62-65):
// Range: 1..30.
// General focus: +2 all 4 departments (clamped to 30).
// Targeted focus (Economic, Science, Military): selected +2 (clamped to 30), others -2 (min 1).
inline void trainDepartmentWeights(
    std::array<int, 4>& weights,
    IntelDepartment focus,
    const IntelligenceRules& rules
) {
    if (focus == IntelDepartment::General) {
        for (int i = 0; i < 4; ++i) {
            weights[i] = std::min(rules.maxDepartmentCounter, weights[i] + rules.departmentFocusBonus);
        }
    } else {
        int focusIdx = static_cast<int>(focus);
        weights[focusIdx] = std::min(rules.maxDepartmentCounter, weights[focusIdx] + rules.departmentFocusBonus);
        for (int i = 0; i < 4; ++i) {
            if (i == focusIdx) continue;
            weights[i] = std::max(rules.minDepartmentCounter, weights[i] - rules.departmentOtherDeduction);
        }
    }
}

// S16 Source: Defense strength denominator = ((last 20 Security outputs / 20) + (securityStockpile * 0.02)) * defenderMorale.
// Returned in milli-points (x1000) so the result is exact: 200 rolling + 500 stockpile at 100% = 20000 (20.0).
// PROVISIONAL: Exact numeric morale lookup is unverified in intel.bin; morale defaults to 100%.
inline int64_t calculateDefenseStrength(
    int64_t rollingSecuritySum,
    int64_t securityStockpile,
    int defenderMoralePercent,
    const IntelligenceRules& rules
) {
    const int64_t divisor = std::max(1, rules.securityHistoryDivisor);
    const int64_t rolling = std::clamp<int64_t>(rollingSecuritySum, 0, MaxIntelBalance * 20);
    const int64_t stockpile = std::clamp<int64_t>(securityStockpile, 0, MaxIntelBalance);
    const int64_t baseMilli = (rolling * 1000) / divisor + stockpile * std::clamp(rules.securityStockpilePermille, 0, 1000);
    const int64_t morale = std::clamp(defenderMoralePercent, 10, 500);
    // Split the multiply so (base * morale) cannot overflow for any bounded input.
    return (baseMilli / 100) * morale + ((baseMilli % 100) * morale) / 100;
}

// Deterministic department selection based on actual sum of counters
inline IntelDepartment selectDepartment(const std::array<int, 4>& weights, uint64_t& rngState) {
    rngState = rngState * 6364136223846793005ULL + 1442695040888963407ULL;
    int total = weights[0] + weights[1] + weights[2] + weights[3];
    if (total <= 0) return IntelDepartment::General;

    int roll = static_cast<int>((rngState >> 32) % static_cast<uint64_t>(total));
    int accum = 0;
    for (int i = 0; i < 4; ++i) {
        accum += weights[i];
        if (roll < accum) {
            return static_cast<IntelDepartment>(i);
        }
    }
    return IntelDepartment::General;
}

// Per-turn combined attack strength (milli-points) including the concentration factor.
inline int64_t concentratedAttackStrength(const RivalIntelState& rState, const IntelligenceRules& rules) {
    const int64_t average = rState.getAverageAttackStrengthMilli(rules.maxHistoryTurns);
    return (average * std::clamp(rules.attackConcentrationPercent, 0, 1000)) / 100;
}

// Deterministic LCG step shared by department selection and outcome rolls.
inline uint64_t intelNextRandom(uint64_t& rngState) {
    rngState = rngState * 6364136223846793005ULL + 1442695040888963407ULL;
    return rngState >> 32;
}

// Attack/defense ratio in permille (1000 = evenly matched), including the +100
// permille per computer tech level lead. Never negative.
inline int64_t intelStrengthRatioPermille(int64_t attackStrengthMilli, int64_t defenseStrengthMilli, int techDiff) {
    const int64_t attack = std::clamp<int64_t>(attackStrengthMilli, -MaxIntelBalance * 1000, MaxIntelBalance * 1000);
    const int64_t defense = std::clamp<int64_t>(defenseStrengthMilli, 1000, MaxIntelBalance * 1000); // Floor of 1.0 point
    // attack <= 1e15: split the division so the permille scaling cannot overflow.
    int64_t ratio = (attack / defense) * 1000 + ((attack % defense) * 1000) / defense;
    ratio = std::clamp<int64_t>(ratio, -1000000, 1000000);
    ratio += static_cast<int64_t>(std::clamp(techDiff, -100, 100)) * 100; // +10% per computer tech lead
    return std::clamp<int64_t>(ratio, 0, 1000000);
}

// Chance (percent) that counter-intelligence exposes a failed operation:
// clamp(90 - ratio/12, min, max). Weak attacks against strong security are
// usually exposed; evenly matched ones rarely.
inline int failureDetectionPercent(int64_t ratioPermille, const IntelligenceRules& rules) {
    const int64_t chance = 90 - ratioPermille / 12;
    return static_cast<int>(std::clamp<int64_t>(chance, rules.minFailureDetectionPercent, rules.maxFailureDetectionPercent));
}

// Resolve operation attempt given attack and defense factors (milli-points).
// Integer fixed point throughout (MODERN RECONSTRUCTION / PROVISIONAL brackets):
//   ratio R  = attack / defense in permille (+100 per computer tech lead), clamped >= 0
//   roll  r  = [0, 1000)
//   score    = R * (600 + r * 800 / 1000) / 1000        (permille)
//   score < 400: deep failure   (exposed with max(50%, detection%); trust penalty 15, 5 at war)
//   score < 800: failure        (exposed with detection% = clamp(90 - R/12, 10, 90); penalty 8, 2 at war)
//   score < 1300 minor, < 2000 standard, otherwise major success.
inline OperationRecord resolveAttempt(
    int attackerId,
    int defenderId,
    IntelArea area,
    IntelDepartment dept,
    int turn,
    int64_t attackStrengthMilli,
    int64_t defenseStrengthMilli,
    int techDiff,
    TreatyLevel treaty,
    int64_t defenderCredits,
    int64_t defenderResearch,
    uint64_t& rngState,
    const IntelligenceRules& rules = IntelligenceRules{}
) {
    OperationRecord op;
    op.attackerEmpireId = attackerId;
    op.defenderEmpireId = defenderId;
    op.area = area;
    op.department = dept;
    op.turn = turn;

    const int64_t ratio = intelStrengthRatioPermille(attackStrengthMilli, defenseStrengthMilli, techDiff);
    const int64_t roll = static_cast<int64_t>(intelNextRandom(rngState) % 1000);
    const int64_t score = (ratio * (600 + (roll * 800) / 1000)) / 1000;

    if (score < 800) {
        const bool deep = score < 400;
        int detectPct = failureDetectionPercent(ratio, rules);
        if (deep) detectPct = std::max(detectPct, 50);
        const bool caught = static_cast<int64_t>(intelNextRandom(rngState) % 100) < detectPct;
        if (caught) {
            op.outcome = IntelOutcome::Fiasco;
            op.detectedByDefender = true;
            if (deep) {
                op.diplomaticIncidentPenalty = (treaty == TreatyLevel::War) ? 5 : 15;
                op.details = "Operatives compromised and captured during infiltration.";
            } else {
                op.diplomaticIncidentPenalty = (treaty == TreatyLevel::War) ? 2 : 8;
                op.details = "Counter-intelligence exposed the operation as it withdrew.";
            }
        } else {
            op.outcome = IntelOutcome::UndetectedFailure;
            op.detectedByDefender = false;
            op.details = deep ? "Operation foiled by enemy counter-intelligence without detection."
                              : "Mission objectives not met; operatives safely withdrew.";
        }
    } else if (score < 1300) {
        op.outcome = IntelOutcome::MinorSuccess;
        op.detectedByDefender = false;
    } else if (score < 2000) {
        op.outcome = IntelOutcome::StandardSuccess;
        op.detectedByDefender = false;
    } else {
        op.outcome = IntelOutcome::MajorSuccess;
        op.detectedByDefender = false;
    }

    if (op.outcome >= IntelOutcome::MinorSuccess) {
        if (area == IntelArea::Espionage) {
            if (dept == IntelDepartment::Economic) {
                op.opType = IntelOpType::StealCredits;
                const int64_t stealPct = (op.outcome == IntelOutcome::MinorSuccess) ? 5 :
                                         (op.outcome == IntelOutcome::StandardSuccess) ? 15 : 35;
                const int64_t credits = std::clamp<int64_t>(defenderCredits, 0, MaxIntelBalance);
                op.creditsStolen = (credits * stealPct) / 100;
                op.details = "Stole " + std::to_string(op.creditsStolen) + " credits from financial reserves.";
            } else if (dept == IntelDepartment::Science) {
                op.opType = IntelOpType::StealResearch;
                const int64_t stealAmount = (op.outcome == IntelOutcome::MinorSuccess) ? 50 :
                                            (op.outcome == IntelOutcome::StandardSuccess) ? 150 : 400;
                op.researchStolen = std::clamp<int64_t>(std::min(defenderResearch, stealAmount), 0, MaxIntelBalance);
                op.details = "Acquired technical blueprints (" + std::to_string(op.researchStolen) + " research points).";
            } else {
                op.opType = IntelOpType::GatherIntel;
                op.details = "Gathered strategic intelligence on enemy deployments.";
            }
        } else { // Sabotage
            op.opType = IntelOpType::SabotageFacility;
            op.sabotageSeverity = (op.outcome == IntelOutcome::MinorSuccess) ? 1 :
                                  (op.outcome == IntelOutcome::StandardSuccess) ? 2 : 3;
            op.details = "Sabotaged infrastructure with severity level " + std::to_string(op.sabotageSeverity) + ".";
        }
    }

    return op;
}

// Advance an empire's intelligence turn:
// 1. Calculate outputs for Internal Security and each rival area.
// 2. Update Internal Security running weighted average overflow-safely:
//    oldStockpile + (secOutput - oldStockpile) / (turnCounter + 1).
// 3. For zero-allocated areas: stockpile decays by 2% of total output (clamped to 0),
//    and history receives negative 2% of total output.
// 4. Update rolling 20-turn history (dropping turn 21+).
// 5. Update department training counters (1..30 range).
// 6. Evaluate attack thresholds and resolve attacks if threshold reached using the COMBINED
//    signed espionage + sabotage rolling history averaged over the window (milli-points).
// 7. Clear stockpile upon attack attempt.
inline std::vector<OperationRecord> advanceTurn(
    EmpireIntelState& attackerState,
    const TurnContext& context,
    const std::map<int, DefenderSecurityContext>& defenderContexts,
    const IntelligenceRules& rules,
    uint64_t& rngSeed
) {
    std::vector<OperationRecord> operations;

    if (!attackerState.budget.isValid(attackerState.empireId)) {
        return operations;
    }

    int64_t totalOutput = context.totalEmpireBaseIntelOutput;
    int bonusPct = context.buildingBonusPercent;

    // 1. Internal Security Output & Stockpile (VERIFIED S16 overflow-safe weighted average)
    int secPct = attackerState.budget.internalSecurity.percentage;
    int64_t secOutput = calculateAreaOutput(totalOutput, secPct, bonusPct, rules.bonusModel);
    attackerState.recordSecurityOutput(secOutput, rules.maxHistoryTurns);

    if (attackerState.securityTurnCounter <= 0) {
        attackerState.internalSecurityStockpile = secOutput;
        attackerState.securityTurnCounter = 1;
    } else {
        int64_t oldS = attackerState.internalSecurityStockpile;
        attackerState.securityTurnCounter = std::min(MaxIntelTurns, attackerState.securityTurnCounter + 1);
        int64_t newN = attackerState.securityTurnCounter;
        attackerState.internalSecurityStockpile = oldS + (secOutput - oldS) / newN;
    }

    // 2. Process each rival
    for (auto& rivalAlloc : attackerState.budget.rivalAllocations) {
        int rivalId = rivalAlloc.rivalId;
        RivalIntelState& rState = attackerState.getOrCreateRivalState(rivalId);

        const RivalContext* rCtx = nullptr;
        for (const auto& k : context.knownRivals) {
            if (k.rivalId == rivalId) {
                rCtx = &k;
                break;
            }
        }
        TreatyLevel treaty = rCtx ? rCtx->treaty : TreatyLevel::None;
        int diffMod = rCtx ? rCtx->difficultyModifier : 0;
        int64_t targetCredits = rCtx ? rCtx->currentCredits : 0;
        int64_t targetResearch = rCtx ? rCtx->currentResearchReserve : 0;

        // Authoritative defender counter-intelligence state:
        int64_t defRollingSec = 0;
        int64_t defStockpile = 0;
        int defTech = 0;
        int defMorale = rules.defaultDefenderMoralePercent;

        auto defIt = defenderContexts.find(rivalId);
        if (defIt != defenderContexts.end()) {
            defRollingSec = defIt->second.rollingSecuritySum;
            defStockpile = defIt->second.securityStockpile;
            defTech = defIt->second.computerTechLevel;
            defMorale = defIt->second.defenderMoralePercent;
        } else if (rCtx) {
            defTech = rCtx->computerTechLevel;
        }

        int techDiff = context.attackerComputerTechLevel - defTech;
        const int64_t totalDefStrength = calculateDefenseStrength(defRollingSec, defStockpile, defMorale, rules);

        // Update department training counters (range 1..30)
        trainDepartmentWeights(rState.departmentWeights, rState.focusDepartment, rules);

        // Stage both outputs/decays to history and stockpiles first so both attempts
        // see the complete same-turn combined rolling attack history.
        int espPct = rivalAlloc.espionage.percentage;
        if (espPct > 0) {
            int64_t espOut = calculateAreaOutput(totalOutput, espPct, bonusPct, rules.bonusModel);
            rState.espionageStockpile = std::min(MaxIntelBalance, rState.espionageStockpile + espOut);
            rState.recordEspionageOutput(espOut, rules.maxHistoryTurns);
        } else {
            int64_t decay = (totalOutput * rules.decayRatePercent) / 100;
            rState.espionageStockpile = std::max<int64_t>(0, rState.espionageStockpile - decay);
            rState.recordEspionageOutput(-decay, rules.maxHistoryTurns);
        }

        int sabPct = rivalAlloc.sabotage.percentage;
        if (sabPct > 0) {
            int64_t sabOut = calculateAreaOutput(totalOutput, sabPct, bonusPct, rules.bonusModel);
            rState.sabotageStockpile = std::min(MaxIntelBalance, rState.sabotageStockpile + sabOut);
            rState.recordSabotageOutput(sabOut, rules.maxHistoryTurns);
        } else {
            int64_t decay = (totalOutput * rules.decayRatePercent) / 100;
            rState.sabotageStockpile = std::max<int64_t>(0, rState.sabotageStockpile - decay);
            rState.recordSabotageOutput(-decay, rules.maxHistoryTurns);
        }

        // --- Espionage Attempt ---
        int64_t espThreshold = computeAttackThreshold(rules, context.currentTurn, rState.lastEspionageAttackTurn, treaty, diffMod);
        if (rState.espionageStockpile >= espThreshold) {
            rState.espionageStockpile = 0; // Cleared on attempt
            rState.lastEspionageAttackTurn = context.currentTurn;

            IntelDepartment opDept = selectDepartment(rState.departmentWeights, rngSeed);
            const int64_t attackStrength = concentratedAttackStrength(rState, rules);

            OperationRecord op = resolveAttempt(
                attackerState.empireId, rivalId, IntelArea::Espionage, opDept,
                context.currentTurn, attackStrength, totalDefStrength,
                techDiff, treaty, targetCredits, targetResearch, rngSeed, rules
            );
            operations.push_back(op);
        }

        // --- Sabotage Attempt ---
        int64_t sabThreshold = computeAttackThreshold(rules, context.currentTurn, rState.lastSabotageAttackTurn, treaty, diffMod);
        if (rState.sabotageStockpile >= sabThreshold) {
            rState.sabotageStockpile = 0; // Cleared on attempt
            rState.lastSabotageAttackTurn = context.currentTurn;

            IntelDepartment opDept = selectDepartment(rState.departmentWeights, rngSeed);
            const int64_t attackStrength = concentratedAttackStrength(rState, rules);

            OperationRecord op = resolveAttempt(
                attackerState.empireId, rivalId, IntelArea::Sabotage, opDept,
                context.currentTurn, attackStrength, totalDefStrength,
                techDiff, treaty, targetCredits, targetResearch, rngSeed, rules
            );
            operations.push_back(op);
        }
    }

    return operations;
}

// -----------------------------------------------------------------------------
// Adaptive AI Intelligence Strategy (MODERN)
// -----------------------------------------------------------------------------
//
// AI empires re-plan their intelligence budget and department focus every turn
// from their doctrine (personality) and situation. Pure, integer-only and
// deterministic. Doctrine indices mirror diplomacy::MajorPersonality so this
// header stays independent of Diplomacy.h.

enum class IntelDoctrine : int {
    Federation = 0, // Security first; spies on enemies, sabotages only in war
    Klingon = 1,    // Sabotage-heavy against enemies; military intelligence
    Romulan = 2,    // Tal Shiar: the largest foreign service; science theft
    Cardassian = 3, // Obsidian Order: strong security and broad surveillance
    Ferengi = 4     // Economic espionage for profit
};

struct IntelDoctrineProfile {
    int baseSecurity;        // Internal security percentage in peacetime
    int foreignWeight;       // Baseline interest in any rival
    int enemyWeight;         // Extra interest in rivals we are at war with
    int peaceSabotageShare;  // Share (percent) of a rival's foreign budget used for sabotage at peace
    int warSabotageShare;    // ... and at war
    bool spiesOnAllies;
    IntelDepartment peaceFocus;
    IntelDepartment warFocus;
};

inline IntelDoctrineProfile doctrineProfile(IntelDoctrine doctrine) {
    switch (doctrine) {
        case IntelDoctrine::Federation: return {80, 2, 6, 0, 40, false, IntelDepartment::Science, IntelDepartment::Military};
        case IntelDoctrine::Klingon:    return {65, 2, 8, 20, 70, false, IntelDepartment::Military, IntelDepartment::Military};
        case IntelDoctrine::Romulan:    return {55, 4, 6, 30, 50, true, IntelDepartment::Science, IntelDepartment::Military};
        case IntelDoctrine::Cardassian: return {70, 3, 6, 30, 60, false, IntelDepartment::General, IntelDepartment::Military};
        case IntelDoctrine::Ferengi:    return {70, 3, 4, 10, 30, false, IntelDepartment::Economic, IntelDepartment::Economic};
        default: return {80, 2, 6, 0, 40, false, IntelDepartment::General, IntelDepartment::Military};
    }
}

struct AiIntelRivalView {
    int rivalId = 0;
    bool atWar = false;
    TreatyLevel treaty = TreatyLevel::None;
    int trust = 50;
    int64_t ownPower = 0;
    int64_t rivalPower = 0;
    int incidentsAgainstUs = 0; // Operations by this rival we detected recently
    int exposedOperations = 0;  // Our recent operations against this rival that were exposed
    bool eliminated = false;
};

inline constexpr size_t AiIntelMaxTargets = 2; // AI concentrates its foreign service

struct AiIntelPlan {
    BudgetAllocations budget;
    std::map<int, IntelDepartment> focus; // rivalId -> department focus

    bool operator==(const AiIntelPlan& other) const = default;
};

inline AiIntelPlan planAiIntelStrategy(int empireId, IntelDoctrine doctrine, const std::vector<AiIntelRivalView>& rivals) {
    const IntelDoctrineProfile prof = doctrineProfile(doctrine);
    AiIntelPlan plan;

    // 1. Security: doctrine baseline, raised by wars and by spies we caught.
    int incidents = 0;
    bool anyWar = false;
    for (const auto& r : rivals) {
        if (r.eliminated || r.rivalId == empireId) continue;
        incidents += std::max(0, r.incidentsAgainstUs);
        anyWar = anyWar || r.atWar;
    }
    int security = prof.baseSecurity + std::min(20, 5 * incidents) + (anyWar ? 5 : 0);
    security = std::clamp(security, 40, 90);

    // 2. Interest in each rival (the two most interesting, highest first, ties by ID).
    struct Interest { int rivalId; int weight; const AiIntelRivalView* view; };
    std::vector<Interest> interests;
    for (const auto& r : rivals) {
        if (r.eliminated || r.rivalId <= 0 || r.rivalId == empireId) continue;
        int weight = prof.foreignWeight;
        if (r.atWar) weight += prof.enemyWeight;
        if (r.trust < 30) weight += 2;
        if (r.rivalPower > r.ownPower) weight += 2;
        weight += std::min(3, std::max(0, r.incidentsAgainstUs)); // Retaliation
        weight -= 2 * std::min(3, std::max(0, r.exposedOperations)); // Our agents keep getting caught: back off
        if (r.atWar) weight = std::max(weight, 1);
        if (!r.atWar && (r.treaty == TreatyLevel::Alliance || r.treaty == TreatyLevel::Affiliation)) {
            if (!prof.spiesOnAllies) weight = 0;
        } else if (!r.atWar && r.treaty == TreatyLevel::Friendship) {
            weight /= 2;
        }
        if (weight > 0) interests.push_back({r.rivalId, weight, &r});
    }
    std::sort(interests.begin(), interests.end(), [](const Interest& a, const Interest& b) {
        return a.weight != b.weight ? a.weight > b.weight : a.rivalId < b.rivalId;
    });
    if (interests.size() > AiIntelMaxTargets) interests.resize(AiIntelMaxTargets);

    if (interests.empty()) {
        plan.budget.internalSecurity.percentage = 100;
        return plan;
    }

    // 3. Distribute the foreign share by interest (largest remainder, ties by order).
    const int foreign = 100 - security;
    int totalWeight = 0;
    for (const auto& i : interests) totalWeight += i.weight;
    std::vector<int> shares(interests.size(), 0);
    std::vector<std::pair<int, size_t>> remainders;
    int assigned = 0;
    for (size_t i = 0; i < interests.size(); ++i) {
        shares[i] = (foreign * interests[i].weight) / totalWeight;
        remainders.push_back({(foreign * interests[i].weight) % totalWeight, i});
        assigned += shares[i];
    }
    std::stable_sort(remainders.begin(), remainders.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (size_t k = 0; assigned < foreign && k < remainders.size(); ++k, ++assigned) {
        ++shares[remainders[k].second];
    }

    plan.budget.internalSecurity.percentage = security;
    std::vector<size_t> order(interests.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return interests[a].rivalId < interests[b].rivalId; });
    for (size_t idx : order) {
        const auto& interest = interests[idx];
        const int share = shares[idx];
        const int sabotageShare = interest.view->atWar ? prof.warSabotageShare : prof.peaceSabotageShare;
        RivalAllocation alloc;
        alloc.rivalId = interest.rivalId;
        alloc.sabotage.percentage = (share * sabotageShare) / 100;
        alloc.espionage.percentage = share - alloc.sabotage.percentage;
        plan.budget.rivalAllocations.push_back(alloc);
        plan.focus[interest.rivalId] = interest.view->atWar ? prof.warFocus : prof.peaceFocus;
    }
    if (!plan.budget.isValid(empireId)) {
        // Defensive fallback; cannot happen for bounded inputs.
        plan.budget = BudgetAllocations{};
        plan.budget.internalSecurity.percentage = 100;
        plan.focus.clear();
    }
    return plan;
}

// Applies a plan: budget replaced, focus set per rival, per-rival history kept.
inline void applyAiIntelPlan(EmpireIntelState& state, const AiIntelPlan& plan) {
    state.budget = plan.budget;
    for (const auto& [rivalId, dept] : plan.focus) {
        state.getOrCreateRivalState(rivalId).focusDepartment = dept;
    }
}

// -----------------------------------------------------------------------------
// Bounded Serialization & Deserialization
// -----------------------------------------------------------------------------

inline void writeRivalIntelBody(std::ostringstream& ss, const RivalIntelState* rState) {
    if (rState) {
        ss << "rData " << rState->espionageStockpile << " " << rState->sabotageStockpile << " "
           << rState->lastEspionageAttackTurn << " " << rState->lastSabotageAttackTurn << " "
           << static_cast<int>(rState->focusDepartment) << " "
           << rState->departmentWeights[0] << " " << rState->departmentWeights[1] << " "
           << rState->departmentWeights[2] << " " << rState->departmentWeights[3] << "\n";

        ss << "espHist " << rState->espionageHistory.size();
        for (auto v : rState->espionageHistory) ss << " " << v;
        ss << "\n";

        ss << "sabHist " << rState->sabotageHistory.size();
        for (auto v : rState->sabotageHistory) ss << " " << v;
        ss << "\n";
    } else {
        ss << "rData 0 0 -100 -100 0 10 10 10 10\n";
        ss << "espHist 0\n";
        ss << "sabHist 0\n";
    }
}

// Parses the rData/espHist/sabHist body of one rival (strict bounds).
inline bool readRivalIntelBody(std::istringstream& ss, RivalIntelState& rState, std::string* errorMsg) {
    std::string token;
    int focusDept = 0;
    if (!(ss >> token) || token != "rData" ||
        !(ss >> rState.espionageStockpile >> rState.sabotageStockpile
             >> rState.lastEspionageAttackTurn >> rState.lastSabotageAttackTurn
             >> focusDept
             >> rState.departmentWeights[0] >> rState.departmentWeights[1]
             >> rState.departmentWeights[2] >> rState.departmentWeights[3])) {
        if (errorMsg) *errorMsg = "Malformed rData entry";
        return false;
    }
    if (rState.espionageStockpile < 0 || rState.espionageStockpile > MaxIntelBalance ||
        rState.sabotageStockpile < 0 || rState.sabotageStockpile > MaxIntelBalance) {
        if (errorMsg) *errorMsg = "Stockpile out of bounds [0, MaxIntelBalance]";
        return false;
    }
    if (rState.lastEspionageAttackTurn < MinLastAttackTurn || rState.lastEspionageAttackTurn > MaxIntelTurns ||
        rState.lastSabotageAttackTurn < MinLastAttackTurn || rState.lastSabotageAttackTurn > MaxIntelTurns) {
        if (errorMsg) *errorMsg = "Last attack turn out of bounds [-100, MaxIntelTurns]";
        return false;
    }
    if (focusDept < 0 || focusDept > 3) {
        if (errorMsg) *errorMsg = "Invalid focus department";
        return false;
    }
    rState.focusDepartment = static_cast<IntelDepartment>(focusDept);

    for (int i = 0; i < 4; ++i) {
        if (rState.departmentWeights[i] < 1 || rState.departmentWeights[i] > 30) {
            if (errorMsg) *errorMsg = "Department counter out of valid range 1..30";
            return false;
        }
    }

    size_t espHistCount = 0;
    if (!(ss >> token) || token != "espHist" || !(ss >> espHistCount) || espHistCount > 20) {
        if (errorMsg) *errorMsg = "Malformed espHist";
        return false;
    }
    for (size_t i = 0; i < espHistCount; ++i) {
        int64_t v = 0;
        if (!(ss >> v) || v < -MaxIntelBalance || v > MaxIntelBalance) {
            if (errorMsg) *errorMsg = "Malformed or out-of-bounds espHist value";
            return false;
        }
        rState.espionageHistory.push_back(v);
    }

    size_t sabHistCount = 0;
    if (!(ss >> token) || token != "sabHist" || !(ss >> sabHistCount) || sabHistCount > 20) {
        if (errorMsg) *errorMsg = "Malformed sabHist";
        return false;
    }
    for (size_t i = 0; i < sabHistCount; ++i) {
        int64_t v = 0;
        if (!(ss >> v) || v < -MaxIntelBalance || v > MaxIntelBalance) {
            if (errorMsg) *errorMsg = "Malformed or out-of-bounds sabHist value";
            return false;
        }
        rState.sabotageHistory.push_back(v);
    }
    return true;
}

inline std::string serializeIntelState(const EmpireIntelState& state) {
    std::ostringstream ss;
    ss << "BOTF_INTEL_V1\n";
    ss << "empireId " << state.empireId << "\n";
    ss << "secStockpile " << state.internalSecurityStockpile << " " << state.securityTurnCounter << "\n";
    ss << "secAlloc " << state.budget.internalSecurity.percentage << " " << (state.budget.internalSecurity.isLocked ? 1 : 0) << "\n";

    ss << "secHistory " << state.internalSecurityHistory.size();
    for (auto val : state.internalSecurityHistory) {
        ss << " " << val;
    }
    ss << "\n";

    ss << "rivalCount " << state.budget.rivalAllocations.size() << "\n";
    for (const auto& rAlloc : state.budget.rivalAllocations) {
        int rId = rAlloc.rivalId;
        const RivalIntelState* rState = state.getRivalState(rId);

        ss << "rival " << rId << " "
           << rAlloc.espionage.percentage << " " << (rAlloc.espionage.isLocked ? 1 : 0) << " "
           << rAlloc.sabotage.percentage << " " << (rAlloc.sabotage.isLocked ? 1 : 0) << "\n";

        writeRivalIntelBody(ss, rState);
    }

    // Per-rival state for rivals outside the current budget (focus, stockpiles,
    // history, attack clocks) must survive a reload too. Emitted only when present
    // so saves without such rivals re-serialize byte-for-byte.
    // A pristine (default) rival state is indistinguishable from an absent one, so
    // it is omitted to keep the save canonical.
    std::vector<const RivalIntelState*> extras;
    for (const auto& r : state.rivalStates) {
        const bool budgeted = std::any_of(state.budget.rivalAllocations.begin(), state.budget.rivalAllocations.end(),
            [&](const RivalAllocation& a) { return a.rivalId == r.rivalId; });
        RivalIntelState pristine;
        pristine.rivalId = r.rivalId;
        if (!budgeted && !(r == pristine)) extras.push_back(&r);
    }
    if (!extras.empty()) {
        std::sort(extras.begin(), extras.end(), [](const RivalIntelState* a, const RivalIntelState* b) { return a->rivalId < b->rivalId; });
        ss << "extraRivals " << extras.size() << "\n";
        for (const auto* r : extras) {
            ss << "xrival " << r->rivalId << "\n";
            writeRivalIntelBody(ss, r);
        }
    }

    ss << "END_BOTF_INTEL\n";
    return ss.str();
}

inline bool deserializeIntelState(const std::string& data, EmpireIntelState& outState, std::string* errorMsg = nullptr) {
    std::istringstream ss(data);
    std::string token;

    if (!(ss >> token) || token != "BOTF_INTEL_V1") {
        if (errorMsg) *errorMsg = "Invalid header or magic token";
        return false;
    }

    EmpireIntelState temp;

    if (!(ss >> token) || token != "empireId" || !(ss >> temp.empireId) || temp.empireId <= 0) {
        if (errorMsg) *errorMsg = "Malformed or missing empireId";
        return false;
    }

    if (!(ss >> token) || token != "secStockpile" || !(ss >> temp.internalSecurityStockpile >> temp.securityTurnCounter) ||
        temp.internalSecurityStockpile < 0 || temp.internalSecurityStockpile > MaxIntelBalance ||
        temp.securityTurnCounter < 0 || temp.securityTurnCounter > MaxIntelTurns) {
        if (errorMsg) *errorMsg = "Malformed or out-of-bounds secStockpile or securityTurnCounter";
        return false;
    }

    int secLock = 0;
    if (!(ss >> token) || token != "secAlloc" || !(ss >> temp.budget.internalSecurity.percentage >> secLock)) {
        if (errorMsg) *errorMsg = "Malformed secAlloc";
        return false;
    }
    if (secLock != 0 && secLock != 1) {
        if (errorMsg) *errorMsg = "Non-boolean secLock flag";
        return false;
    }
    temp.budget.internalSecurity.isLocked = (secLock == 1);
    if (temp.budget.internalSecurity.percentage < 0 || temp.budget.internalSecurity.percentage > 100) {
        if (errorMsg) *errorMsg = "Internal security allocation out of bounds";
        return false;
    }

    size_t secHistCount = 0;
    if (!(ss >> token) || token != "secHistory" || !(ss >> secHistCount)) {
        if (errorMsg) *errorMsg = "Malformed secHistory";
        return false;
    }
    if (secHistCount > 20) {
        if (errorMsg) *errorMsg = "Internal security history exceeds 20 turns";
        return false;
    }
    for (size_t i = 0; i < secHistCount; ++i) {
        int64_t v = 0;
        if (!(ss >> v) || v < 0 || v > MaxIntelBalance) {
            if (errorMsg) *errorMsg = "Malformed or out-of-bounds security history value";
            return false;
        }
        temp.internalSecurityHistory.push_back(v);
    }

    size_t rivalCount = 0;
    if (!(ss >> token) || token != "rivalCount" || !(ss >> rivalCount)) {
        if (errorMsg) *errorMsg = "Malformed rivalCount";
        return false;
    }
    if (rivalCount > 4) {
        if (errorMsg) *errorMsg = "rivalCount exceeds maximum of 4 rivals";
        return false;
    }

    std::vector<int> seenRivalIds;
    for (size_t r = 0; r < rivalCount; ++r) {
        int rivalId = 0;
        int espPct = 0, espLock = 0, sabPct = 0, sabLock = 0;
        if (!(ss >> token) || token != "rival" || !(ss >> rivalId >> espPct >> espLock >> sabPct >> sabLock)) {
            if (errorMsg) *errorMsg = "Malformed rival allocation entry";
            return false;
        }
        if (rivalId <= 0 || rivalId == temp.empireId || std::find(seenRivalIds.begin(), seenRivalIds.end(), rivalId) != seenRivalIds.end()) {
            if (errorMsg) *errorMsg = "Duplicate, self, or non-positive rivalId";
            return false;
        }
        seenRivalIds.push_back(rivalId);

        if (espLock != 0 && espLock != 1) {
            if (errorMsg) *errorMsg = "Non-boolean espLock flag";
            return false;
        }
        if (sabLock != 0 && sabLock != 1) {
            if (errorMsg) *errorMsg = "Non-boolean sabLock flag";
            return false;
        }

        if (espPct < 0 || espPct > 100 || sabPct < 0 || sabPct > 100) {
            if (errorMsg) *errorMsg = "Foreign allocation percentage out of bounds";
            return false;
        }

        RivalAllocation alloc;
        alloc.rivalId = rivalId;
        alloc.espionage.percentage = espPct;
        alloc.espionage.isLocked = (espLock == 1);
        alloc.sabotage.percentage = sabPct;
        alloc.sabotage.isLocked = (sabLock == 1);
        temp.budget.rivalAllocations.push_back(alloc);

        RivalIntelState rState;
        rState.rivalId = rivalId;
        if (!readRivalIntelBody(ss, rState, errorMsg)) return false;

        temp.rivalStates.push_back(rState);
    }

    if (!(ss >> token)) {
        if (errorMsg) *errorMsg = "Missing END_BOTF_INTEL termination token";
        return false;
    }
    if (token == "extraRivals") {
        size_t extraCount = 0;
        if (!(ss >> extraCount) || extraCount == 0 || extraCount + rivalCount > 4) {
            if (errorMsg) *errorMsg = "Malformed extraRivals count";
            return false;
        }
        int previousExtra = 0;
        for (size_t r = 0; r < extraCount; ++r) {
            int rivalId = 0;
            if (!(ss >> token) || token != "xrival" || !(ss >> rivalId)) {
                if (errorMsg) *errorMsg = "Malformed extra rival entry";
                return false;
            }
            if (rivalId <= 0 || rivalId == temp.empireId || rivalId <= previousExtra ||
                std::find(seenRivalIds.begin(), seenRivalIds.end(), rivalId) != seenRivalIds.end()) {
                if (errorMsg) *errorMsg = "Duplicate, unsorted, self, or non-positive extra rivalId";
                return false;
            }
            previousExtra = rivalId;
            seenRivalIds.push_back(rivalId);
            RivalIntelState rState;
            rState.rivalId = rivalId;
            if (!readRivalIntelBody(ss, rState, errorMsg)) return false;
            temp.rivalStates.push_back(rState);
        }
        if (!(ss >> token)) {
            if (errorMsg) *errorMsg = "Missing END_BOTF_INTEL termination token";
            return false;
        }
    }
    if (token != "END_BOTF_INTEL") {
        if (errorMsg) *errorMsg = "Missing END_BOTF_INTEL termination token";
        return false;
    }

    std::string trailing;
    if (ss >> trailing) {
        if (errorMsg) *errorMsg = "Trailing tokens detected after END_BOTF_INTEL";
        return false;
    }

    if (!temp.budget.isValid(temp.empireId)) {
        if (errorMsg) *errorMsg = "Deserialized budget allocations do not sum to 100 or exceed 4 rivals";
        return false;
    }

    outState = temp;
    return true;
}

inline std::string encodeString(const std::string& str) {
    if (str.empty()) {
        return "%empty%";
    }
    std::string result;
    result.reserve(str.size() * 3);
    for (unsigned char ch : str) {
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('%');
            char hexBuf[3];
            snprintf(hexBuf, sizeof(hexBuf), "%02X", static_cast<unsigned int>(ch));
            result.append(hexBuf);
        }
    }
    return result;
}

inline bool decodeString(const std::string& token, std::string& out, std::string* outError = nullptr) {
    if (token == "%empty%") {
        out.clear();
        return true;
    }
    out.clear();
    out.reserve(token.size());
    for (size_t i = 0; i < token.size(); ++i) {
        if (token[i] == '%') {
            if (i + 2 >= token.size()) {
                if (outError) *outError = "Malformed hex escape in encoded string";
                return false;
            }
            // Exactly two hex digits: strtol would also accept "+F", " F" or "-0".
            auto fromHex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = fromHex(token[i + 1]);
            const int lo = fromHex(token[i + 2]);
            if (hi < 0 || lo < 0) {
                if (outError) *outError = "Invalid hex escape in encoded string";
                return false;
            }
            out.push_back(static_cast<char>((hi << 4) | lo));
            i += 2;
        } else {
            out.push_back(token[i]);
        }
    }
    return true;
}

inline std::string serializeOperationRecord(const OperationRecord& op) {
    std::ostringstream ss;
    ss << op.attackerEmpireId << " "
       << op.defenderEmpireId << " "
       << static_cast<int>(op.area) << " "
       << static_cast<int>(op.department) << " "
       << static_cast<int>(op.outcome) << " "
       << static_cast<int>(op.opType) << " "
       << op.turn << " "
       << op.creditsStolen << " "
       << op.researchStolen << " "
       << op.sabotageTargetId << " "
       << op.sabotageSeverity << " "
       << (op.detectedByDefender ? 1 : 0) << " "
       << op.diplomaticIncidentPenalty << " "
       << encodeString(op.details);
    return ss.str();
}

inline bool deserializeOperationRecord(const std::string& line, OperationRecord& outOp, std::string* errorMsg = nullptr) {
    std::istringstream ss(line);
    int areaInt = 0, deptInt = 0, outcomeInt = 0, opTypeInt = 0, detectedInt = 0;
    std::string encodedDetails, trailing;
    if (!(ss >> outOp.attackerEmpireId >> outOp.defenderEmpireId
             >> areaInt >> deptInt >> outcomeInt >> opTypeInt
             >> outOp.turn
             >> outOp.creditsStolen >> outOp.researchStolen
             >> outOp.sabotageTargetId >> outOp.sabotageSeverity
             >> detectedInt >> outOp.diplomaticIncidentPenalty
             >> encodedDetails) || (ss >> trailing)) {
        if (errorMsg) *errorMsg = "Malformed operation record row";
        return false;
    }
    if (outOp.attackerEmpireId <= 0 || outOp.defenderEmpireId <= 0 || outOp.turn < 0) {
        if (errorMsg) *errorMsg = "Invalid IDs or turn in operation record";
        return false;
    }
    if (areaInt < 0 || areaInt > 2 || deptInt < 0 || deptInt > 3 ||
        outcomeInt < 0 || outcomeInt > 4 || opTypeInt < 0 || opTypeInt > 4 ||
        (detectedInt != 0 && detectedInt != 1)) {
        if (errorMsg) *errorMsg = "Invalid enum value in operation record";
        return false;
    }
    if (outOp.attackerEmpireId == outOp.defenderEmpireId || outOp.turn > MaxIntelTurns ||
        outOp.creditsStolen < 0 || outOp.creditsStolen > MaxIntelBalance ||
        outOp.researchStolen < 0 || outOp.researchStolen > MaxIntelBalance ||
        outOp.sabotageTargetId < 0 || outOp.sabotageSeverity < 0 || outOp.sabotageSeverity > 3 ||
        outOp.diplomaticIncidentPenalty < 0 || outOp.diplomaticIncidentPenalty > 100) {
        if (errorMsg) *errorMsg = "Out-of-range value in operation record";
        return false;
    }
    outOp.area = static_cast<IntelArea>(areaInt);
    outOp.department = static_cast<IntelDepartment>(deptInt);
    outOp.outcome = static_cast<IntelOutcome>(outcomeInt);
    outOp.opType = static_cast<IntelOpType>(opTypeInt);
    outOp.detectedByDefender = (detectedInt != 0);
    if (!decodeString(encodedDetails, outOp.details, errorMsg)) {
        return false;
    }
    return true;
}

} // namespace botf::intelligence
