#include "Intelligence.h"

#include <cassert>
#include <cmath>
#include <iostream>
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
    dSec.defenderMoraleMultiplier = 1.0;
    defContexts[2] = dSec;

    uint64_t seed1 = 4242;
    uint64_t seedCopy = seed1;

    auto ops = advanceTurn(attacker, ctx, defContexts, rules, seed1);
    assert(ops.size() == 1);

    int expectedTechDiff = 4 - 2; // 2 (NOT 4 - 2 - 2 = 0)
    double totalDef = calculateDefenseStrength(dSec.rollingSecuritySum, dSec.securityStockpile, dSec.defenderMoraleMultiplier, rules);

    selectDepartment(attacker.getRivalState(2)->departmentWeights, seedCopy);

    OperationRecord directOp = resolveAttempt(
        1, 2, IntelArea::Espionage, ops[0].department, 1,
        attacker.getRivalState(2)->getCombinedRollingAttackSum(),
        totalDef,
        expectedTechDiff, TreatyLevel::None,
        0, 0, seedCopy
    );

    assert(ops[0].outcome == directOp.outcome);
    std::cout << "  PASS: defender tech/morale resolved authoritatively without duplicate penalty." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 12: S16 Defense Strength Calculation Regression & Morale Multiplier
// -----------------------------------------------------------------------------
void testS16DefenseStrengthCalculationRegression() {
    std::cout << "Testing: S16 defense strength calculation regression ((last20 / 20) + stockpile * 0.02) * morale..." << std::endl;

    IntelligenceRules rules;
    assert(rules.securityHistoryDivisor == 20);
    assert(std::abs(rules.securityStockpileCoefficient - 0.02) < 1e-6);

    int64_t rollingSec = 200;
    int64_t secStockpile = 500;

    // S16 formula with default morale 1.0: (200 / 20) + (500 * 0.02) = 10.0 + 10.0 = 20.0
    double defStrength = calculateDefenseStrength(rollingSec, secStockpile, 1.0, rules);
    assert(std::abs(defStrength - 20.0) < 1e-6);
    assert(defStrength != 325.0);

    // Morale multiplier verification: 1.25x morale boosts defense to 25.0
    double boostedDef = calculateDefenseStrength(rollingSec, secStockpile, 1.25, rules);
    assert(std::abs(boostedDef - 25.0) < 1e-6);

    // Morale multiplier verification: 0.8x morale lowers defense to 16.0
    double weakenedDef = calculateDefenseStrength(rollingSec, secStockpile, 0.8, rules);
    assert(std::abs(weakenedDef - 16.0) < 1e-6);

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
    dSec.defenderMoraleMultiplier = 1.0;
    defContexts[2] = dSec;

    uint64_t seed1 = 7777;
    uint64_t seedCopy = seed1;

    auto ops = advanceTurn(attacker, ctx, defContexts, rules, seed1);
    assert(ops.size() == 1);

    selectDepartment(attacker.getRivalState(2)->departmentWeights, seedCopy);
    OperationRecord expectedOp = resolveAttempt(
        1, 2, IntelArea::Espionage, ops[0].department, 1,
        attacker.getRivalState(2)->getCombinedRollingAttackSum(),
        20.0,
        0, TreatyLevel::None,
        0, 0, seedCopy
    );

    assert(ops[0].outcome == expectedOp.outcome);
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
        0, 20.0, 2, TreatyLevel::None, 1000, 1000, seed1
    );

    // Run B: attackStrength = -50 (penalized by unallocated zero-area decay history)
    OperationRecord opNegative = resolveAttempt(
        1, 2, IntelArea::Espionage, IntelDepartment::Economic, 1,
        -50, 20.0, 2, TreatyLevel::None, 1000, 1000, seed2
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

        std::cout << "\n>>> ALL 14 INTELLIGENCE TEST SUITES PASSED CLEANLY! <<<" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL with exception: " << ex.what() << std::endl;
        return 1;
    }
}
