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
//   defenseDenominator = ((last 20 security outputs / 20) + (securityStockpile * 0.02)) * defenderMoraleMultiplier.
// - VERIFIED (S16): Zero-allocation areas contribute -2% of total intelligence output to rolling history
//   for success calculation, and decay stockpile by 2% of total output (clamped to 0).
// - VERIFIED (S16): Internal Security stockpile is a running weighted average across turns:
//   (oldStockpile * turnCounter + currentSecurityOutput) / (turnCounter + 1).
// - VERIFIED (S16): Proportional bonus bug fix vs legacy parity (1000 base * 40% * 1.5 = 600).
// - VERIFIED (S16): Department preference counters range 1..30; General focus adds +2 to ALL 4 departments;
//   targeted focus adds +2 to selected and -2 to others; weighted draw uses actual counter sum.
// - PROVISIONAL / MODERN RECONSTRUCTION: Base readiness threshold (250), attack delay (5 turns),
//   treaty modifiers, credit/sabotage scaling, and exact morale multiplier mapping (default 1.0).
//
// Architectural Invariants:
// - Self-contained header; independent of Simulation.h (no circular dependencies).
// - Bounded arithmetic adhering to MaxIntelBalance (10^12) and MaxIntelTurns (10^6).
// - Returns immutable OperationRecord objects for atomic external application; no GameState mutation.
// =============================================================================

#include <algorithm>
#include <array>
#include <cmath>
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
    double securityStockpileCoefficient = 0.02; // S16: 0.02 multiplier on security stockpile

    // PROVISIONAL: Default defender morale multiplier when unspecified (default 1.0)
    double defaultDefenderMoraleMultiplier = 1.0;

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
    int64_t currentResearchReserve = 0;
};

// Authoritative defender counter-intelligence state
struct DefenderSecurityContext {
    int64_t rollingSecuritySum = 0;
    int64_t securityStockpile = 0;
    int computerTechLevel = 0;
    // Sourced defense morale multiplier. Exact curve is PROVISIONAL (default 1.0).
    double defenderMoraleMultiplier = 1.0;
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
// Proportional Corrected: base * (alloc / 100.0) * (1.0 + bonus / 100.0)
// Example: 1000 base * 40% * 1.5 = 600.
// Legacy Additive Bug: base * (alloc + bonus) / 100.0 = 900.
inline int64_t calculateAreaOutput(int64_t totalBaseOutput, int allocPct, int bonusPct, BonusModel model) {
    if (totalBaseOutput <= 0 || allocPct <= 0) return 0;

    if (model == BonusModel::ProportionalCorrected) {
        double allocFraction = static_cast<double>(allocPct) / 100.0;
        double bonusMultiplier = 1.0 + (static_cast<double>(bonusPct) / 100.0);
        return static_cast<int64_t>(std::floor(static_cast<double>(totalBaseOutput) * allocFraction * bonusMultiplier));
    } else {
        double combinedPct = std::min(100.0, static_cast<double>(allocPct + bonusPct));
        return static_cast<int64_t>(std::floor(static_cast<double>(totalBaseOutput) * (combinedPct / 100.0)));
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

// S16 Source: Defense strength denominator = ((last 20 Security outputs / 20) + (securityStockpile * 0.02)) * defenderMoraleMultiplier.
// PROVISIONAL: Exact numeric morale lookup is unverified in intel.bin; defenderMoraleMultiplier default is 1.0.
inline double calculateDefenseStrength(
    int64_t rollingSecuritySum,
    int64_t securityStockpile,
    double defenderMoraleMultiplier,
    const IntelligenceRules& rules
) {
    double rollingAvg = static_cast<double>(rollingSecuritySum) / static_cast<double>(rules.securityHistoryDivisor);
    double stockpileTerm = static_cast<double>(securityStockpile) * rules.securityStockpileCoefficient;
    double baseDefense = rollingAvg + stockpileTerm;
    double effectiveMultiplier = std::max(0.1, defenderMoraleMultiplier);
    return baseDefense * effectiveMultiplier;
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

// Resolve operation attempt given attack and defense factors.
inline OperationRecord resolveAttempt(
    int attackerId,
    int defenderId,
    IntelArea area,
    IntelDepartment dept,
    int turn,
    int64_t attackStrength,
    double defenseStrength,
    int techDiff,
    TreatyLevel treaty,
    int64_t defenderCredits,
    int64_t defenderResearch,
    uint64_t& rngState
) {
    OperationRecord op;
    op.attackerEmpireId = attackerId;
    op.defenderEmpireId = defenderId;
    op.area = area;
    op.department = dept;
    op.turn = turn;

    // Defense denominator already incorporates defender's morale multiplier
    double safeDefense = std::max(1.0, defenseStrength);
    double strengthRatio = static_cast<double>(attackStrength) / safeDefense;

    // Modifiers: applied exactly once
    double techBonus = static_cast<double>(techDiff) * 0.10; // +10% per computer tech lead

    double netRatio = strengthRatio + techBonus;

    // Valid probability clamping: if netRatio <= 0, clamped at probability stage
    if (netRatio < 0.0) {
        netRatio = 0.0;
    }

    // PRNG roll [0.0, 1.0)
    rngState = rngState * 6364136223846793005ULL + 1442695040888963407ULL;
    double roll = static_cast<double>((rngState >> 32) & 0xFFFFFF) / 16777216.0;

    // Outcomes scoring brackets (MODERN RECONSTRUCTION / PROVISIONAL):
    double score = netRatio * (0.6 + roll * 0.8);

    if (score < 0.4) {
        rngState = rngState * 6364136223846793005ULL + 1442695040888963407ULL;
        bool caught = ((rngState >> 32) % 100) < 50;
        if (caught) {
            op.outcome = IntelOutcome::Fiasco;
            op.detectedByDefender = true;
            op.diplomaticIncidentPenalty = (treaty == TreatyLevel::War) ? 10 : 30;
            op.details = "Operatives compromised and captured during infiltration.";
        } else {
            op.outcome = IntelOutcome::UndetectedFailure;
            op.detectedByDefender = false;
            op.details = "Operation foiled by enemy counter-intelligence without detection.";
        }
    } else if (score < 0.8) {
        op.outcome = IntelOutcome::UndetectedFailure;
        op.detectedByDefender = false;
        op.details = "Mission objectives not met; operatives safely withdrew.";
    } else if (score < 1.3) {
        op.outcome = IntelOutcome::MinorSuccess;
        op.detectedByDefender = false;
    } else if (score < 2.0) {
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
                double stealPct = (op.outcome == IntelOutcome::MinorSuccess) ? 0.05 :
                                  (op.outcome == IntelOutcome::StandardSuccess) ? 0.15 : 0.35;
                op.creditsStolen = static_cast<int64_t>(std::floor(static_cast<double>(defenderCredits) * stealPct));
                op.details = "Stole " + std::to_string(op.creditsStolen) + " credits from financial reserves.";
            } else if (dept == IntelDepartment::Science) {
                op.opType = IntelOpType::StealResearch;
                int64_t stealAmount = (op.outcome == IntelOutcome::MinorSuccess) ? 50 :
                                      (op.outcome == IntelOutcome::StandardSuccess) ? 150 : 400;
                op.researchStolen = std::min(defenderResearch, stealAmount);
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
// 6. Evaluate attack thresholds and resolve attacks if threshold reached using COMBINED
//    signed espionage + sabotage rolling history.
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
        double defMoraleMult = rules.defaultDefenderMoraleMultiplier;

        auto defIt = defenderContexts.find(rivalId);
        if (defIt != defenderContexts.end()) {
            defRollingSec = defIt->second.rollingSecuritySum;
            defStockpile = defIt->second.securityStockpile;
            defTech = defIt->second.computerTechLevel;
            defMoraleMult = defIt->second.defenderMoraleMultiplier;
        } else if (rCtx) {
            defTech = rCtx->computerTechLevel;
        }

        int techDiff = context.attackerComputerTechLevel - defTech;
        double totalDefStrength = calculateDefenseStrength(defRollingSec, defStockpile, defMoraleMult, rules);

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
            int64_t attackStrength = rState.getCombinedRollingAttackSum();

            OperationRecord op = resolveAttempt(
                attackerState.empireId, rivalId, IntelArea::Espionage, opDept,
                context.currentTurn, attackStrength, totalDefStrength,
                techDiff, treaty, targetCredits, targetResearch, rngSeed
            );
            operations.push_back(op);
        }

        // --- Sabotage Attempt ---
        int64_t sabThreshold = computeAttackThreshold(rules, context.currentTurn, rState.lastSabotageAttackTurn, treaty, diffMod);
        if (rState.sabotageStockpile >= sabThreshold) {
            rState.sabotageStockpile = 0; // Cleared on attempt
            rState.lastSabotageAttackTurn = context.currentTurn;

            IntelDepartment opDept = selectDepartment(rState.departmentWeights, rngSeed);
            int64_t attackStrength = rState.getCombinedRollingAttackSum();

            OperationRecord op = resolveAttempt(
                attackerState.empireId, rivalId, IntelArea::Sabotage, opDept,
                context.currentTurn, attackStrength, totalDefStrength,
                techDiff, treaty, targetCredits, targetResearch, rngSeed
            );
            operations.push_back(op);
        }
    }

    return operations;
}

// -----------------------------------------------------------------------------
// Bounded Serialization & Deserialization
// -----------------------------------------------------------------------------

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

        temp.rivalStates.push_back(rState);
    }

    if (!(ss >> token) || token != "END_BOTF_INTEL") {
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
            std::string hexStr = token.substr(i + 1, 2);
            char* endPtr = nullptr;
            long val = std::strtol(hexStr.c_str(), &endPtr, 16);
            if (!endPtr || *endPtr != '\0' || val < 0 || val > 255) {
                if (outError) *outError = "Invalid hex escape in encoded string";
                return false;
            }
            out.push_back(static_cast<char>(val));
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
    std::string encodedDetails;
    if (!(ss >> outOp.attackerEmpireId >> outOp.defenderEmpireId
             >> areaInt >> deptInt >> outcomeInt >> opTypeInt
             >> outOp.turn
             >> outOp.creditsStolen >> outOp.researchStolen
             >> outOp.sabotageTargetId >> outOp.sabotageSeverity
             >> detectedInt >> outOp.diplomaticIncidentPenalty
             >> encodedDetails)) {
        if (errorMsg) *errorMsg = "Malformed operation record row";
        return false;
    }
    if (outOp.attackerEmpireId <= 0 || outOp.defenderEmpireId <= 0 || outOp.turn < 0) {
        if (errorMsg) *errorMsg = "Invalid IDs or turn in operation record";
        return false;
    }
    if (areaInt < 0 || areaInt > 2 || deptInt < 0 || deptInt > 3 ||
        outcomeInt < 0 || outcomeInt > 4 || opTypeInt < 0 || opTypeInt > 10) {
        if (errorMsg) *errorMsg = "Invalid enum value in operation record";
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
