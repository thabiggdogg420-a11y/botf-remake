// Scenario suites must check even in RelWithDebInfo/Release (NDEBUG) builds.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "Intelligence.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace botf::intelligence;

// -----------------------------------------------------------------------------
// Test 1: Invalid Budgets No Mutation
// -----------------------------------------------------------------------------
void testInvalidBudgetsNoMutation() {
    std::cout << "Testing: invalid budgets and mutation protection..." << std::endl;

    BudgetAllocations budget;
    budget.setDefault({2, 3});
    assert(budget.isValid());
    assert(budget.totalPercentage() == 100);

    auto baseline = budget;

    // 1. Negative internal security percentage
    bool ok = budget.setInternalSecurity(-10);
    assert(!ok && "Negative allocation must be rejected");
    assert(budget == baseline && "State must not mutate on rejected call");

    // 2. Over 100% internal security percentage
    ok = budget.setInternalSecurity(105);
    assert(!ok && "Over 100% allocation must be rejected");
    assert(budget == baseline && "State must not mutate on rejected call");

    // 3. Foreign slider out of bounds
    ok = budget.setRivalAllocation(2, IntelArea::Espionage, 150);
    assert(!ok && "Over 100% foreign allocation must be rejected");
    assert(budget == baseline && "State must not mutate on rejected call");

    ok = budget.setRivalAllocation(2, IntelArea::Espionage, -1);
    assert(!ok && "Negative foreign allocation must be rejected");
    assert(budget == baseline && "State must not mutate on rejected call");

    // 4. Unknown rival ID
    ok = budget.setRivalAllocation(999, IntelArea::Espionage, 20);
    assert(!ok && "Non-existent rival must be rejected");
    assert(budget == baseline && "State must not mutate on rejected call");

    // 5. Impossibility to rebalance due to locks
    budget.setLock(2, IntelArea::Espionage, true);
    budget.setLock(2, IntelArea::Sabotage, true);
    budget.setLock(3, IntelArea::Espionage, true);
    budget.setLock(3, IntelArea::Sabotage, true);

    auto lockedBaseline = budget;
    ok = budget.setInternalSecurity(70);
    assert(!ok && "Cannot adjust internal security when all other sliders are locked");
    assert(budget == lockedBaseline && "State must not mutate when redistribution is impossible");

    std::cout << "  PASS: invalid budgets rejected with zero mutation." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 2: Lock Redistribution
// -----------------------------------------------------------------------------
void testLockRedistribution() {
    std::cout << "Testing: lock redistribution and slider balancing..." << std::endl;

    BudgetAllocations budget;
    budget.setDefault({2, 3}); // 80% Internal Security, 5% each for 4 foreign slots
    assert(budget.internalSecurity.percentage == 80);
    assert(budget.rivalAllocations.size() == 2);

    // Case A: Adjust Internal Security directly (80% -> 60%, delta = -20%)
    bool ok = budget.setInternalSecurity(60);
    assert(ok);
    assert(budget.internalSecurity.percentage == 60);
    assert(budget.totalPercentage() == 100);
    assert(budget.rivalAllocations[0].espionage.percentage == 10);
    assert(budget.rivalAllocations[0].sabotage.percentage == 10);
    assert(budget.rivalAllocations[1].espionage.percentage == 10);
    assert(budget.rivalAllocations[1].sabotage.percentage == 10);

    // Case B: Foreign slider adjustment keeps Internal Security fixed
    ok = budget.setRivalAllocation(2, IntelArea::Espionage, 22);
    assert(ok);
    assert(budget.internalSecurity.percentage == 60 && "Internal Security MUST stay fixed when editing foreign slider");
    assert(budget.totalPercentage() == 100);
    assert(budget.rivalAllocations[0].espionage.percentage == 22);
    assert(budget.rivalAllocations[0].sabotage.percentage == 6);
    assert(budget.rivalAllocations[1].espionage.percentage == 6);
    assert(budget.rivalAllocations[1].sabotage.percentage == 6);

    // Case C: Locking a foreign slider protects it from redistribution
    budget.setLock(3, IntelArea::Sabotage, true);
    ok = budget.setRivalAllocation(2, IntelArea::Espionage, 26);
    assert(ok);
    assert(budget.internalSecurity.percentage == 60);
    assert(budget.rivalAllocations[1].sabotage.percentage == 6 && "Locked slider must not change!");
    assert(budget.rivalAllocations[0].sabotage.percentage == 4);
    assert(budget.rivalAllocations[1].espionage.percentage == 4);
    assert(budget.totalPercentage() == 100);

    std::cout << "  PASS: lock redistribution and balance preservation verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 3: 21-Turn Rolling Window Drops Oldest
// -----------------------------------------------------------------------------
void testRollingWindowDropsOldest() {
    std::cout << "Testing: 21-turn rolling window drops oldest turn..." << std::endl;

    EmpireIntelState state;
    state.empireId = 1;
    state.budget.setDefault({2});
    IntelligenceRules rules;
    rules.maxHistoryTurns = 20;

    for (int t = 1; t <= 20; ++t) {
        state.recordSecurityOutput(10, rules.maxHistoryTurns);
    }
    assert(state.internalSecurityHistory.size() == 20);
    assert(state.getRollingSecuritySum() == 200);

    state.recordSecurityOutput(50, rules.maxHistoryTurns);
    assert(state.internalSecurityHistory.size() == 20 && "History must remain capped at maxHistoryTurns (20)");
    assert(state.getRollingSecuritySum() == 240);
    assert(state.internalSecurityHistory.back() == 50);

    state.recordSecurityOutput(100, rules.maxHistoryTurns);
    assert(state.internalSecurityHistory.size() == 20);
    assert(state.getRollingSecuritySum() == 330);

    std::cout << "  PASS: rolling 20-turn window strictly drops oldest entries." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 4: Stockpile Threshold vs Success Separation & Combined Attack Strength
// -----------------------------------------------------------------------------
void testStockpileThresholdVsSuccessSeparate() {
    std::cout << "Testing: stockpile threshold vs success separation & combined attack strength..." << std::endl;

    EmpireIntelState attacker;
    attacker.empireId = 1;
    attacker.initializeDefault({2});
    attacker.budget.setInternalSecurity(50);
    attacker.budget.setRivalAllocation(2, IntelArea::Sabotage, 0);
    attacker.budget.setRivalAllocation(2, IntelArea::Espionage, 50);
    assert(attacker.budget.isValid());

    IntelligenceRules rules;
    rules.baseAttackThreshold = 200;
    rules.attackDelayTurns = 0;

    TurnContext ctx;
    ctx.currentTurn = 1;
    ctx.totalEmpireBaseIntelOutput = 40;
    ctx.buildingBonusPercent = 0;

    RivalContext rCtx;
    rCtx.rivalId = 2;
    rCtx.currentCredits = 1000;
    ctx.knownRivals.push_back(rCtx);

    std::map<int, DefenderSecurityContext> defContexts;
    DefenderSecurityContext dSec;
    dSec.rollingSecuritySum = 1000;
    dSec.securityStockpile = 1000;
    defContexts[2] = dSec;

    uint64_t seed = 42;

    for (int t = 1; t <= 9; ++t) {
        ctx.currentTurn = t;
        auto ops = advanceTurn(attacker, ctx, defContexts, rules, seed);
        assert(ops.empty() && "No operation should trigger while stockpile < threshold");
    }

    RivalIntelState* rState = attacker.getRivalState(2);
    assert(rState != nullptr);
    assert(rState->espionageStockpile == 180);

    ctx.currentTurn = 10;
    auto ops = advanceTurn(attacker, ctx, defContexts, rules, seed);
    assert(ops.size() == 1 && "Operation attempt must trigger when threshold reached");
    assert(rState->espionageStockpile == 0 && "Stockpile must be cleared upon attack attempt");

    rState->recordSabotageOutput(50, rules.maxHistoryTurns);
    assert(rState->getCombinedRollingAttackSum() == rState->getRollingEspionageSum() + 50);

    std::cout << "  PASS: readiness threshold and outcome evaluation strictly decoupled, combined strength confirmed." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 5: Zero-Allocation Decay and Negative History Contribution
// -----------------------------------------------------------------------------
void testZeroAllocationDecayAndNegativeHistory() {
    std::cout << "Testing: zero-allocation decay and negative history contribution..." << std::endl;

    EmpireIntelState attacker;
    attacker.empireId = 1;
    attacker.initializeDefault({2});
    attacker.budget.setInternalSecurity(100);
    assert(attacker.budget.isValid());

    RivalIntelState* rState = attacker.getRivalState(2);
    assert(rState != nullptr);
    rState->espionageStockpile = 100;

    IntelligenceRules rules;
    rules.decayRatePercent = 2;

    TurnContext ctx;
    ctx.totalEmpireBaseIntelOutput = 1000;
    ctx.buildingBonusPercent = 0;

    std::map<int, DefenderSecurityContext> defContexts;
    uint64_t seed = 12345;

    ctx.currentTurn = 1;
    advanceTurn(attacker, ctx, defContexts, rules, seed);
    assert(rState->espionageStockpile == 80);
    assert(!rState->espionageHistory.empty());
    assert(rState->espionageHistory.back() == -20 && "Zero-allocation must contribute negative 2% to history!");

    ctx.currentTurn = 2;
    advanceTurn(attacker, ctx, defContexts, rules, seed);
    assert(rState->espionageStockpile == 60);
    assert(rState->espionageHistory.back() == -20);

    for (int t = 3; t <= 5; ++t) {
        ctx.currentTurn = t;
        advanceTurn(attacker, ctx, defContexts, rules, seed);
    }
    assert(rState->espionageStockpile == 0);

    ctx.currentTurn = 6;
    advanceTurn(attacker, ctx, defContexts, rules, seed);
    assert(rState->espionageStockpile == 0);

    std::cout << "  PASS: zero-allocation stockpile decay (clamped to 0) and negative history verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 6: Internal Security Stockpile Weighted Running Average
// -----------------------------------------------------------------------------
void testInternalSecurityStockpileWeightedAverage() {
    std::cout << "Testing: internal security stockpile weighted running average..." << std::endl;

    EmpireIntelState state;
    state.empireId = 1;
    state.initializeDefault({2});
    state.budget.setInternalSecurity(80);

    IntelligenceRules rules;
    TurnContext ctx;
    ctx.totalEmpireBaseIntelOutput = 100;
    ctx.buildingBonusPercent = 0;
    std::map<int, DefenderSecurityContext> defContexts;
    uint64_t seed = 999;

    for (int t = 1; t <= 10; ++t) {
        ctx.currentTurn = t;
        advanceTurn(state, ctx, defContexts, rules, seed);
        assert(state.internalSecurityStockpile == 80 && "Constant output must yield constant weighted average!");
        assert(state.securityTurnCounter == t);
    }

    ctx.currentTurn = 11;
    ctx.totalEmpireBaseIntelOutput = 150;
    advanceTurn(state, ctx, defContexts, rules, seed);
    assert(state.internalSecurityStockpile == 83);
    assert(state.securityTurnCounter == 11);

    std::cout << "  PASS: running weighted average strictly maintained without infinite accumulation." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 7: Proportional Bug Regression
// -----------------------------------------------------------------------------
void testProportionalBonusBugRegression() {
    std::cout << "Testing: proportional bonus bug regression (1000 * 40% * 1.5 = 600)..." << std::endl;

    int64_t baseOutput = 1000;
    int allocPct = 40;
    int bonusPct = 50;

    int64_t correctedOutput = calculateAreaOutput(baseOutput, allocPct, bonusPct, BonusModel::ProportionalCorrected);
    assert(correctedOutput == 600);

    int64_t legacyBugOutput = calculateAreaOutput(baseOutput, allocPct, bonusPct, BonusModel::LegacyAdditiveParity);
    assert(legacyBugOutput == 900);

    std::cout << "  Corrected output: " << correctedOutput << " (expected 600)" << std::endl;
    std::cout << "  Legacy bug output: " << legacyBugOutput << " (expected 900)" << std::endl;
    std::cout << "  PASS: proportional bonus bug regression confirmed." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 8: Deterministic Save/Reload
// -----------------------------------------------------------------------------
void testDeterministicSaveReload() {
    std::cout << "Testing: deterministic save/reload round-trip..." << std::endl;

    EmpireIntelState original;
    original.empireId = 1;
    original.initializeDefault({2, 3});
    original.budget.setInternalSecurity(70);
    original.budget.setLock(2, IntelArea::Espionage, true);
    original.internalSecurityStockpile = 345;
    original.securityTurnCounter = 14;
    original.internalSecurityHistory = {10, 20, 30, 40, 50};

    RivalIntelState* r2 = original.getRivalState(2);
    assert(r2 != nullptr);
    r2->espionageStockpile = 88;
    r2->sabotageStockpile = 12;
    r2->lastEspionageAttackTurn = 5;
    r2->lastSabotageAttackTurn = 3;
    r2->espionageHistory = {-20, 5, 8, 12};
    r2->sabotageHistory = {1, 2, 3};
    r2->focusDepartment = IntelDepartment::Military;
    r2->departmentWeights = {{5, 10, 15, 30}};

    std::string savedData = serializeIntelState(original);
    assert(!savedData.empty());

    EmpireIntelState loaded;
    std::string error;
    bool ok = deserializeIntelState(savedData, loaded, &error);
    if (!ok) {
        std::cerr << "Deserialization failed: " << error << std::endl;
    }
    assert(ok);

    assert(loaded == original);

    std::string resavedData = serializeIntelState(loaded);
    assert(savedData == resavedData);

    std::cout << "  PASS: byte-exact and logic-exact save/reload verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 9: Malformed Save & Strict Parser Rejection
// -----------------------------------------------------------------------------
void testMalformedSaveRejection() {
    std::cout << "Testing: malformed save and strict parser rejection..." << std::endl;

    EmpireIntelState valid;
    valid.empireId = 1;
    valid.initializeDefault({2});
    std::string validSave = serializeIntelState(valid);

    EmpireIntelState out;
    std::string err;

    // 1. Missing header
    assert(!deserializeIntelState("INVALID_HEADER\nempireId 1", out, &err));

    // 2. Truncated save
    assert(!deserializeIntelState("BOTF_INTEL_V1\nempireId 1\nsecStockpile 10 1", out, &err));

    // 3. Negative empire ID
    std::string badEmpireId = validSave;
    size_t pos = badEmpireId.find("empireId 1");
    badEmpireId.replace(pos, 10, "empireId -5");
    assert(!deserializeIntelState(badEmpireId, out, &err));

    // 4. Duplicate rival IDs
    std::string dupRivals =
        "BOTF_INTEL_V1\n"
        "empireId 1\n"
        "secStockpile 80 1\n"
        "secAlloc 80 0\n"
        "secHistory 0\n"
        "rivalCount 2\n"
        "rival 2 5 0 5 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "rival 2 5 0 5 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "END_BOTF_INTEL\n";
    assert(!deserializeIntelState(dupRivals, out, &err));

    // 5. Rival ID equal to own empire ID
    std::string selfRival =
        "BOTF_INTEL_V1\n"
        "empireId 1\n"
        "secStockpile 80 1\n"
        "secAlloc 80 0\n"
        "secHistory 0\n"
        "rivalCount 1\n"
        "rival 1 10 0 10 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "END_BOTF_INTEL\n";
    assert(!deserializeIntelState(selfRival, out, &err));

    // 6. Non-boolean lock flag
    std::string badLock =
        "BOTF_INTEL_V1\n"
        "empireId 1\n"
        "secStockpile 80 1\n"
        "secAlloc 80 7\n"
        "secHistory 0\n"
        "rivalCount 1\n"
        "rival 2 10 0 10 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "END_BOTF_INTEL\n";
    assert(!deserializeIntelState(badLock, out, &err));

    // 7. Negative or out-of-range training weights (-10, 30, 40, 40)
    std::string badWeights =
        "BOTF_INTEL_V1\n"
        "empireId 1\n"
        "secStockpile 80 1\n"
        "secAlloc 80 0\n"
        "secHistory 0\n"
        "rivalCount 1\n"
        "rival 2 10 0 10 0\n"
        "rData 0 0 -100 -100 0 -10 30 40 40\n"
        "espHist 0\nsabHist 0\n"
        "END_BOTF_INTEL\n";
    assert(!deserializeIntelState(badWeights, out, &err));

    // 8. Trailing junk after END_BOTF_INTEL
    std::string trailingJunk = validSave + "extra_junk_data\n";
    assert(!deserializeIntelState(trailingJunk, out, &err));

    // 9. More than 4 rivals
    std::string tooManyRivals =
        "BOTF_INTEL_V1\n"
        "empireId 1\n"
        "secStockpile 80 1\n"
        "secAlloc 80 0\n"
        "secHistory 0\n"
        "rivalCount 5\n"
        "rival 2 2 0 2 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "rival 3 2 0 2 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "rival 4 2 0 2 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "rival 5 2 0 2 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "rival 6 2 0 2 0\n"
        "rData 0 0 -100 -100 0 10 10 10 10\nespHist 0\nsabHist 0\n"
        "END_BOTF_INTEL\n";
    assert(!deserializeIntelState(tooManyRivals, out, &err));

    std::cout << "  PASS: all malformed saves, invalid flags, out-of-range weights, and trailing tokens rejected." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 10: Department Counters Range 1..30 & S16 General +2 All Training
// -----------------------------------------------------------------------------
void testDepartmentCountersSourcedRange() {
    std::cout << "Testing: department counters range 1..30 & S16 General +2 all training..." << std::endl;

    IntelligenceRules rules;

    // Case A: General focus adds +2 to ALL 4 departments (S16 lines 62-65)
    std::array<int, 4> genWeights = {{10, 10, 10, 10}};
    trainDepartmentWeights(genWeights, IntelDepartment::General, rules);
    assert(genWeights[0] == 12 && genWeights[1] == 12 && genWeights[2] == 12 && genWeights[3] == 12);
    std::cout << "  General focus (+2 all): Gen=" << genWeights[0] << ", Eco=" << genWeights[1]
              << ", Sci=" << genWeights[2] << ", Mil=" << genWeights[3] << std::endl;

    // Case B: Targeted focus (Military) adds +2 to selected, -2 to other 3 (min 1)
    std::array<int, 4> milWeights = {{10, 10, 10, 10}};
    trainDepartmentWeights(milWeights, IntelDepartment::Military, rules);
    assert(milWeights[3] == 12);
    assert(milWeights[0] == 8 && milWeights[1] == 8 && milWeights[2] == 8);
    std::cout << "  Military focus (+2 selected, -2 others): Gen=" << milWeights[0] << ", Eco=" << milWeights[1]
              << ", Sci=" << milWeights[2] << ", Mil=" << milWeights[3] << std::endl;

    for (int i = 0; i < 20; ++i) {
        trainDepartmentWeights(milWeights, IntelDepartment::Military, rules);
    }
    assert(milWeights[3] == 30 && milWeights[3] >= 1);
    assert(milWeights[0] == 1 && milWeights[1] == 1 && milWeights[2] == 1);

    uint64_t rng = 1337;
    int counts[4] = {0, 0, 0, 0};
    const int draws = 10000;
    for (int i = 0; i < draws; ++i) {
        IntelDepartment dept = selectDepartment(milWeights, rng);
        counts[static_cast<int>(dept)]++;
    }

    int totalCounter = milWeights[0] + milWeights[1] + milWeights[2] + milWeights[3];
    double milExpected = static_cast<double>(milWeights[3]) / static_cast<double>(totalCounter);
    double milActual = static_cast<double>(counts[3]) / draws;
    std::cout << "  Military draw frequency: " << milActual * 100.0 << "% (Expected: ~"
              << milExpected * 100.0 << "%)" << std::endl;

    assert(std::abs(milActual - milExpected) < 0.02);

    std::cout << "  PASS: S16 department counters [1..30], General +2 all, and dynamic weighted draw verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 11: Defender Tech & Morale Unified Authority (No Double Counting)
// -----------------------------------------------------------------------------
void testDefenderTechAndMoraleUnifiedAuthority() {
    std::cout << "Testing: defender tech & morale unified authority (no double counting)..." << std::endl;

    EmpireIntelState attacker;
    attacker.empireId = 1;
    attacker.initializeDefault({2});
    attacker.budget.setInternalSecurity(50);
    attacker.budget.setRivalAllocation(2, IntelArea::Espionage, 50);

    IntelligenceRules rules;
    rules.baseAttackThreshold = 50;
    rules.attackDelayTurns = 0;

    TurnContext ctx;
    ctx.currentTurn = 1;
    ctx.attackerComputerTechLevel = 4;
    ctx.totalEmpireBaseIntelOutput = 100;

    RivalContext rCtx;
    rCtx.rivalId = 2;
    rCtx.computerTechLevel = 2;
    ctx.knownRivals.push_back(rCtx);

    std::map<int, DefenderSecurityContext> defContexts;
    DefenderSecurityContext dSec;
    dSec.rollingSecuritySum = 100;
    dSec.securityStockpile = 50;
    dSec.computerTechLevel = 2;
    dSec.defenderMoralePercent = 100;
    defContexts[2] = dSec;

    uint64_t seed1 = 4242;
    uint64_t seedCopy = seed1;

    auto ops = advanceTurn(attacker, ctx, defContexts, rules, seed1);
    assert(ops.size() == 1);

    int expectedTechDiff = 4 - 2; // 2 (NOT 4 - 2 - 2 = 0)
    const int64_t totalDef = calculateDefenseStrength(dSec.rollingSecuritySum, dSec.securityStockpile, dSec.defenderMoralePercent, rules);

    selectDepartment(attacker.getRivalState(2)->departmentWeights, seedCopy);

    OperationRecord directOp = resolveAttempt(
        1, 2, IntelArea::Espionage, ops[0].department, 1,
        concentratedAttackStrength(*attacker.getRivalState(2), rules),
        totalDef,
        expectedTechDiff, TreatyLevel::None,
        0, 0, seedCopy, rules
    );

    assert(ops[0] == directOp);
    std::cout << "  PASS: defender tech/morale resolved authoritatively without duplicate penalty." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 12: S16 Defense Strength Calculation Regression & Morale Multiplier
// -----------------------------------------------------------------------------
void testS16DefenseStrengthCalculationRegression() {
    std::cout << "Testing: S16 defense strength calculation regression ((last20 / 20) + stockpile * 0.02) * morale..." << std::endl;

    IntelligenceRules rules;
    assert(rules.securityHistoryDivisor == 20);
    assert(rules.securityStockpilePermille == 20);

    int64_t rollingSec = 200;
    int64_t secStockpile = 500;

    // S16 formula with default morale 100%: (200 / 20) + (500 * 0.02) = 10.0 + 10.0 = 20.0 (20000 milli-points)
    const int64_t defStrength = calculateDefenseStrength(rollingSec, secStockpile, 100, rules);
    assert(defStrength == 20000);
    assert(defStrength != 325000);

    // Morale 125% boosts defense to 25.0 exactly
    const int64_t boostedDef = calculateDefenseStrength(rollingSec, secStockpile, 125, rules);
    assert(boostedDef == 25000);

    // Morale 80% lowers defense to 16.0 exactly
    const int64_t weakenedDef = calculateDefenseStrength(rollingSec, secStockpile, 80, rules);
    assert(weakenedDef == 16000);

    // Live advanceTurn regression: verify advanceTurn uses this denominator
    EmpireIntelState attacker;
    attacker.empireId = 1;
    attacker.initializeDefault({2});
    attacker.budget.setInternalSecurity(50);
    attacker.budget.setRivalAllocation(2, IntelArea::Espionage, 50);

    rules.baseAttackThreshold = 50;
    rules.attackDelayTurns = 0;

    TurnContext ctx;
    ctx.currentTurn = 1;
    ctx.totalEmpireBaseIntelOutput = 100;

    std::map<int, DefenderSecurityContext> defContexts;
    DefenderSecurityContext dSec;
    dSec.rollingSecuritySum = rollingSec;
    dSec.securityStockpile = secStockpile;
    dSec.defenderMoralePercent = 100;
    defContexts[2] = dSec;

    uint64_t seed1 = 7777;
    uint64_t seedCopy = seed1;

    auto ops = advanceTurn(attacker, ctx, defContexts, rules, seed1);
    assert(ops.size() == 1);

    selectDepartment(attacker.getRivalState(2)->departmentWeights, seedCopy);
    OperationRecord expectedOp = resolveAttempt(
        1, 2, IntelArea::Espionage, ops[0].department, 1,
        concentratedAttackStrength(*attacker.getRivalState(2), rules),
        20000,
        0, TreatyLevel::None,
        0, 0, seedCopy, rules
    );

    assert(ops[0] == expectedOp);
    std::cout << "  Calculated defense strength: " << defStrength << " (expected 20.0, legacy bug would be 325.0)" << std::endl;
    std::cout << "  PASS: advanceTurn strictly uses S16 defense denominator with multiplicative morale factor." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 13: Signed Attack Strength Preserved and Reduces Odds
// -----------------------------------------------------------------------------
void testSignedAttackStrengthReducesOdds() {
    std::cout << "Testing: signed attack strength preserved and reduces odds..." << std::endl;

    EmpireIntelState state;
    state.empireId = 1;
    state.initializeDefault({2});

    RivalIntelState* rState = state.getRivalState(2);
    assert(rState != nullptr);

    // Record two negative 2% contributions: -20 and -30
    rState->recordEspionageOutput(-20, 20);
    rState->recordSabotageOutput(-30, 20);

    // getCombinedRollingAttackSum MUST NOT clamp to 0; must return -50!
    int64_t combinedSum = rState->getCombinedRollingAttackSum();
    assert(combinedSum == -50 && "getCombinedRollingAttackSum must preserve signed negative values!");
    std::cout << "  Preserved signed combined attack strength: " << combinedSum << std::endl;

    // Compare resolveAttempt with neutral attackStrength (0) vs negative attackStrength (-50)
    // with attacker tech advantage (+20% techBonus) under identical PRNG seed.
    uint64_t seed1 = 987654321ULL;
    uint64_t seed2 = seed1;

    // Run A: attackStrength = 0
    OperationRecord opNeutral = resolveAttempt(
        1, 2, IntelArea::Espionage, IntelDepartment::Economic, 1,
        0, 20000, 2, TreatyLevel::None, 1000, 1000, seed1
    );

    // Run B: attackStrength = -50 points average (penalized by unallocated zero-area decay history)
    OperationRecord opNegative = resolveAttempt(
        1, 2, IntelArea::Espionage, IntelDepartment::Economic, 1,
        -50000, 20000, 2, TreatyLevel::None, 1000, 1000, seed2
    );

    std::cout << "  Neutral attack (0 points) outcome:    " << outcomeName(opNeutral.outcome) << std::endl;
    std::cout << "  Penalized attack (-50 points) outcome: " << outcomeName(opNegative.outcome) << std::endl;

    // The negative points must strictly reduce the odds/outcome compared to 0 points
    assert(static_cast<int>(opNeutral.outcome) >= static_cast<int>(opNegative.outcome));
    assert(opNegative.outcome == IntelOutcome::UndetectedFailure || opNegative.outcome == IntelOutcome::Fiasco);

    std::cout << "  PASS: negative history contributions reduce success odds prior to probability clamping." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 14: MaxBalance and Overflow Safety
// -----------------------------------------------------------------------------
void testMaxBalanceAndOverflowSafety() {
    std::cout << "Testing: MaxBalance (10^12) bounds, overflow safety, and adversarial rejection..." << std::endl;

    // 1. Test overflow-safe running average formula at extreme values
    EmpireIntelState extremeState;
    extremeState.empireId = 1;
    extremeState.initializeDefault({2});
    extremeState.internalSecurityStockpile = MaxIntelBalance - 100;
    extremeState.securityTurnCounter = MaxIntelTurns - 1;

    IntelligenceRules rules;
    TurnContext ctx;
    ctx.totalEmpireBaseIntelOutput = 1000;
    std::map<int, DefenderSecurityContext> defContexts;
    uint64_t seed = 555;

    // Advance turn: must not overflow or wrap into negative numbers
    advanceTurn(extremeState, ctx, defContexts, rules, seed);
    assert(extremeState.internalSecurityStockpile > 0 && extremeState.internalSecurityStockpile <= MaxIntelBalance);
    assert(extremeState.securityTurnCounter <= MaxIntelTurns);
    std::cout << "  Extreme weighted average safe: stockpile=" << extremeState.internalSecurityStockpile
              << ", turns=" << extremeState.securityTurnCounter << std::endl;

    // 2. Adversarial Deserializer Rejections
    EmpireIntelState valid;
    valid.empireId = 1;
    valid.initializeDefault({2});
    std::string validSave = serializeIntelState(valid);

    EmpireIntelState out;
    std::string err;

    // Case A: Stockpile exceeds MaxIntelBalance (e.g. 1000000000001)
    std::string badStockpile = validSave;
    size_t pos = badStockpile.find("secStockpile 0 0");
    badStockpile.replace(pos, 16, "secStockpile 1000000000001 0");
    assert(!deserializeIntelState(badStockpile, out, &err) && "Must reject secStockpile > MaxIntelBalance");

    // Case B: Turn counter exceeds MaxIntelTurns (e.g. 1000001)
    std::string badTurnCounter = validSave;
    pos = badTurnCounter.find("secStockpile 0 0");
    badTurnCounter.replace(pos, 16, "secStockpile 0 1000001");
    assert(!deserializeIntelState(badTurnCounter, out, &err) && "Must reject securityTurnCounter > MaxIntelTurns");

    // Case C: Last attack turn below -100 sentinel
    std::string badLastAttack = validSave;
    pos = badLastAttack.find("-100 -100");
    badLastAttack.replace(pos, 9, "-105 -100");
    assert(!deserializeIntelState(badLastAttack, out, &err) && "Must reject last attack turn < -100");

    // Case D: Last attack turn above MaxIntelTurns
    std::string badLastAttackHigh = validSave;
    pos = badLastAttackHigh.find("-100 -100");
    badLastAttackHigh.replace(pos, 9, "1000005 -100");
    assert(!deserializeIntelState(badLastAttackHigh, out, &err) && "Must reject last attack turn > MaxIntelTurns");

    // Case E: History entry exceeding MaxIntelBalance
    std::string badHistory = validSave;
    pos = badHistory.find("secHistory 0");
    badHistory.replace(pos, 12, "secHistory 1 1000000000005");
    assert(!deserializeIntelState(badHistory, out, &err) && "Must reject history value > MaxIntelBalance");

    std::cout << "  PASS: MaxBalance core bounds strictly enforced; signed overflow mathematically prevented." << std::endl;
}

// =============================================================================
// Lane B regression suites
// =============================================================================

// Bug 11: double + floor misrounded (100 x 29% gave 28) and outcomes used doubles.
void testIntegerOutputIsExact() {
    std::cout << "Testing: integer area output and exact defense math (bug 11)..." << std::endl;
    assert(calculateAreaOutput(100, 29, 0, BonusModel::ProportionalCorrected) == 29);
    assert(calculateAreaOutput(100, 57, 0, BonusModel::ProportionalCorrected) == 57);
    for (int pct = 1; pct <= 100; ++pct) {
        assert(calculateAreaOutput(100, pct, 0, BonusModel::ProportionalCorrected) == pct);
        assert(calculateAreaOutput(1000, pct, 10, BonusModel::ProportionalCorrected) == pct * 11);
    }
    assert(calculateAreaOutput(333, 33, 33, BonusModel::ProportionalCorrected) == (333 * 33 * 133) / 10000);
    assert(calculateAreaOutput(100, 29, 0, BonusModel::LegacyAdditiveParity) == 29);
    assert(calculateAreaOutput(100, 90, 50, BonusModel::LegacyAdditiveParity) == 100);
    assert(calculateAreaOutput(100, 50, -200, BonusModel::ProportionalCorrected) == 0); // Bonus clamps at -100%
    // Saturating and overflow-free at the bounds.
    assert(calculateAreaOutput(MaxIntelBalance, 100, 1000, BonusModel::ProportionalCorrected) == MaxIntelBalance);
    assert(calculateAreaOutput(INT64_MAX, 100, 0, BonusModel::ProportionalCorrected) == MaxIntelBalance);

    IntelligenceRules rules;
    // 199 / 20 = 9.95 -> 9950 milli-points exactly; 7 * 0.02 = 0.14 -> 140.
    assert(calculateDefenseStrength(199, 7, 100, rules) == 10090);
    assert(calculateDefenseStrength(MaxIntelBalance * 20, MaxIntelBalance, 500, rules) > 0);
    assert(intelStrengthRatioPermille(10000, 20000, 0) == 500);
    assert(intelStrengthRatioPermille(10000, 20000, 3) == 800);
    assert(intelStrengthRatioPermille(-50000, 20000, 1) == 0);
    assert(intelStrengthRatioPermille(MaxIntelBalance * 1000, 1000, 0) == 1000000);
    std::cout << "  PASS: 100 x 29% = 29; defense and ratios exact in fixed point." << std::endl;
}

namespace {
// Runs `attempts` operations of an attacker holding `attackPct` of output against a single
// rival whose security share is `securityPct` (both empires produce 100 intel per turn).
std::array<int, 5> duel(int attackPct, int securityPct, int attempts, uint64_t seed) {
    IntelligenceRules rules;
    rules.attackDelayTurns = 0;
    rules.baseAttackThreshold = 50;
    EmpireIntelState attacker;
    attacker.empireId = 1;
    attacker.budget.internalSecurity.percentage = 100 - attackPct;
    RivalAllocation alloc;
    alloc.rivalId = 2;
    alloc.espionage.percentage = attackPct;
    attacker.budget.rivalAllocations.push_back(alloc);
    assert(attacker.budget.isValid(1));
    std::map<int, DefenderSecurityContext> defenders;
    DefenderSecurityContext def;
    def.rollingSecuritySum = static_cast<int64_t>(securityPct) * 20; // 20 turns at securityPct of 100 output
    def.securityStockpile = securityPct;
    defenders[2] = def;
    TurnContext ctx;
    ctx.totalEmpireBaseIntelOutput = 100;
    RivalContext rc;
    rc.rivalId = 2;
    rc.currentCredits = 1000;
    ctx.knownRivals.push_back(rc);
    std::array<int, 5> outcomes{};
    int done = 0;
    for (int turn = 1; done < attempts && turn < 5000; ++turn) {
        ctx.currentTurn = turn;
        for (const auto& op : advanceTurn(attacker, ctx, defenders, rules, seed)) {
            if (turn <= 20) continue; // Let the attack history fill its window first
            ++outcomes[static_cast<int>(op.outcome)];
            ++done;
        }
    }
    return outcomes;
}
}

// Bug 12: attack used a 20-turn SUM against a 20-turn AVERAGE defense.
void testSecurityInvestmentMatters() {
    std::cout << "Testing: rebalanced attack vs security and failure exposure (bug 12)..." << std::endl;
    const auto weak = duel(20, 80, 60, 11);
    const int weakSuccesses = weak[2] + weak[3] + weak[4];
    assert(weakSuccesses == 0 && "A 20% operation must not beat 80% security");
    assert(weak[static_cast<int>(IntelOutcome::Fiasco)] > 0 && "Strong security exposes failed operations");

    const auto even = duel(50, 50, 60, 11);
    const int evenSuccesses = even[2] + even[3] + even[4];
    assert(evenSuccesses > 40 && "A concentrated 50% service beats 50% security most of the time");

    // Shallow failures are sometimes exposed too (not only deep fiascos).
    IntelligenceRules rules;
    int exposedShallow = 0, quietShallow = 0;
    for (uint64_t seed = 1; seed <= 400; ++seed) {
        uint64_t rng = seed * 7919ULL;
        // Ratio 700 permille: score spans 420..980, so failures here are shallow ones.
        const auto op = resolveAttempt(1, 2, IntelArea::Espionage, IntelDepartment::General, 1, 7000, 10000, 0,
                                       TreatyLevel::None, 0, 0, rng, rules);
        if (op.outcome == IntelOutcome::Fiasco) {
            ++exposedShallow;
            assert(op.diplomaticIncidentPenalty == 8);
        } else if (op.outcome == IntelOutcome::UndetectedFailure) {
            ++quietShallow;
        }
    }
    assert(exposedShallow > 0 && quietShallow > exposedShallow && "Shallow failures are exposed at ~32%, not always");
    assert(failureDetectionPercent(0, rules) == 90 && failureDetectionPercent(5000, rules) == 10);
    std::cout << "  PASS: 20% vs 80%: " << weakSuccesses << " successes; 50% vs 50%: " << evenSuccesses
              << "/60; shallow failures exposed " << exposedShallow << "/" << (exposedShallow + quietShallow) << "." << std::endl;
}

// Bug 12 (parsing): strtol accepted '%+F'; opType and amounts were unbounded on load.
void testStrictDecodingAndBoundedRecords() {
    std::cout << "Testing: strict hex decoding and bounded operation records (bug 12)..." << std::endl;
    std::string out;
    assert(decodeString("A%20B", out) && out == "A B");
    assert(!decodeString("%+F", out));
    assert(!decodeString("% F", out));
    assert(!decodeString("%-0", out));
    assert(!decodeString("%0x", out));
    assert(!decodeString("%4", out));

    OperationRecord op;
    op.attackerEmpireId = 1;
    op.defenderEmpireId = 2;
    op.opType = IntelOpType::StealCredits;
    op.outcome = IntelOutcome::StandardSuccess;
    op.turn = 12;
    op.creditsStolen = 150;
    op.details = "Stole 150 credits.";
    const std::string line = serializeOperationRecord(op);
    OperationRecord back;
    std::string err;
    assert(deserializeOperationRecord(line, back, &err) && back == op);
    auto mutate = [&](int field, const std::string& value) {
        std::istringstream in(line);
        std::vector<std::string> parts;
        std::string tok;
        while (in >> tok) parts.push_back(tok);
        parts[static_cast<size_t>(field)] = value;
        std::string joined;
        for (size_t i = 0; i < parts.size(); ++i) joined += (i ? " " : "") + parts[i];
        OperationRecord tmp;
        return deserializeOperationRecord(joined, tmp, &err);
    };
    assert(!mutate(5, "7") && "opType beyond StealResearch/SabotageFacility rejected");
    assert(!mutate(7, "-5") && "Negative credits rejected");
    assert(!mutate(7, "1000000000001") && "Credits beyond MaxIntelBalance rejected");
    assert(!mutate(8, "-1") && "Negative research rejected");
    assert(!mutate(10, "4") && "Severity beyond 3 rejected");
    assert(!mutate(11, "2") && "Non-boolean detection flag rejected");
    assert(!mutate(12, "101") && "Penalty beyond 100 rejected");
    assert(!mutate(1, "1") && "Attacker equal to defender rejected");
    assert(!deserializeOperationRecord(line + " trailing", back, &err) && "Trailing tokens rejected");
    std::cout << "  PASS: strict escapes and bounded operation records." << std::endl;
}

// Bug 10: only rivals present in the budget were serialized.
void testAllRivalStateSerialized() {
    std::cout << "Testing: per-rival state outside the budget survives reload (bug 10)..." << std::endl;
    EmpireIntelState state;
    state.empireId = 1;
    state.budget.setDefault({2});
    state.getOrCreateRivalState(2).espionageStockpile = 40;
    {
        auto& unbudgeted = state.getOrCreateRivalState(4);
        unbudgeted.focusDepartment = IntelDepartment::Science;
        unbudgeted.sabotageStockpile = 77;
        unbudgeted.lastEspionageAttackTurn = 9;
        unbudgeted.espionageHistory = {5, -2, 8};
        unbudgeted.departmentWeights = {{3, 4, 25, 6}};
    }
    state.getOrCreateRivalState(3); // Pristine: indistinguishable from absent, omitted
    const RivalIntelState expected = *state.getRivalState(4); // (references above may dangle after growth)
    const std::string text = serializeIntelState(state);
    assert(text.find("extraRivals 1") != std::string::npos && text.find("xrival 4") != std::string::npos);
    EmpireIntelState loaded;
    std::string err;
    assert(deserializeIntelState(text, loaded, &err));
    const auto* r4 = loaded.getRivalState(4);
    assert(r4 && *r4 == expected && "Focus, stockpiles, clocks and history of rival 4 preserved");
    assert(loaded.getRivalState(2)->espionageStockpile == 40);
    assert(serializeIntelState(loaded) == text && "Canonical re-serialization");

    // Saves without extra rivals keep their exact old format.
    EmpireIntelState plain;
    plain.empireId = 1;
    plain.initializeDefault({2, 3});
    const std::string plainText = serializeIntelState(plain);
    assert(plainText.find("extraRivals") == std::string::npos);

    std::string bad = text;
    bad.replace(bad.find("xrival 4"), 8, "xrival 1");
    assert(!deserializeIntelState(bad, loaded, &err) && "Self as extra rival rejected");
    bad = text;
    bad.replace(bad.find("xrival 4"), 8, "xrival 2");
    assert(!deserializeIntelState(bad, loaded, &err) && "Duplicate extra rival rejected");
    std::cout << "  PASS: all per-rival state roundtrips; pristine rows omitted." << std::endl;
}

// Bug 13: AI intelligence budgets never changed from the defaults.
void testAdaptiveAiIntelDoctrines() {
    std::cout << "Testing: adaptive AI intelligence doctrines (bug 13)..." << std::endl;
    auto view = [](int id) {
        AiIntelRivalView v;
        v.rivalId = id;
        v.ownPower = 100;
        v.rivalPower = 100;
        return v;
    };
    std::vector<AiIntelRivalView> peace{view(1), view(3), view(4)};
    const auto fed = planAiIntelStrategy(1 + 1, IntelDoctrine::Federation, peace);
    assert(fed.budget.isValid(2) && fed.budget.internalSecurity.percentage == 80);
    for (const auto& a : fed.budget.rivalAllocations) assert(a.sabotage.percentage == 0 && "No peacetime sabotage for the Federation");
    assert(fed.budget.rivalAllocations.size() <= AiIntelMaxTargets);

    auto war = peace;
    war[1].atWar = true; // At war with empire 3
    const auto klingon = planAiIntelStrategy(2, IntelDoctrine::Klingon, war);
    assert(klingon.budget.isValid(2));
    const RivalAllocation* enemy = nullptr;
    for (const auto& a : klingon.budget.rivalAllocations) if (a.rivalId == 3) enemy = &a;
    assert(enemy && enemy->sabotage.percentage > enemy->espionage.percentage && "Klingons sabotage their enemies");
    assert(klingon.focus.at(3) == IntelDepartment::Military);
    int enemyShare = enemy->espionage.percentage + enemy->sabotage.percentage;
    for (const auto& a : klingon.budget.rivalAllocations) {
        if (a.rivalId != 3) assert(a.espionage.percentage + a.sabotage.percentage <= enemyShare && "Enemies get the largest share");
    }

    const auto ferengi = planAiIntelStrategy(5, IntelDoctrine::Ferengi, peace);
    for (const auto& [id, dept] : ferengi.focus) assert(dept == IntelDepartment::Economic && "Ferengi steal credits");

    auto allies = peace;
    for (auto& v : allies) v.treaty = TreatyLevel::Alliance;
    assert(planAiIntelStrategy(2, IntelDoctrine::Federation, allies).budget.rivalAllocations.empty() && "No spying on allies");
    assert(!planAiIntelStrategy(3, IntelDoctrine::Romulan, allies).budget.rivalAllocations.empty() && "The Tal Shiar spies on everyone");

    auto spied = peace;
    spied[0].incidentsAgainstUs = 3;
    const auto alarmed = planAiIntelStrategy(2, IntelDoctrine::Cardassian, spied);
    assert(alarmed.budget.internalSecurity.percentage > planAiIntelStrategy(2, IntelDoctrine::Cardassian, peace).budget.internalSecurity.percentage &&
           "Detected spies raise internal security");

    auto burned = war;
    burned[1].exposedOperations = 3;
    burned[1].atWar = false;
    const auto cautious = planAiIntelStrategy(2, IntelDoctrine::Romulan, burned);
    for (const auto& a : cautious.budget.rivalAllocations) assert(a.rivalId != 3 && "Back off after repeated exposure");

    auto dead = peace;
    for (auto& v : dead) v.eliminated = true;
    const auto none = planAiIntelStrategy(2, IntelDoctrine::Klingon, dead);
    assert(none.budget.internalSecurity.percentage == 100 && none.budget.rivalAllocations.empty());

    EmpireIntelState applied;
    applied.empireId = 2;
    applied.getOrCreateRivalState(3).espionageStockpile = 99;
    applyAiIntelPlan(applied, klingon);
    assert(applied.budget == klingon.budget && applied.getRivalState(3)->espionageStockpile == 99 &&
           applied.getRivalState(3)->focusDepartment == IntelDepartment::Military && "Plans keep per-rival history");
    assert(planAiIntelStrategy(2, IntelDoctrine::Klingon, war) == klingon && "Deterministic");
    std::cout << "  PASS: doctrine, war, alliance, retaliation and back-off behaviour." << std::endl;
}

// -----------------------------------------------------------------------------
// Main Runner
// -----------------------------------------------------------------------------
int main() {
    std::cout << "=======================================================" << std::endl;
    std::cout << "BOTF Intelligence Subsystem Verification Suite" << std::endl;
    std::cout << "=======================================================" << std::endl;

    try {
        testInvalidBudgetsNoMutation();
        testLockRedistribution();
        testRollingWindowDropsOldest();
        testStockpileThresholdVsSuccessSeparate();
        testZeroAllocationDecayAndNegativeHistory();
        testInternalSecurityStockpileWeightedAverage();
        testProportionalBonusBugRegression();
        testDeterministicSaveReload();
        testMalformedSaveRejection();
        testDepartmentCountersSourcedRange();
        testDefenderTechAndMoraleUnifiedAuthority();
        testS16DefenseStrengthCalculationRegression();
        testSignedAttackStrengthReducesOdds();
        testMaxBalanceAndOverflowSafety();
        testIntegerOutputIsExact();
        testSecurityInvestmentMatters();
        testStrictDecodingAndBoundedRecords();
        testAllRivalStateSerialized();
        testAdaptiveAiIntelDoctrines();

        std::cout << "\n>>> ALL 19 INTELLIGENCE TEST SUITES PASSED CLEANLY! <<<" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL with exception: " << ex.what() << std::endl;
        return 1;
    }
}
