#include "TacticalCombat.h"
#include "Simulation.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Checks stay active in every build type (assert() vanished under NDEBUG, which
// RelWithDebInfo/Release define, so these suites previously verified nothing).
#define CHECK(cond) do { if (!(cond)) throw std::runtime_error(std::string("Check failed at line ") + \
    std::to_string(__LINE__) + ": " + #cond); } while (0)

namespace botf::tactical::tests {

// Helper to ensure empires 1 and 2 are at war for test setups
void ensureAtWar(GameState& state, int ownerA = 1, int ownerB = 2) {
    for (auto& rel : state.majorRelations) {
        if ((rel.first == ownerA && rel.second == ownerB) ||
            (rel.first == ownerB && rel.second == ownerA)) {
            rel.atWar = true;
            rel.contact = true;
            return;
        }
    }
    state.majorRelations.push_back({ownerA, ownerB, true, true, false, state.turn, 0});
}

// -----------------------------------------------------------------------------
// Test 1: Construction Validation, Exactly 2 Owners at War, Same Sector
// -----------------------------------------------------------------------------
void testEncounterConstructionValidation() {
    std::cout << "Testing: encounter construction validation (2 owners, at war, same sector)..." << std::endl;
    GameState state = generateCampaign(1337, 2, 16, 12, 28, 1);
    ensureAtWar(state, 1, 2);
    std::cout << "  Diagnostic: state generated and war ensured." << std::endl;

    CHECK(state.fleets.size() >= 2);
    Fleet f1 = state.fleets[0];
    Fleet f2 = state.fleets[1];

    // Case 1: Less than 2 participants
    try {
        constructEncounter(state, {f1.id});
        CHECK(false && "Should have thrown for < 2 participants");
    } catch (const std::invalid_argument&) {}
    std::cout << "  Diagnostic: Case 1 passed." << std::endl;

    // Case 2: Duplicate IDs
    try {
        constructEncounter(state, {f1.id, f1.id});
        CHECK(false && "Should have thrown for duplicate IDs");
    } catch (const std::invalid_argument&) {}

    // Case 3: Non-existent ID
    try {
        constructEncounter(state, {f1.id, 999999});
        CHECK(false && "Should have thrown for non-existent ID");
    } catch (const std::invalid_argument&) {}

    // Case 4: Participants in different sectors
    Fleet fDiff = f2;
    fDiff.id = 999;
    fDiff.x = f1.x + 2;
    fDiff.y = f1.y + 2;
    state.fleets.push_back(fDiff);
    try {
        constructEncounter(state, {f1.id, fDiff.id});
        CHECK(false && "Should have thrown for different sectors");
    } catch (const std::invalid_argument&) {}
    state.fleets.pop_back();

    // Case 5: Same owner participants only (1 owner)
    Fleet fFriendly = f1;
    fFriendly.id = 998;
    state.fleets.push_back(fFriendly);
    try {
        constructEncounter(state, {f1.id, fFriendly.id});
        CHECK(false && "Should have thrown for 1 owner");
    } catch (const std::invalid_argument&) {}
    state.fleets.pop_back();

    // Case 6: 3 owners in same encounter (reject 3+)
    Fleet f3;
    f3.id = 997; f3.ownerId = 3; f3.x = 5; f3.y = 5; f3.hull = 50; f3.shields = 20;
    Fleet f1_sec = f1; f1_sec.id = 995; f1_sec.ownerId = 1; f1_sec.x = 5; f1_sec.y = 5; f1_sec.hull = 50;
    Fleet f2_sec = f2; f2_sec.id = 996; f2_sec.ownerId = 2; f2_sec.x = 5; f2_sec.y = 5; f2_sec.hull = 50;
    state.fleets.push_back(f1_sec);
    state.fleets.push_back(f2_sec);
    state.fleets.push_back(f3);
    try {
        constructEncounter(state, {f1_sec.id, f2_sec.id, f3.id});
        CHECK(false && "Should have thrown for 3+ owners");
    } catch (const std::invalid_argument&) {}
    state.fleets.pop_back();
    state.fleets.pop_back();
    state.fleets.pop_back();

    // Case 7: Peaceful combat (not at war)
    GameState peacefulState = generateCampaign(1337, 2, 16, 12, 28, 1);
    for (auto& rel : peacefulState.majorRelations) {
        rel.atWar = false; // Ensure peace
    }
    Fleet pf1; pf1.id = 1101; pf1.ownerId = 1; pf1.x = 4; pf1.y = 4; pf1.hull = 50; pf1.shields = 20;
    Fleet pf2; pf2.id = 1102; pf2.ownerId = 2; pf2.x = 4; pf2.y = 4; pf2.hull = 50; pf2.shields = 20;
    peacefulState.fleets.push_back(pf1);
    peacefulState.fleets.push_back(pf2);
    try {
        constructEncounter(peacefulState, {pf1.id, pf2.id});
        CHECK(false && "Should have thrown for peaceful combat between non-war empires");
    } catch (const std::invalid_argument&) {}

    // Case 8: Valid construction and deterministic canonical deployment
    Fleet hostA = f1;
    hostA.id = 1001; hostA.ownerId = 1; hostA.x = 5; hostA.y = 5; hostA.hull = 100; hostA.shields = 50;
    Fleet hostB = f2;
    hostB.id = 1002; hostB.ownerId = 2; hostB.x = 5; hostB.y = 5; hostB.hull = 80; hostB.shields = 40;
    state.fleets.push_back(hostA);
    state.fleets.push_back(hostB);

    Encounter enc = constructEncounter(state, {hostB.id, hostA.id}, 4242); // pass in reverse order
    CHECK(enc.ships.size() == 2);
    CHECK(enc.sectorX == 5 && enc.sectorY == 5);
    CHECK(enc.currentRound == 0);
    CHECK(!enc.isFinished);
    CHECK(enc.controllingOwnerId == state.playerEmpire);
    // Deterministic canonical sorting puts owner 1 on left, owner 2 on right
    CHECK(enc.ships[0].ownerId == 1 && enc.ships[0].fleetId == 1001);
    CHECK(enc.ships[1].ownerId == 2 && enc.ships[1].fleetId == 1002);
    CHECK(enc.ships[0].position.x == 1);
    CHECK(enc.ships[1].position.x == enc.arenaWidth - 2);

    std::cout << "  PASS: construction validation verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 2: Shield Damage Absorption before Hull
// -----------------------------------------------------------------------------
void testShieldDamageAbsorption() {
    std::cout << "Testing: shield absorption before hull damage..." << std::endl;
    Encounter enc;
    enc.arenaWidth = 10;
    enc.arenaHeight = 10;
    enc.initialSeed = 100;
    enc.rngState = 100;
    enc.controllingOwnerId = 1;

    Ship attacker;
    attacker.fleetId = 1;
    attacker.ownerId = 1;
    attacker.beamAttack = 60;
    attacker.torpedoAttack = 0;
    attacker.speed = 1;
    attacker.position = {2, 5};
    attacker.hull = 100;
    attacker.maxHull = 100;
    attacker.initialHull = 100;
    attacker.shields = 100;
    attacker.maxShields = 100;
    attacker.initialShields = 100;

    Ship defender;
    defender.fleetId = 2;
    defender.ownerId = 2;
    defender.beamAttack = 10;
    defender.torpedoAttack = 0;
    defender.speed = 1;
    defender.position = {3, 5}; // Range = 1
    defender.hull = 100;
    defender.maxHull = 100;
    defender.initialHull = 100;
    defender.shields = 80; // Shields should absorb first
    defender.maxShields = 80;
    defender.initialShields = 80;

    enc.ships.push_back(attacker);
    enc.ships.push_back(defender);

    enc.submitOrder({1, 2, TacticalManeuver::Engage});

    RoundResult r = enc.resolveRound();
    const Ship* defAfter = enc.getShip(2);
    CHECK(defAfter != nullptr);
    // Round 1: Attacker deals ~40-60 damage. 80 shields absorb, hull remains 100!
    CHECK(defAfter->shields < 80);
    CHECK(defAfter->hull == 100);
    std::cout << "  Defender shields: 80 -> " << defAfter->shields << ", hull remained " << defAfter->hull << std::endl;

    // Round 2: Remaining shields should break to 0 and penetrate hull
    enc.submitOrder({1, 2, TacticalManeuver::Engage});
    r = enc.resolveRound();
    defAfter = enc.getShip(2);
    CHECK(defAfter->shields == 0);
    CHECK(defAfter->hull < 100);
    std::cout << "  Defender shields broke to 0, hull took penetration damage: " << defAfter->hull << std::endl;
    std::cout << "  PASS: shield damage absorption verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 3: Focus Fire
// -----------------------------------------------------------------------------
void testFocusFire() {
    std::cout << "Testing: focus fire on single target..." << std::endl;
    Encounter enc;
    enc.arenaWidth = 12;
    enc.arenaHeight = 12;
    enc.rngState = 999;
    enc.controllingOwnerId = 1;

    Ship a1{1, 1, "Attacker 1", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 40, 20, 2, {3, 4}, true, true, false, 0};
    Ship a2{2, 1, "Attacker 2", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 40, 20, 2, {3, 6}, true, true, false, 0};
    Ship target{3, 2, "Target", Building::Destroyer, -1, 50, 50, 50, 30, 30, 30, 0, 0, 1, {4, 5}, true, true, false, 0};
    // Pinned target: under remaster rules an unarmed ship that can escape would
    // disengage (-25% incoming) instead of holding still for the focus-fire check.
    target.canRetreat = false;

    enc.ships.push_back(a1);
    enc.ships.push_back(a2);
    enc.ships.push_back(target);

    // Both a1 and a2 focus fire on target 3
    enc.submitOrder({1, 3, TacticalManeuver::Engage});
    enc.submitOrder({2, 3, TacticalManeuver::Engage});

    RoundResult res = enc.resolveRound();
    const Ship* targetAfter = enc.getShip(3);
    CHECK(targetAfter != nullptr);
    CHECK(!targetAfter->isAlive || targetAfter->hull == 0);
    CHECK(!targetAfter->activeInCombat);
    CHECK(std::find(res.lostFleetIds.begin(), res.lostFleetIds.end(), 3) != res.lostFleetIds.end());
    CHECK(enc.isFinished);
    CHECK(enc.winnerOwnerId == 1);
    std::cout << "  PASS: focus fire destroyed target in round 1." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 4: All Tactical Maneuvers (Engage, Charge, Circle, Hold, Retreat)
// -----------------------------------------------------------------------------
void testAllManeuvers() {
    std::cout << "Testing: all tactical maneuvers (Engage, Charge, Circle, Hold, Retreat)..." << std::endl;

    // 1. Charge maneuver test: closes distance rapidly
    {
        Encounter enc;
        enc.arenaWidth = 16;
        enc.arenaHeight = 16;
        enc.controllingOwnerId = 1;
        Ship s1{1, 1, "Charger", Building::Destroyer, -1, 100, 100, 100, 50, 50, 50, 20, 0, 3, {2, 8}, true, true, false, 0};
        Ship s2{2, 2, "Target", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 0, 0, 1, {10, 8}, true, true, false, 0};
        enc.ships.push_back(s1);
        enc.ships.push_back(s2);

        enc.submitOrder({1, 2, TacticalManeuver::Charge});
        enc.resolveRound();

        const Ship* s1After = enc.getShip(1);
        CHECK(s1After->position.x == 5);
        std::cout << "  Maneuver Charge: moved from x=2 to x=" << s1After->position.x << std::endl;
    }

    // 2. Hold maneuver test. Legacy rules: stationary, recharges shields. Remaster
    // rules: stationary brace (-20% incoming) with no recharge, so battle records
    // only ever show damage.
    {
        Encounter enc;
        enc.rulesVersion = LegacyTacticalRules;
        enc.arenaWidth = 10;
        enc.arenaHeight = 10;
        enc.controllingOwnerId = 1;
        Ship s1{1, 1, "Defender", Building::Scout, -1, 100, 100, 100, 20, 100, 20, 0, 0, 2, {5, 5}, true, true, false, 0};
        Ship s2{2, 2, "Dummy", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 0, 0, 1, {1, 1}, true, true, false, 0};
        enc.ships.push_back(s1);
        enc.ships.push_back(s2);

        enc.submitOrder({1, 0, TacticalManeuver::Hold});
        enc.resolveRound();

        const Ship* s1After = enc.getShip(1);
        CHECK(s1After->position.x == 5 && s1After->position.y == 5);
        CHECK(s1After->shields > 20);
        std::cout << "  Maneuver Hold (legacy): stationary at (5,5), shields recharged from 20 to " << s1After->shields << std::endl;
    }
    {
        auto braceDamage = [](TacticalManeuver maneuver) {
            Encounter enc;
            enc.rngState = 4321;
            enc.controllingOwnerId = 2;
            enc.ships.push_back({1, 1, "Attacker", Building::Destroyer, -1, 100, 100, 100, 0, 0, 0, 40, 0, 1, {3, 5}, true, true, false, 0});
            enc.ships.push_back({2, 2, "Holder", Building::Scout, -1, 100, 100, 100, 20, 100, 20, 0, 0, 1, {4, 5}, true, true, false, 0});
            enc.ships[1].canRetreat = false;
            enc.submitOrder({2, 1, maneuver});
            enc.resolveRound();
            const Ship* holder = enc.getShip(2);
            CHECK(holder->position.x == 4 || maneuver != TacticalManeuver::Hold);
            CHECK(holder->shields <= 20); // No recharge under remaster rules.
            return (100 - holder->hull) + (20 - holder->shields);
        };
        const int held = braceDamage(TacticalManeuver::Hold), engaged = braceDamage(TacticalManeuver::Engage);
        CHECK(held < engaged);
        std::cout << "  Maneuver Hold (remaster): braced for " << held << " vs " << engaged << " under Engage." << std::endl;
    }

    // 3. Circle/Evade maneuver test: defensive evasion reduces incoming damage
    {
        Encounter encEngage;
        encEngage.rngState = 1234;
        encEngage.controllingOwnerId = 2; // Controlling owner is 2 to issue Circle order
        Ship attEng{1, 1, "Attacker", Building::Destroyer, -1, 100, 100, 100, 0, 0, 0, 40, 0, 1, {3, 5}, true, true, false, 0};
        Ship defEng{2, 2, "Defender", Building::Scout, -1, 100, 100, 100, 0, 0, 0, 0, 0, 1, {4, 5}, true, true, false, 0};
        encEngage.ships.push_back(attEng);
        encEngage.ships.push_back(defEng);
        encEngage.submitOrder({2, 1, TacticalManeuver::Engage});
        encEngage.resolveRound();
        const int engageDamage = 100 - encEngage.getShip(2)->hull;

        Encounter encCircle;
        encCircle.rngState = 1234;
        encCircle.controllingOwnerId = 2;
        encCircle.ships.push_back(attEng);
        encCircle.ships.push_back(defEng);
        encCircle.submitOrder({2, 1, TacticalManeuver::Circle});
        encCircle.resolveRound();
        const int circleDamage = 100 - encCircle.getShip(2)->hull;

        CHECK(circleDamage < engageDamage);
        std::cout << "  Maneuver Circle: took " << circleDamage << " damage vs " << engageDamage << " under Engage." << std::endl;
    }

    // 4. Retreat maneuver: ship survives, marked retreated, removed from combat grid
    {
        Encounter enc;
        enc.arenaWidth = 10;
        enc.arenaHeight = 10;
        enc.controllingOwnerId = 1;
        Ship s1{1, 1, "Retreater", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 0, 0, 2, {2, 5}, true, true, false, 0};
        Ship s2{2, 2, "Hostile", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 0, 0, 1, {8, 5}, true, true, false, 0};
        enc.ships.push_back(s1);
        enc.ships.push_back(s2);

        enc.submitOrder({1, 0, TacticalManeuver::Retreat});
        RoundResult r = enc.resolveRound();
        const Ship* s1After = enc.getShip(1);
        // Remaster rules: the first round only disengages; the ship stays targetable.
        CHECK(!s1After->retreated && s1After->activeInCombat && s1After->disengaging());
        CHECK(r.retreatedFleetIds.empty());
        r = enc.resolveRound(); // Standing Retreat order: escapes at the next movement phase.
        s1After = enc.getShip(1);

        // Verification of Sol Review Blocker 1:
        // Retreated ship MUST survive: isAlive is TRUE, hull > 0, retreated is TRUE, activeInCombat is FALSE!
        CHECK(s1After->retreated);
        CHECK(s1After->isAlive);
        CHECK(!s1After->activeInCombat);
        CHECK(s1After->hull > 0);
        CHECK(std::find(r.retreatedFleetIds.begin(), r.retreatedFleetIds.end(), 1) != r.retreatedFleetIds.end());
        CHECK(std::find(r.lostFleetIds.begin(), r.lostFleetIds.end(), 1) == r.lostFleetIds.end());
        std::cout << "  Maneuver Retreat: ship disengaged, survives with hull=" << s1After->hull << ", activeInCombat=false." << std::endl;
    }
    std::cout << "  PASS: all maneuvers verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 5: Authorization and Atomic Failure of Orders
// -----------------------------------------------------------------------------
void testInvalidOrderAuthorizationAndAtomicFailure() {
    std::cout << "Testing: controlling owner authorization and atomic failure..." << std::endl;
    Encounter enc;
    enc.controllingOwnerId = 1; // Empire 1 controls
    Ship s1{1, 1, "Player Ship 1", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 10, 10, 1, {1, 5}, true, true, false, 0};
    Ship s2{2, 1, "Player Ship 2", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 10, 10, 1, {1, 6}, true, true, false, 0};
    Ship s3{3, 2, "Enemy Ship", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 10, 10, 1, {14, 5}, true, true, false, 0};
    enc.ships.push_back(s1);
    enc.ships.push_back(s2);
    enc.ships.push_back(s3);

    // Initial valid player order
    enc.submitOrder({1, 3, TacticalManeuver::Engage});
    CHECK(enc.pendingOrders.size() == 1);

    // Blocker 3: Player submitting order on ENEMY fleet must be rejected
    try {
        enc.submitOrder({3, 1, TacticalManeuver::Engage}); // Fleet 3 belongs to owner 2!
        CHECK(false && "Should have thrown when player attempts to command enemy fleet");
    } catch (const std::invalid_argument&) {}

    // Blocker 3: Targeting friendly ship must be rejected
    try {
        enc.submitOrder({1, 2, TacticalManeuver::Engage}); // Target 2 is friendly
        CHECK(false && "Should have thrown for targeting friendly ship");
    } catch (const std::invalid_argument&) {}
    CHECK(enc.getOrder(1).targetFleetId == 3); // Prior order unchanged

    // Ordering non-existent ship
    try {
        enc.submitOrder({999, 3, TacticalManeuver::Engage});
        CHECK(false && "Should have thrown for non-existent ship");
    } catch (const std::invalid_argument&) {}

    // Atomic batch submission failure: if one fails, none apply
    std::vector<Order> batch = {
        {1, 3, TacticalManeuver::Charge},   // Valid
        {3, 1, TacticalManeuver::Engage}    // Invalid: enemy fleet
    };
    try {
        enc.submitOrders(batch);
        CHECK(false && "Should have thrown for batch containing enemy order");
    } catch (const std::invalid_argument&) {}
    CHECK(enc.getOrder(1).maneuver == TacticalManeuver::Engage); // Remains Engage, not Charge

    // Blocker 3: Reject orders after encounter is finished
    enc.isFinished = true;
    try {
        enc.submitOrder({1, 3, TacticalManeuver::Engage});
        CHECK(false && "Should have thrown for order on finished encounter");
    } catch (const std::invalid_argument&) {}

    std::cout << "  PASS: authorization and atomic order failure verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 6: Deterministic Replay
// -----------------------------------------------------------------------------
void testDeterministicReplay() {
    std::cout << "Testing: deterministic replay matches bit-for-bit..." << std::endl;
    auto createEnc = [](std::uint64_t seed) {
        Encounter enc;
        enc.initialSeed = seed;
        enc.rngState = seed;
        enc.controllingOwnerId = 1;
        enc.ships.push_back({1, 1, "Alpha", Building::Destroyer, -1, 120, 120, 120, 60, 60, 60, 30, 20, 2, {2, 5}, true, true, false, 0});
        enc.ships.push_back({2, 2, "Beta", Building::Destroyer, -1, 140, 140, 140, 70, 70, 70, 25, 25, 1, {13, 5}, true, true, false, 0});
        return enc;
    };

    Encounter enc1 = createEnc(777);
    Encounter enc2 = createEnc(777);

    for (int r = 1; r <= 5; ++r) {
        enc1.submitOrder({1, 2, TacticalManeuver::Engage});
        RoundResult r1 = enc1.resolveRound();

        enc2.submitOrder({1, 2, TacticalManeuver::Engage});
        RoundResult r2 = enc2.resolveRound();

        CHECK(r1.roundIndex == r2.roundIndex);
        CHECK(r1.damage.size() == r2.damage.size());
        for (std::size_t i = 0; i < r1.damage.size(); ++i) {
            CHECK(r1.damage[i].fleetId == r2.damage[i].fleetId);
            CHECK(r1.damage[i].hullAfter == r2.damage[i].hullAfter);
            CHECK(r1.damage[i].shieldAfter == r2.damage[i].shieldAfter);
        }
        CHECK(enc1.rngState == enc2.rngState);
        CHECK(enc1.getShip(1)->hull == enc2.getShip(1)->hull);
        CHECK(enc1.getShip(2)->hull == enc2.getShip(2)->hull);
    }
    std::cout << "  PASS: 5 rounds produced bit-for-bit identical results." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 7: Strict Deserialization Validation and Mid-battle Roundtrip
// -----------------------------------------------------------------------------
void testStrictDeserialization() {
    std::cout << "Testing: strict deserialization validation and full shipStatuses roundtrip..." << std::endl;

    // 1. Invalid header
    try {
        deserializeEncounter("BAD_HEADER\nARENA 16 12\n");
        CHECK(false && "Should reject bad header");
    } catch (const std::invalid_argument&) {}

    // 2. Truncated stream
    try {
        deserializeEncounter("TACTICAL_ENCOUNTER_V1\nARENA 16\n");
        CHECK(false && "Should reject truncated ARENA");
    } catch (const std::invalid_argument&) {}

    // 3. Negative / giant count without allocations
    try {
        deserializeEncounter("TACTICAL_ENCOUNTER_V1\nARENA 16 12\nROUNDS 0 50 0 0\nSECTOR 1 1 1\nRNG 1 1\nCONTROLLER 1 0\nSHIPS 999999999\n");
        CHECK(false && "Should reject giant ship count");
    } catch (const std::invalid_argument&) {}

    // 4. Permissive boolean rejection (e.g. non 0/1 boolean value)
    try {
        deserializeEncounter("TACTICAL_ENCOUNTER_V1\nARENA 16 12\nROUNDS 0 50 2 0\nSECTOR 1 1 1\nRNG 1 1\nCONTROLLER 1 0\nSHIPS 2\n"
                             "1 1 100 100 100 50 50 50 10 10 1 1 1 1 1 0 0 1 0 -1 Ship1\n"
                             "2 2 100 100 100 50 50 50 10 10 1 5 5 1 1 0 0 1 0 -1 Ship2\n"
                             "ORDERS 0\nROUND_RESULTS 0\nEND\n");
        CHECK(false && "Should reject non-boolean value 2 for isFinished");
    } catch (const std::invalid_argument&) {}

    // 5. Trailing data after END
    Encounter validEnc;
    validEnc.controllingOwnerId = 1;
    validEnc.ships.push_back({1, 1, "Ship1", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 10, 10, 1, {1, 1}, true, true, false, 0, true});
    validEnc.ships.push_back({2, 2, "Ship2", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 10, 10, 1, {5, 5}, true, true, false, 0, true});
    std::string text = serializeEncounter(validEnc);
    std::string textWithTrailing = text + "EXTRA_CORRUPT_BYTES\n";
    try {
        deserializeEncounter(textWithTrailing);
        CHECK(false && "Should reject trailing data after END");
    } catch (const std::invalid_argument&) {}

    // 6. Multi-round simulation with non-default Charge/Circle/target orders and exact shipStatuses roundtrip
    Encounter simEnc;
    simEnc.arenaWidth = 16;
    simEnc.arenaHeight = 12;
    simEnc.controllingOwnerId = 1;
    simEnc.rngState = 88888;
    simEnc.ships.push_back({101, 1, "Federation Defiant", Building::Destroyer, -1, 200, 200, 200, 100, 100, 100, 30, 20, 2, {2, 5}, true, true, false, 0, true});
    simEnc.ships.push_back({202, 2, "Cardassian Galor", Building::Destroyer, -1, 250, 250, 250, 120, 120, 120, 25, 25, 1, {12, 5}, true, true, false, 0, true});

    // Round 1: Non-default Charge order
    simEnc.submitOrder({101, 202, TacticalManeuver::Charge});
    RoundResult r1 = simEnc.resolveRound();
    CHECK(r1.shipStatuses[0].maneuverExecuted == TacticalManeuver::Charge);
    CHECK(r1.shipStatuses[0].targetFleetId == 202);

    // Round 2: Non-default Circle order
    simEnc.submitOrder({101, 202, TacticalManeuver::Circle});
    RoundResult r2 = simEnc.resolveRound();
    CHECK(r2.shipStatuses[0].maneuverExecuted == TacticalManeuver::Circle);
    CHECK(r2.shipStatuses[0].targetFleetId == 202);

    // Exact mid-battle serialization
    std::string midTurnText = serializeEncounter(simEnc);
    Encounter loaded = deserializeEncounter(midTurnText);

    // Verify all public encounter state matches
    CHECK(loaded.currentRound == simEnc.currentRound);
    CHECK(loaded.rngState == simEnc.rngState);
    CHECK(loaded.ships.size() == simEnc.ships.size());
    CHECK(loaded.rounds.size() == simEnc.rounds.size());

    // Verify full shipStatuses preserved across all rounds
    for (std::size_t roundIdx = 0; roundIdx < simEnc.rounds.size(); ++roundIdx) {
        const auto& origRound = simEnc.rounds[roundIdx];
        const auto& loadRound = loaded.rounds[roundIdx];
        CHECK(origRound.roundIndex == loadRound.roundIndex);
        CHECK(origRound.encounterEnded == loadRound.encounterEnded);
        CHECK(origRound.shipStatuses.size() == loadRound.shipStatuses.size());
        for (std::size_t sIdx = 0; sIdx < origRound.shipStatuses.size(); ++sIdx) {
            const auto& os = origRound.shipStatuses[sIdx];
            const auto& ls = loadRound.shipStatuses[sIdx];
            CHECK(os.fleetId == ls.fleetId);
            CHECK(os.ownerId == ls.ownerId);
            CHECK(os.hullBefore == ls.hullBefore);
            CHECK(os.hullAfter == ls.hullAfter);
            CHECK(os.shieldBefore == ls.shieldBefore);
            CHECK(os.shieldAfter == ls.shieldAfter);
            CHECK(os.beamDamageDealt == ls.beamDamageDealt);
            CHECK(os.torpedoDamageDealt == ls.torpedoDamageDealt);
            CHECK(os.totalDamageTaken == ls.totalDamageTaken);
            CHECK(os.positionBefore == ls.positionBefore);
            CHECK(os.positionAfter == ls.positionAfter);
            CHECK(os.maneuverExecuted == ls.maneuverExecuted);
            CHECK(os.targetFleetId == ls.targetFleetId);
            CHECK(os.destroyedThisRound == ls.destroyedThisRound);
            CHECK(os.retreatedThisRound == ls.retreatedThisRound);
        }
    }

    // Resolve Round 3 on both: must be byte-for-byte identical
    simEnc.submitOrder({101, 202, TacticalManeuver::Hold});
    RoundResult nextOrig = simEnc.resolveRound();

    loaded.submitOrder({101, 202, TacticalManeuver::Hold});
    RoundResult nextLoaded = loaded.resolveRound();

    CHECK(nextOrig.roundIndex == nextLoaded.roundIndex);
    CHECK(nextOrig.shipStatuses.size() == nextLoaded.shipStatuses.size());
    for (std::size_t i = 0; i < nextOrig.shipStatuses.size(); ++i) {
        CHECK(nextOrig.shipStatuses[i] == nextLoaded.shipStatuses[i]);
    }
    CHECK(simEnc.rngState == loaded.rngState);

    // Re-serializing loaded must produce identical text
    CHECK(serializeEncounter(loaded) == serializeEncounter(simEnc));
    std::cout << "  PASS: full multi-round shipStatuses and deterministic continuation verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 8: Campaign Result Mapping, Atomic Validation, and Retreat Relocation
// -----------------------------------------------------------------------------
void testCampaignResultMappingAndRelocation() {
    std::cout << "Testing: campaign result mapping, atomic validation, and retreat relocation..." << std::endl;
    GameState state = generateCampaign(5555, 2, 16, 12, 28, 1);
    ensureAtWar(state, 1, 2);

    const int initialTurn = state.turn;
    const std::size_t initialColonyCount = state.colonies.size();

    // Create 2 test fleets at sector (3, 3)
    Fleet f1;
    f1.id = 501; f1.ownerId = 1; f1.x = 3; f1.y = 3; f1.hull = 20; f1.shields = 0; f1.shipClass = Building::Destroyer;
    Fleet f2;
    f2.id = 502; f2.ownerId = 2; f2.x = 3; f2.y = 3; f2.hull = 10; f2.shields = 0; f2.shipClass = Building::Scout;
    state.fleets.push_back(f1);
    state.fleets.push_back(f2);

    Encounter enc = constructEncounter(state, {501, 502}, 9876);
    // Ship 1 orders retreat
    enc.submitOrder({501, 0, TacticalManeuver::Retreat});
    enc.runUntilFinished();
    CHECK(enc.isFinished);
    CHECK(enc.getShip(501)->retreated);
    CHECK(enc.getShip(501)->isAlive);
    CHECK(enc.getShip(501)->hull > 0);

    // Blocker 4: Check CombatReport uses initialHull / initialShields
    CombatReport rep = toCombatReport(enc);
    CHECK(rep.participants[0].initialHull == 20);
    CHECK(rep.participants[0].initialShields == 0);
    // Retreated ship is in retreatedFleetIds, NOT lostFleetIds
    CHECK(std::find(rep.retreatedFleetIds.begin(), rep.retreatedFleetIds.end(), 501) != rep.retreatedFleetIds.end());
    CHECK(std::find(rep.lostFleetIds.begin(), rep.lostFleetIds.end(), 501) == rep.lostFleetIds.end());

    // Blocker 4: Stale turn rejection test
    GameState staleState = state;
    staleState.turn += 1;
    try {
        applyEncounterResults(staleState, enc);
        CHECK(false && "Should reject application on stale turn");
    } catch (const std::invalid_argument&) {}

    // Blocker 4: Stale location rejection test
    GameState movedState = state;
    for (auto& f : movedState.fleets) if (f.id == 501) f.x += 1;
    try {
        applyEncounterResults(movedState, enc);
        CHECK(false && "Should reject application if fleet moved");
    } catch (const std::invalid_argument&) {}

    // Blocker 4: Stale HP rejection test
    GameState hpModifiedState = state;
    for (auto& f : hpModifiedState.fleets) if (f.id == 501) f.hull += 10;
    try {
        applyEncounterResults(hpModifiedState, enc);
        CHECK(false && "Should reject application if fleet HP changed concurrently");
    } catch (const std::invalid_argument&) {}

    // Clean application test
    applyEncounterResults(state, enc);

    // Verify Blocker 1: Retreated ship survived and was relocated!
    auto it501 = std::find_if(state.fleets.begin(), state.fleets.end(), [](const Fleet& f) { return f.id == 501; });
    CHECK(it501 != state.fleets.end() && "Retreated ship must NOT be destroyed in GameState!");
    CHECK(it501->hull > 0);
    CHECK(it501->retreat == true);
    // Relocation moved it to an adjacent sector or withdrawal target
    CHECK(it501->x != 3 || it501->y != 3);
    std::cout << "  Retreated fleet 501 relocated from (3,3) to (" << it501->x << "," << it501->y << ") with hull=" << it501->hull << std::endl;

    // Verify Blocker 4: Double application rejected
    try {
        applyEncounterResults(state, enc);
        CHECK(false && "Should reject applying results twice");
    } catch (const std::invalid_argument&) {}

    // Verify invariants: strategic turn unmodified
    CHECK(state.turn == initialTurn);
    CHECK(state.colonies.size() == initialColonyCount);

    // Verify retreat destination was revealed in explored knowledge
    const int destCell = it501->y * state.width + it501->x;
    const auto& explored = empire(state, 1).explored;
    CHECK(std::binary_search(explored.begin(), explored.end(), destCell));
    std::cout << "  Retreat destination cell " << destCell << " verified in empire explored knowledge." << std::endl;

    // Test rejection of retreat when trapped (no safe supplied adjacent sector exists)
    GameState trappedState = generateCampaign(9999, 2, 16, 12, 28, 1);
    ensureAtWar(trappedState, 1, 2);
    Fleet trappedF1;
    trappedF1.id = 601; trappedF1.ownerId = 1; trappedF1.x = 0; trappedF1.y = 0; trappedF1.hull = 20; trappedF1.shipClass = Building::Scout;
    Fleet enemyF0;
    enemyF0.id = 602; enemyF0.ownerId = 2; enemyF0.x = 0; enemyF0.y = 0; enemyF0.hull = 20; enemyF0.shipClass = Building::Scout;
    Fleet block1;
    block1.id = 603; block1.ownerId = 2; block1.x = 1; block1.y = 0; block1.hull = 20; block1.shipClass = Building::Destroyer;
    Fleet block2;
    block2.id = 604; block2.ownerId = 2; block2.x = 0; block2.y = 1; block2.hull = 20; block2.shipClass = Building::Destroyer;
    Fleet block3;
    block3.id = 605; block3.ownerId = 2; block3.x = 1; block3.y = 1; block3.hull = 20; block3.shipClass = Building::Destroyer;
    trappedState.fleets.push_back(trappedF1);
    trappedState.fleets.push_back(enemyF0);
    trappedState.fleets.push_back(block1);
    trappedState.fleets.push_back(block2);
    trappedState.fleets.push_back(block3);

    Encounter trappedEnc = constructEncounter(trappedState, {601, 602}, 1234);
    CHECK(!trappedEnc.getShip(601)->canRetreat);
    try {
        trappedEnc.submitOrder({601, 0, TacticalManeuver::Retreat});
        CHECK(false && "Should have rejected retreat order for trapped fleet with no safe escape");
    } catch (const std::invalid_argument&) {}
    std::cout << "  Trapped fleet retreat order properly rejected." << std::endl;

    std::cout << "  PASS: retreat survival, strategic relocation, exploration reveal, and trapped checks verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 9: Added Original Maneuvers (Assault, Harry, Flyby, Strafe, Evade, Ram, Hail)
// -----------------------------------------------------------------------------
void testExtendedManeuvers() {
    std::cout << "Testing: extended tactical maneuvers (Assault, Harry, Flyby, Strafe, Evade, Ram, Hail)..." << std::endl;

    // 1. Evade: defensive posture reduces incoming damage more than Circle (-35% vs -25%)
    {
        Encounter encEngage;
        encEngage.rngState = 42;
        encEngage.controllingOwnerId = 2;
        Ship attEng{1, 1, "Attacker", Building::Destroyer, -1, 100, 100, 100, 0, 0, 0, 50, 0, 1, {3, 5}, true, true, false, 0};
        Ship defEng{2, 2, "Defender", Building::Scout, -1, 100, 100, 100, 0, 0, 0, 0, 0, 1, {4, 5}, true, true, false, 0};
        encEngage.ships.push_back(attEng);
        encEngage.ships.push_back(defEng);
        encEngage.submitOrder({2, 1, TacticalManeuver::Engage});
        encEngage.resolveRound();
        const int engageDamage = 100 - encEngage.getShip(2)->hull;

        Encounter encEvade;
        encEvade.rngState = 42;
        encEvade.controllingOwnerId = 2;
        encEvade.ships.push_back(attEng);
        encEvade.ships.push_back(defEng);
        encEvade.submitOrder({2, 1, TacticalManeuver::Evade});
        encEvade.resolveRound();
        const int evadeDamage = 100 - encEvade.getShip(2)->hull;

        CHECK(evadeDamage < engageDamage);
        std::cout << "  Maneuver Evade: took " << evadeDamage << " damage vs " << engageDamage << " under Engage." << std::endl;
    }

    // 2. Strafe: close burst (+25% beam damage)
    {
        Encounter encEngage;
        encEngage.rngState = 100;
        encEngage.controllingOwnerId = 1;
        Ship att1{1, 1, "Attacker", Building::Destroyer, -1, 100, 100, 100, 0, 0, 0, 40, 0, 2, {3, 5}, true, true, false, 0};
        Ship def1{2, 2, "Defender", Building::Destroyer, -1, 200, 200, 200, 0, 0, 0, 0, 0, 1, {4, 5}, true, true, false, 0};
        encEngage.ships.push_back(att1);
        encEngage.ships.push_back(def1);
        encEngage.submitOrder({1, 2, TacticalManeuver::Engage});
        encEngage.resolveRound();
        const int engageDealt = 200 - encEngage.getShip(2)->hull;

        Encounter encStrafe;
        encStrafe.rngState = 100;
        encStrafe.controllingOwnerId = 1;
        encStrafe.ships.push_back(att1);
        encStrafe.ships.push_back(def1);
        encStrafe.submitOrder({1, 2, TacticalManeuver::Strafe});
        encStrafe.resolveRound();
        const int strafeDealt = 200 - encStrafe.getShip(2)->hull;

        CHECK(strafeDealt > engageDealt);
        std::cout << "  Maneuver Strafe: dealt " << strafeDealt << " damage vs " << engageDealt << " under Engage." << std::endl;
    }

    // 3. Ram: kinetic collision inflicts massive impact damage based on hull
    {
        Encounter encRam;
        encRam.rngState = 500;
        encRam.controllingOwnerId = 1;
        Ship rammer{1, 1, "Rammer", Building::Destroyer, -1, 150, 150, 150, 0, 0, 0, 10, 0, 2, {4, 5}, true, true, false, 0};
        Ship target{2, 2, "Victim", Building::Destroyer, -1, 300, 300, 300, 0, 0, 0, 0, 0, 1, {5, 5}, true, true, false, 0};
        encRam.ships.push_back(rammer);
        encRam.ships.push_back(target);
        encRam.submitOrder({1, 2, TacticalManeuver::Ram});
        encRam.resolveRound();

        const Ship* victimAfter = encRam.getShip(2);
        const int damageTaken = 300 - victimAfter->hull;
        CHECK(damageTaken >= 150); // Impact inflicted rammer's hull value
        std::cout << "  Maneuver Ram: rammer inflicted " << damageTaken << " kinetic impact damage on victim." << std::endl;
    }

    // 4. Hail: mutual hail results in peaceful truce without casualties
    {
        Encounter encHail;
        encHail.controllingOwnerId = 1;
        Ship s1{1, 1, "Federation Envoy", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 20, 20, 1, {3, 5}, true, true, false, 0};
        Ship s2{2, 2, "Klingon Envoy", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 20, 20, 1, {6, 5}, true, true, false, 0};
        encHail.ships.push_back(s1);
        encHail.ships.push_back(s2);

        // Player orders Hail; set standing order on s2 as Hail also
        encHail.submitOrder({1, 0, TacticalManeuver::Hail});
        encHail.standingOrders[2] = Order{2, 0, TacticalManeuver::Hail};

        RoundResult r = encHail.resolveRound();
        CHECK(r.hailedTruce);
        CHECK(encHail.isFinished);
        CHECK(encHail.winnerOwnerId == 0);
        CHECK(encHail.getShip(1)->hull == 100);
        CHECK(encHail.getShip(2)->hull == 100);
        std::cout << "  Maneuver Hail: mutual hail concluded encounter cleanly in a peaceful truce." << std::endl;
    }

    std::cout << "  PASS: extended tactical maneuvers verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 10: Like-Class Tactical Groups & Group Orders
// -----------------------------------------------------------------------------
void testTacticalGroups() {
    std::cout << "Testing: like-class tactical groups and group orders..." << std::endl;
    Encounter enc;
    enc.controllingOwnerId = 1;

    TacticalGroup gFast;
    gFast.groupId = 1;
    gFast.ownerId = 1;
    gFast.name = "Wolfpack";
    gFast.groupClass = TacticalGroupClass::FastAttack;
    gFast.fleetIds = {101, 102};
    gFast.standingManeuver = TacticalManeuver::Engage;

    TacticalGroup gScout;
    gScout.groupId = 2;
    gScout.ownerId = 1;
    gScout.name = "Recon Wing";
    gScout.groupClass = TacticalGroupClass::Scout;
    gScout.fleetIds = {103};
    gScout.standingManeuver = TacticalManeuver::Evade;

    enc.groups.push_back(gFast);
    enc.groups.push_back(gScout);

    Ship s1{101, 1, "Defiant A", Building::Destroyer, -1, 100, 100, 100, 50, 50, 50, 30, 20, 2, {2, 4}, true, true, false, 0};
    s1.groupId = 1;
    Ship s2{102, 1, "Defiant B", Building::Destroyer, -1, 100, 100, 100, 50, 50, 50, 30, 20, 2, {2, 6}, true, true, false, 0};
    s2.groupId = 1;
    Ship s3{103, 1, "Pathfinder", Building::Scout, -1, 80, 80, 80, 40, 40, 40, 10, 10, 3, {1, 5}, true, true, false, 0};
    s3.groupId = 2;
    Ship e1{201, 2, "Enemy Warbird", Building::Destroyer, -1, 300, 300, 300, 100, 100, 100, 40, 30, 1, {10, 5}, true, true, false, 0};

    enc.ships.push_back(s1);
    enc.ships.push_back(s2);
    enc.ships.push_back(s3);
    enc.ships.push_back(e1);

    // Verify lookup
    CHECK(enc.getGroup(1) != nullptr);
    CHECK(enc.getGroupForShip(101)->groupId == 1);
    CHECK(enc.getGroupForShip(103)->groupId == 2);

    // Issue group order to group 1: Strafe target 201
    enc.submitGroupOrder(1, TacticalManeuver::Strafe, 201);
    CHECK(enc.getOrder(101).maneuver == TacticalManeuver::Strafe);
    CHECK(enc.getOrder(101).targetFleetId == 201);
    CHECK(enc.getOrder(102).maneuver == TacticalManeuver::Strafe);
    CHECK(enc.getOrder(102).targetFleetId == 201);

    // Scout group remains unaffected
    CHECK(enc.getOrder(103).maneuver != TacticalManeuver::Strafe);

    std::cout << "  PASS: tactical groups formation and collective orders verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 11: Cloaking Device Lifecycle and Ambush Strike
// -----------------------------------------------------------------------------
void testCloakLifecycle() {
    std::cout << "Testing: cloaking device lifecycle, untargetability, and decloak ambush bonus..." << std::endl;

    // 1. Untargetability: enemy auto-targeting cannot pick a cloaked ship when another target exists
    {
        Encounter enc;
        enc.controllingOwnerId = 2; // Klingons controlling
        Ship fedScout{1, 1, "Fed Scout", Building::Scout, -1, 100, 100, 100, 50, 50, 50, 10, 10, 2, {2, 5}, true, true, false, 0};
        Ship kBoP{2, 2, "B'rel", Building::Destroyer, -1, 80, 80, 80, 40, 40, 40, 30, 20, 2, {6, 5}, true, true, false, 0};
        kBoP.canCloak = true;
        kBoP.isCloaked = true;

        Ship kCruiser{3, 2, "Vor'cha", Building::Destroyer, -1, 200, 200, 200, 100, 100, 100, 40, 30, 1, {12, 5}, true, true, false, 0};
        Ship fedAttacker{4, 1, "Fed Attacker", Building::Destroyer, -1, 100, 100, 100, 50, 50, 50, 30, 20, 2, {5, 5}, true, true, false, 0};

        enc.ships.push_back(fedScout);
        enc.ships.push_back(kBoP);
        enc.ships.push_back(kCruiser);
        enc.ships.push_back(fedAttacker);

        // Fed attacker has no explicit target, should auto-target living uncloaked enemy (Vor'cha, not B'rel)
        enc.resolveRound();
        const auto& round = enc.rounds.back();
        for (const auto& salvo : round.salvos) {
            if (salvo.attackerFleetId == 4) {
                CHECK(salvo.defenderFleetId == 3); // Must target uncloaked Vor'cha, skipping cloaked B'rel!
            }
        }
        std::cout << "  Cloak: auto-targeting successfully ignored cloaked vessel." << std::endl;
    }

    // 2. Decloak Ambush Bonus: when a cloaked ship issues an offensive maneuver, it decloaks and gets +25% damage
    {
        Encounter encNormal;
        encNormal.rngState = 777;
        encNormal.controllingOwnerId = 2;
        Ship attNormal{10, 2, "Bird of Prey", Building::Destroyer, -1, 100, 100, 100, 50, 50, 50, 40, 20, 2, {4, 5}, true, true, false, 0};
        Ship defNormal{20, 1, "Galaxy", Building::Destroyer, -1, 300, 300, 300, 0, 0, 0, 0, 0, 1, {5, 5}, true, true, false, 0};
        encNormal.ships.push_back(attNormal);
        encNormal.ships.push_back(defNormal);
        encNormal.submitOrder({10, 20, TacticalManeuver::Engage});
        encNormal.resolveRound();
        const int normalDamage = 300 - encNormal.getShip(20)->hull;

        Encounter encCloaked;
        encCloaked.rngState = 777;
        encCloaked.controllingOwnerId = 2;
        Ship attCloaked = attNormal;
        attCloaked.canCloak = true;
        attCloaked.isCloaked = true;
        Ship defCloaked = defNormal;
        encCloaked.ships.push_back(attCloaked);
        encCloaked.ships.push_back(defCloaked);
        encCloaked.submitOrder({10, 20, TacticalManeuver::Engage});
        encCloaked.resolveRound();

        CHECK(!encCloaked.getShip(10)->isCloaked); // Ship decloaked!
        CHECK(encCloaked.rounds.back().shipStatuses[0].decloakedThisRound);
        const int ambushDamage = 300 - encCloaked.getShip(20)->hull;

        CHECK(ambushDamage > normalDamage);
        std::cout << "  Cloak: decloaked on attack with ambush bonus: " << ambushDamage << " damage vs " << normalDamage << "." << std::endl;
    }

    std::cout << "  PASS: cloaking device lifecycle verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 12: Representative Federation vs Klingon Battle with Mid-Battle Save/Replay
// -----------------------------------------------------------------------------
void testRepresentativeFedVsKlingonBattle() {
    std::cout << "Testing: representative Fed vs Klingon multi-ship battle with mid-battle save/reload determinism..." << std::endl;

    Encounter enc;
    enc.arenaWidth = 16;
    enc.arenaHeight = 12;
    enc.controllingOwnerId = 1; // Federation player
    enc.initialSeed = 19990528;
    enc.rngState = 19990528;

    // Federation fleet (Owner 1): 1 Galaxy, 2 Defiants
    Ship fed1{1, 1, "USS Enterprise", Building::Destroyer, -1, 250, 250, 250, 120, 120, 120, 35, 25, 2, {2, 5}, true, true, false, 0};
    Ship fed2{2, 1, "USS Defiant", Building::Destroyer, -1, 120, 120, 120, 80, 80, 80, 30, 20, 3, {2, 4}, true, true, false, 0};
    Ship fed3{3, 1, "USS Valiant", Building::Destroyer, -1, 120, 120, 120, 80, 80, 80, 30, 20, 3, {2, 6}, true, true, false, 0};

    // Klingon fleet (Owner 2): 1 Negh'Var, 2 Birds of Prey
    Ship klg1{11, 2, "IKS Negh'Var", Building::Destroyer, -1, 300, 300, 300, 100, 100, 100, 40, 30, 2, {14, 5}, true, true, false, 0};
    Ship klg2{12, 2, "IKS Rotarran", Building::Destroyer, -1, 90, 90, 90, 60, 60, 60, 25, 15, 3, {14, 4}, true, true, false, 0};
    klg2.canCloak = true;
    klg2.isCloaked = true;
    Ship klg3{13, 2, "IKS Koraga", Building::Destroyer, -1, 90, 90, 90, 60, 60, 60, 25, 15, 3, {14, 6}, true, true, false, 0};
    klg3.canCloak = true;
    klg3.isCloaked = true;

    enc.ships = {fed1, fed2, fed3, klg1, klg2, klg3};

    // Round 1: Player orders Enterprise to Assault Negh'Var, Defiants to Strafe Negh'Var
    enc.submitOrder({1, 11, TacticalManeuver::Assault});
    enc.submitOrder({2, 11, TacticalManeuver::Strafe});
    enc.submitOrder({3, 11, TacticalManeuver::Strafe});
    enc.resolveRound();

    // Round 2: Re-order Defiant to Harry
    enc.submitOrder({2, 11, TacticalManeuver::Harry});
    enc.resolveRound();

    // Mid-battle save
    std::string midSave = serializeEncounter(enc);

    // Execute to completion on original
    Encounter fullRun = enc;
    fullRun.runUntilFinished();

    // Reload from mid-battle save and execute to completion
    Encounter reloaded = deserializeEncounter(midSave);
    reloaded.runUntilFinished();

    // Verify bit-for-bit identical state
    CHECK(serializeEncounter(fullRun) == serializeEncounter(reloaded));
    CHECK(fullRun.isFinished && reloaded.isFinished);
    CHECK(fullRun.winnerOwnerId == reloaded.winnerOwnerId);
    CHECK(fullRun.currentRound == reloaded.currentRound);

    std::cout << "  Representative battle completed in " << fullRun.currentRound << " rounds. Winner: Owner " << fullRun.winnerOwnerId << std::endl;
    std::cout << "  PASS: mid-battle save/reload bit-for-bit equivalence verified." << std::endl;
}

} // namespace botf::tactical::tests

int main() {
    std::cout << "=== Running BOTF Tactical Combat Tests (Complete Suite) ===" << std::endl;
    using namespace botf::tactical::tests;
    const std::vector<std::pair<const char*, std::function<void()>>> suites{
        {"encounter construction validation", testEncounterConstructionValidation},
        {"shield damage absorption", testShieldDamageAbsorption},
        {"focus fire", testFocusFire},
        {"core maneuvers", testAllManeuvers},
        {"order authorization and atomic failure", testInvalidOrderAuthorizationAndAtomicFailure},
        {"deterministic replay", testDeterministicReplay},
        {"strict deserialization", testStrictDeserialization},
        {"campaign result mapping and relocation", testCampaignResultMappingAndRelocation},
        {"extended maneuvers", testExtendedManeuvers},
        {"tactical groups", testTacticalGroups},
        {"cloak lifecycle", testCloakLifecycle},
        {"representative Fed vs Klingon battle", testRepresentativeFedVsKlingonBattle},
    };
    int passed = 0, failed = 0;
    for (const auto& [name, suite] : suites) {
        try {
            suite();
            ++passed;
            std::cout << "PASS " << name << std::endl;
        } catch (const std::exception& ex) {
            ++failed;
            std::cout << "FAIL " << name << ": " << ex.what() << std::endl;
        }
    }
    std::cout << (failed ? "FAILED: " : "ALL PASSED: ") << passed << " passed, " << failed << " failed tactical suites" << std::endl;
    return failed ? 1 : 0;
}
