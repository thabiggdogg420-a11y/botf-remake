#include "Diplomacy.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace botf::diplomacy;

// -----------------------------------------------------------------------------
// Helper Assertions
// -----------------------------------------------------------------------------

#define DIPLOMACY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "ASSERTION FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            assert(cond); \
        } \
    } while (0)

// -----------------------------------------------------------------------------
// Test 1: Valid Peace Negotiation & Typed SetWarState
// -----------------------------------------------------------------------------
void testValidPeaceNegotiation() {
    std::cout << "Testing: valid peace negotiation and typed SetWarState..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId klingon = ParticipantId::major(2);

    ctx.setContact(fed, klingon, true);
    ctx.setWar(fed, klingon, true);
    ctx.setTrust(fed, klingon, 40);
    ctx.setBalance(fed, 1000);
    ctx.setBalance(klingon, 500);

    // Federation proposes Peace to Klingons with a 150 credit sweetener
    uint64_t propId = 0;
    std::string err;
    bool ok = DiplomacyEngine::submitProposal(
        state, ctx, fed, klingon,
        TreatyType::Peace, TreatyDuration::Indefinite,
        DiplomaticTone::Cordial, 150, 0,
        &propId, &err
    );
    DIPLOMACY_ASSERT(ok, "Peace proposal submission must succeed between belligerents at war");
    DIPLOMACY_ASSERT(propId == 1, "First proposal ID must be 1");
    DIPLOMACY_ASSERT(state.proposals.size() == 1, "Pending proposal must be registered");

    // Klingons accept the offer
    ok = DiplomacyEngine::respondToOffer(state, klingon, propId, true);
    DIPLOMACY_ASSERT(ok, "Accepting offer as authorized target must succeed");

    // Resolve turn
    TurnResolutionDelta delta = DiplomacyEngine::resolveTurn(state, ctx);

    DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "Proposal must be marked accepted");
    DIPLOMACY_ASSERT(delta.acceptedProposals[0] == propId, "Accepted proposal ID must match");

    // Verify delta operations
    bool hasCreditOp = false;
    bool hasPeaceOp = false;
    for (const auto& op : delta.operations) {
        if (op.opType == DiplomaticOpType::TransferCredits && op.partyA == fed && op.partyB == klingon && op.credits == 150) {
            hasCreditOp = true;
        }
        if (op.opType == DiplomaticOpType::SetWarState && op.partyA == fed && op.partyB == klingon) {
            DIPLOMACY_ASSERT(op.setAtWar == false, "Peace SetWarState operation must explicitly carry setAtWar = false");
            hasPeaceOp = true;
        }
    }
    DIPLOMACY_ASSERT(hasCreditOp, "Credits operation must transfer 150 from Fed to Klingon");
    DIPLOMACY_ASSERT(hasPeaceOp, "Peace operation must be emitted to clear war");

    std::cout << "  PASS: valid peace negotiation verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 2: Response Authorization (Self/Outbound and Third-Party Reject)
// -----------------------------------------------------------------------------
void testResponseAuthorization() {
    std::cout << "Testing: response authorization (outbound and third-party rejection)..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId klingon = ParticipantId::major(2);
    ParticipantId romulan = ParticipantId::major(3);

    ctx.setContact(fed, klingon, true);
    ctx.setContact(fed, romulan, true);
    ctx.setBalance(fed, 500);
    ctx.setBalance(klingon, 500);

    uint64_t propId = 0;
    DiplomacyEngine::submitProposal(
        state, ctx, fed, klingon,
        TreatyType::NonAggression, TreatyDuration::Turns25,
        DiplomaticTone::Neutral, 0, 0, &propId
    );

    // 1. Proposer (Fed) attempts to accept its own outbound proposal -> MUST FAIL
    bool okSelf = DiplomacyEngine::respondToOffer(state, fed, propId, true);
    DIPLOMACY_ASSERT(!okSelf, "Proposer cannot accept its own outbound proposal");
    DIPLOMACY_ASSERT(state.findProposal(propId)->status == ProposalStatus::Pending,
                     "Proposal must remain Pending after rejected self-response");

    // 2. Third party (Romulan) attempts to accept proposal between Fed and Klingon -> MUST FAIL
    bool okThird = DiplomacyEngine::respondToOffer(state, romulan, propId, true);
    DIPLOMACY_ASSERT(!okThird, "Third party cannot respond to another empire's proposal");
    DIPLOMACY_ASSERT(state.findProposal(propId)->status == ProposalStatus::Pending,
                     "Proposal must remain Pending after rejected third-party response");

    // 3. Authorized target (Klingon) responds -> SUCCEEDS
    bool okTarget = DiplomacyEngine::respondToOffer(state, klingon, propId, true);
    DIPLOMACY_ASSERT(okTarget, "Intended target must be authorized to respond");
    DIPLOMACY_ASSERT(state.findProposal(propId)->status == ProposalStatus::Accepted,
                     "Proposal status must be updated upon authorized acceptance");

    std::cout << "  PASS: response authorization verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 3: Rejected Incompatible Treaties & Legality Matrix
// -----------------------------------------------------------------------------
void testRejectedIncompatibleTreaty() {
    std::cout << "Testing: rejected incompatible treaties and legality matrix..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId romulan = ParticipantId::major(3);
    ParticipantId uncontacted = ParticipantId::major(4);

    ctx.setContact(fed, romulan, true);
    ctx.setContact(fed, uncontacted, false);
    ctx.setWar(fed, romulan, true);
    ctx.setBalance(fed, 1000);
    ctx.setBalance(romulan, 1000);

    std::string err;
    uint64_t propId = 0;

    // 1. Propose treaty to oneself -> strictly illegal
    bool ok = DiplomacyEngine::submitProposal(
        state, ctx, fed, fed,
        TreatyType::NonAggression, TreatyDuration::Turns25,
        DiplomaticTone::Neutral, 0, 0, &propId, &err
    );
    DIPLOMACY_ASSERT(!ok, "Self-proposals must be rejected");

    // 2. Propose treaty to uncontacted party -> illegal
    ok = DiplomacyEngine::submitProposal(
        state, ctx, fed, uncontacted,
        TreatyType::NonAggression, TreatyDuration::Turns25,
        DiplomaticTone::Neutral, 0, 0, &propId, &err
    );
    DIPLOMACY_ASSERT(!ok, "Uncontacted target proposal must be rejected");

    // 3. Propose Alliance while at war -> illegal (must negotiate peace first)
    ok = DiplomacyEngine::submitProposal(
        state, ctx, fed, romulan,
        TreatyType::Alliance, TreatyDuration::Indefinite,
        DiplomaticTone::Cordial, 0, 0, &propId, &err
    );
    DIPLOMACY_ASSERT(!ok, "Alliance while at war must be rejected");

    // 4. Propose Membership to a major empire -> strictly illegal
    ok = DiplomacyEngine::submitProposal(
        state, ctx, fed, romulan,
        TreatyType::Membership, TreatyDuration::Indefinite,
        DiplomaticTone::Cordial, 0, 0, &propId, &err
    );
    DIPLOMACY_ASSERT(!ok, "Proposing membership to another major empire must be rejected");

    // 5. Propose Non-Aggression when an Alliance is already active
    ctx.setWar(fed, romulan, false);
    ActiveTreaty alliance;
    alliance.id = 99;
    alliance.partyA = fed;
    alliance.partyB = romulan;
    alliance.type = TreatyType::Alliance;
    alliance.duration = TreatyDuration::Indefinite;
    alliance.status = TreatyStatus::Active;
    state.activeTreaties.push_back(alliance);

    ok = DiplomacyEngine::submitProposal(
        state, ctx, fed, romulan,
        TreatyType::NonAggression, TreatyDuration::Turns25,
        DiplomaticTone::Neutral, 0, 0, &propId, &err
    );
    DIPLOMACY_ASSERT(!ok, "Proposing NonAggression when Alliance is already active must be rejected");

    std::cout << "  PASS: incompatible treaties rejected cleanly." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 4: Incoming Player Accept, Reject, and Ignore
// -----------------------------------------------------------------------------
void testIncomingPlayerAcceptRejectIgnore() {
    std::cout << "Testing: incoming player accept, reject, and ignore..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId ferengi = ParticipantId::major(5);

    ctx.setHuman(fed);
    ctx.setContact(fed, ferengi, true);
    ctx.setWar(fed, ferengi, false);
    ctx.setBalance(fed, 500);
    ctx.setBalance(ferengi, 500);

    // Scenario A: Ferengi proposes Trade Pact to Player (Fed), Player Accepts
    uint64_t propA = 0;
    DiplomacyEngine::submitProposal(
        state, ctx, ferengi, fed,
        TreatyType::Trade, TreatyDuration::Turns50,
        DiplomaticTone::Cordial, 50, 0, &propA
    );
    DiplomacyEngine::respondToOffer(state, fed, propA, true);
    TurnResolutionDelta deltaA = DiplomacyEngine::resolveTurn(state, ctx);

    DIPLOMACY_ASSERT(deltaA.acceptedProposals.size() == 1, "Proposal A must be accepted");
    DIPLOMACY_ASSERT(state.activeTreaties.size() == 1, "Trade pact must now be active");
    DIPLOMACY_ASSERT(state.activeTreaties[0].type == TreatyType::Trade, "Treaty must be Trade");

    // Scenario B: Ferengi proposes NonAggression, Player Rejects
    uint64_t propB = 0;
    DiplomacyEngine::submitProposal(
        state, ctx, ferengi, fed,
        TreatyType::NonAggression, TreatyDuration::Turns25,
        DiplomaticTone::Neutral, 0, 0, &propB
    );
    DiplomacyEngine::respondToOffer(state, fed, propB, false, "Player declined non-aggression pact");
    TurnResolutionDelta deltaB = DiplomacyEngine::resolveTurn(state, ctx);

    DIPLOMACY_ASSERT(deltaB.rejectedProposals.size() == 1, "Proposal B must be rejected");
    DIPLOMACY_ASSERT(state.activeTreaties.size() == 1, "Active treaties must remain unchanged");

    // Scenario C: Ferengi proposes Friendship to Player. Player takes no action across turns -> Ignored!
    uint64_t propC = 0;
    DiplomacyEngine::submitProposal(
        state, ctx, ferengi, fed,
        TreatyType::Friendship, TreatyDuration::Turns50,
        DiplomaticTone::Demanding, 0, 0, &propC
    );
    TurnResolutionDelta deltaTurn1 = DiplomacyEngine::resolveTurn(state, ctx);
    DIPLOMACY_ASSERT(deltaTurn1.ignoredProposals.empty(), "Proposal should be pending for player review");

    TurnResolutionDelta deltaTurn2 = DiplomacyEngine::resolveTurn(state, ctx);
    DIPLOMACY_ASSERT(deltaTurn2.ignoredProposals.size() == 1, "Unanswered proposal must be ignored upon due turn expiry");
    DIPLOMACY_ASSERT(deltaTurn2.ignoredProposals[0] == propC, "Ignored proposal ID must match");

    std::cout << "  PASS: incoming player accept/reject/ignore lifecycle verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 5: Unaffordable Payment Atomic Overdraft Rejection
// -----------------------------------------------------------------------------
void testUnaffordablePaymentAtomicReject() {
    std::cout << "Testing: unaffordable payment atomic reject (overdraft protection)..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId cardassian = ParticipantId::major(4);

    ctx.setContact(fed, cardassian, true);
    ctx.setWar(fed, cardassian, false);
    ctx.setTrust(fed, cardassian, 50);

    ctx.setBalance(fed, 500);
    ctx.setBalance(cardassian, 100);

    uint64_t propId = 0;
    std::string err;
    bool ok = DiplomacyEngine::submitProposal(
        state, ctx, fed, cardassian,
        TreatyType::NonAggression, TreatyDuration::Turns25,
        DiplomaticTone::Cordial, 400, 0, &propId, &err
    );
    DIPLOMACY_ASSERT(ok, "Creation must succeed since Fed has 500 credits >= 400");

    DiplomacyEngine::respondToOffer(state, cardassian, propId, true);

    // Treasury drops before resolution
    ctx.setBalance(fed, 50);

    TurnResolutionDelta delta = DiplomacyEngine::resolveTurn(state, ctx);

    DIPLOMACY_ASSERT(delta.acceptedProposals.empty(), "Overdraft proposal must NOT be accepted");
    DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Proposal must be atomically rejected");
    DIPLOMACY_ASSERT(state.activeTreaties.empty(), "Treaty must NOT be ratified on overdraft");

    for (const auto& op : delta.operations) {
        DIPLOMACY_ASSERT(op.opType != DiplomaticOpType::TransferCredits, "No credit transfer can occur on overdraft");
    }

    std::cout << "  PASS: unaffordable payment atomic reject verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 6: Treaty Expiry Countdown and Archival
// -----------------------------------------------------------------------------
void testTreatyExpiry() {
    std::cout << "Testing: treaty duration expiry countdown and history retention..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId klingon = ParticipantId::major(2);

    ctx.setContact(fed, klingon, true);

    ActiveTreaty t;
    t.id = 1;
    t.partyA = fed;
    t.partyB = klingon;
    t.type = TreatyType::NonAggression;
    t.duration = TreatyDuration::Turns25;
    t.startTurn = 1;
    t.turnsRemaining = 2;
    t.status = TreatyStatus::Active;
    state.activeTreaties.push_back(t);

    TurnResolutionDelta delta1 = DiplomacyEngine::resolveTurn(state, ctx);
    DIPLOMACY_ASSERT(state.activeTreaties.size() == 1, "Treaty must remain active on turn 1");
    DIPLOMACY_ASSERT(state.activeTreaties[0].turnsRemaining == 1, "turnsRemaining must decrement to 1");

    TurnResolutionDelta delta2 = DiplomacyEngine::resolveTurn(state, ctx);
    DIPLOMACY_ASSERT(state.activeTreaties.empty(), "Expired treaty must be removed from active treaties");
    DIPLOMACY_ASSERT(delta2.expiredTreaties.size() == 1, "Expiry delta must record expired treaty");
    DIPLOMACY_ASSERT(state.treatyHistory.size() == 1, "Expired treaty must be retained in bounded treaty history");
    DIPLOMACY_ASSERT(state.treatyHistory[0].status == TreatyStatus::Expired, "Archived treaty status must be Expired");

    std::cout << "  PASS: treaty expiry countdown and archival verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 7: War Breaks Incompatible Treaties & Explicit SetWarState
// -----------------------------------------------------------------------------
void testWarBreaksIncompatibleTreaties() {
    std::cout << "Testing: war breaks incompatible treaties and sets typed war boolean..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId cardassian = ParticipantId::major(4);

    ctx.setContact(fed, cardassian, true);

    ActiveTreaty t1;
    t1.id = 10;
    t1.partyA = fed;
    t1.partyB = cardassian;
    t1.type = TreatyType::Friendship;
    t1.duration = TreatyDuration::Turns50;
    t1.turnsRemaining = 50;
    t1.status = TreatyStatus::Active;
    state.activeTreaties.push_back(t1);

    ActiveTreaty t2;
    t2.id = 11;
    t2.partyA = fed;
    t2.partyB = cardassian;
    t2.type = TreatyType::Trade;
    t2.duration = TreatyDuration::Indefinite;
    t2.turnsRemaining = -1;
    t2.status = TreatyStatus::Active;
    state.activeTreaties.push_back(t2);

    DIPLOMACY_ASSERT(state.activeTreaties.size() == 2, "Two active treaties exist");

    TurnResolutionDelta warDelta;
    DiplomacyEngine::declareWar(state, cardassian, fed, warDelta);

    DIPLOMACY_ASSERT(state.activeTreaties.empty(), "War must remove all treaties between belligerents from active list");
    DIPLOMACY_ASSERT(warDelta.brokenTreaties.size() == 2, "War delta must report 2 broken treaties");
    DIPLOMACY_ASSERT(warDelta.operations.front().setAtWar == true, "DeclareWar must set setAtWar = true");
    DIPLOMACY_ASSERT(state.treatyHistory.size() == 2, "Broken treaties must be preserved in history");
    DIPLOMACY_ASSERT(state.treatyHistory[0].status == TreatyStatus::BrokenByWar, "History record must mark BrokenByWar");

    std::cout << "  PASS: war breaking incompatible treaties verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 8: Minor Membership Revalidation at Settlement (Broken Friendship & Attitude Drop)
// -----------------------------------------------------------------------------
void testMinorMembershipRevalidationAtSettlement() {
    std::cout << "Testing: minor membership revalidation at settlement..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId vulcan = ParticipantId::minor(101);

    ctx.setContact(fed, vulcan, true);
    ctx.setMinorOwner(101, 0);

    MinorDiplomacyState minor;
    minor.minorId = 101;
    minor.systemId = 20;
    minor.raceName = "Vulcans";
    minor.membershipOwner = 0;
    minor.attitudes[1] = 85;
    minor.contacts[1] = true;
    minor.friendshipTurns[1] = 0; // Friendship from turn 0
    state.minorStates.push_back(minor);

    // Case A: Attitude drops below 80 before settlement
    {
        DiplomacyState testState = state;
        uint64_t propId = 0;
        bool ok = DiplomacyEngine::submitProposal(
            testState, ctx, fed, vulcan,
            TreatyType::Membership, TreatyDuration::Indefinite,
            DiplomaticTone::Cordial, 0, 0, &propId
        );
        DIPLOMACY_ASSERT(ok, "Submission succeeds with attitude 85");

        DiplomacyEngine::respondToOffer(testState, vulcan, propId, true);

        // Attitude drops before settlement!
        testState.minorStates.front().attitudes[1] = 75;

        TurnResolutionDelta delta = DiplomacyEngine::resolveTurn(testState, ctx);
        DIPLOMACY_ASSERT(delta.acceptedProposals.empty(), "Membership must not be accepted after attitude drop");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Membership rejected at settlement");
        DIPLOMACY_ASSERT(testState.minorStates.front().membershipOwner == 0, "Minor remains independent");
    }

    // Case B: Friendship was broken by war before settlement
    {
        DiplomacyState testState = state;
        uint64_t propId = 0;
        bool ok = DiplomacyEngine::submitProposal(
            testState, ctx, fed, vulcan,
            TreatyType::Membership, TreatyDuration::Indefinite,
            DiplomaticTone::Cordial, 0, 0, &propId
        );
        DIPLOMACY_ASSERT(ok, "Submission succeeds with friendship");

        DiplomacyEngine::respondToOffer(testState, vulcan, propId, true);

        // War breaks out with minor! Friendship broken!
        testState.minorStates.front().friendshipTurns[1] = -1;

        TurnResolutionDelta delta = DiplomacyEngine::resolveTurn(testState, ctx);
        DIPLOMACY_ASSERT(delta.acceptedProposals.empty(), "Membership must not be accepted with broken friendship");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Membership rejected at settlement");
    }

    std::cout << "  PASS: minor membership revalidation at settlement verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 9: Multi-Offer Working Resolution Ledger Overdraft Protection
// -----------------------------------------------------------------------------
void testMultiOfferWorkingLedgerOverdraw() {
    std::cout << "Testing: multi-offer working resolution ledger and overdraft protection..." << std::endl;

    const auto a = ParticipantId::major(1);
    const auto b = ParticipantId::major(2);
    const auto c = ParticipantId::major(3);

    SimpleDiplomacyContext ctx;
    ctx.setContact(a, b, true);
    ctx.setContact(a, c, true);
    ctx.setBalance(a, 100);
    ctx.setBalance(b, 100);
    ctx.setBalance(c, 100);

    DiplomacyState state;
    uint64_t first = 0, second = 0;
    bool ok1 = DiplomacyEngine::submitProposal(state, ctx, a, b, TreatyType::None,
        TreatyDuration::Indefinite, DiplomaticTone::Neutral, 80, 0, &first);
    bool ok2 = DiplomacyEngine::submitProposal(state, ctx, a, c, TreatyType::None,
        TreatyDuration::Indefinite, DiplomaticTone::Neutral, 80, 0, &second);
    DIPLOMACY_ASSERT(ok1 && ok2, "Both proposals individually affordable at submission");

    DiplomacyEngine::respondToOffer(state, b, first, true);
    DiplomacyEngine::respondToOffer(state, c, second, true);

    const auto delta = DiplomacyEngine::resolveTurn(state, ctx);

    DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "Only first proposal can be accepted with 100 balance");
    DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Second proposal must be rejected due to working overdraft");

    int64_t paid = 0;
    for (const auto& op : delta.operations) {
        if (op.opType == DiplomaticOpType::TransferCredits && op.partyA == a) {
            paid += op.credits;
        }
    }
    DIPLOMACY_ASSERT(paid == 80, "Total paid must be exactly 80, not 160");

    std::cout << "  PASS: multi-offer working resolution ledger overdraft protection verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 10: Three-Party Mixed Payments Working Ledger
// -----------------------------------------------------------------------------
void testThreePartyMixedPaymentsWorkingLedger() {
    std::cout << "Testing: three-party mixed payments in working resolution ledger..." << std::endl;

    const auto a = ParticipantId::major(1);
    const auto b = ParticipantId::major(2);
    const auto c = ParticipantId::major(3);

    SimpleDiplomacyContext ctx;
    ctx.setContact(a, b, true);
    ctx.setContact(b, c, true);

    ctx.setBalance(a, 100);
    ctx.setBalance(b, 40);
    ctx.setBalance(c, 10);

    DiplomacyState state;
    uint64_t prop1 = 0, prop2 = 0;
    DiplomacyEngine::submitProposal(state, ctx, a, b, TreatyType::None,
        TreatyDuration::Indefinite, DiplomaticTone::Cordial, 50, 0, &prop1);

    ctx.setBalance(b, 70); // Allow creation
    DiplomacyEngine::submitProposal(state, ctx, b, c, TreatyType::None,
        TreatyDuration::Indefinite, DiplomaticTone::Cordial, 70, 0, &prop2);
    ctx.setBalance(b, 40); // Restore pre-resolution balance

    DiplomacyEngine::respondToOffer(state, b, prop1, true);
    DiplomacyEngine::respondToOffer(state, c, prop2, true);

    const auto delta = DiplomacyEngine::resolveTurn(state, ctx);

    DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 2, "Both proposals should succeed due to intra-turn receipts");

    int64_t bReceived = 0, bPaid = 0;
    for (const auto& op : delta.operations) {
        if (op.opType == DiplomaticOpType::TransferCredits) {
            if (op.partyB == b) bReceived += op.credits;
            if (op.partyA == b) bPaid += op.credits;
        }
    }
    DIPLOMACY_ASSERT(bReceived == 50, "B must receive 50 from A");
    DIPLOMACY_ASSERT(bPaid == 70, "B must pay 70 to C");

    std::cout << "  PASS: three-party mixed payments verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 11: Simultaneous Minor Membership Competition & Deterministic Seeded RNG
// -----------------------------------------------------------------------------
void testSimultaneousMinorMembershipCompetition() {
    std::cout << "Testing: simultaneous minor membership competition with seeded RNG tie-break..." << std::endl;

    const auto a = ParticipantId::major(1);
    const auto b = ParticipantId::major(2);
    const auto m = ParticipantId::minor(101);

    SimpleDiplomacyContext ctx;
    ctx.setContact(a, m, true);
    ctx.setContact(b, m, true);
    ctx.setMinorOwner(101, 0);

    // Scenario 1: Differentiated attitudes (90 vs 85) -> higher attitude strictly wins
    {
        DiplomacyState state;
        MinorDiplomacyState minor;
        minor.minorId = 101;
        minor.systemId = 101;
        minor.raceName = "Test Minor";
        minor.attitudes[1] = 90;
        minor.attitudes[2] = 85;
        minor.friendshipTurns[1] = 0;
        minor.friendshipTurns[2] = 0;
        state.minorStates.push_back(minor);

        uint64_t propA = 0, propB = 0;
        DiplomacyEngine::submitProposal(state, ctx, a, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propA);
        DiplomacyEngine::submitProposal(state, ctx, b, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propB);

        DiplomacyEngine::respondToOffer(state, m, propA, true);
        DiplomacyEngine::respondToOffer(state, m, propB, true);

        const auto delta = DiplomacyEngine::resolveTurn(state, ctx);

        int transfers = 0;
        for (const auto& op : delta.operations) {
            if (op.opType == DiplomaticOpType::TransferMinorMembership) transfers++;
        }
        DIPLOMACY_ASSERT(transfers == 1, "Exactly ONE membership transfer operation can be emitted");
        DIPLOMACY_ASSERT(state.minorStates.front().membershipOwner == 1,
                         "Minor owner must be major 1 (higher attitude winner)");
        DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "Only winner proposal accepted");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Losing proposal must be rejected");
    }

    // Scenario 2: Equal attitudes (90 vs 90) -> deterministic RNG tie-break
    {
        DiplomacyState state;
        MinorDiplomacyState minor;
        minor.minorId = 101;
        minor.systemId = 101;
        minor.raceName = "Equal Minor";
        minor.attitudes[1] = 90;
        minor.attitudes[2] = 90;
        minor.friendshipTurns[1] = 0;
        minor.friendshipTurns[2] = 0;
        state.minorStates.push_back(minor);

        uint64_t propA = 0, propB = 0;
        DiplomacyEngine::submitProposal(state, ctx, a, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propA);
        DiplomacyEngine::submitProposal(state, ctx, b, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propB);

        DiplomacyEngine::respondToOffer(state, m, propA, true);
        DiplomacyEngine::respondToOffer(state, m, propB, true);

        // Run with seed 1337
        const auto delta = DiplomacyEngine::resolveTurn(state, ctx, 1337);

        int transfers = 0;
        for (const auto& op : delta.operations) {
            if (op.opType == DiplomaticOpType::TransferMinorMembership) transfers++;
        }
        DIPLOMACY_ASSERT(transfers == 1, "Exactly ONE membership transfer on tie");
        int winnerOwner = state.minorStates.front().membershipOwner;
        DIPLOMACY_ASSERT(winnerOwner == 1 || winnerOwner == 2, "Winner must be either major 1 or 2");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Loser must be explicitly rejected");
    }

    // Scenario 3: Broken friendship on higher-attitude candidate (95 broken vs 90 valid)
    // Ineligible top candidate must NOT suppress valid candidate
    {
        DiplomacyState state;
        MinorDiplomacyState minor;
        minor.minorId = 101;
        minor.systemId = 101;
        minor.raceName = "Eligibility Probe Minor";
        minor.attitudes[1] = 95;
        minor.attitudes[2] = 90;
        minor.friendshipTurns[1] = 0;
        minor.friendshipTurns[2] = 0;
        state.minorStates.push_back(minor);

        uint64_t propA = 0, propB = 0;
        DiplomacyEngine::submitProposal(state, ctx, a, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propA);
        DiplomacyEngine::submitProposal(state, ctx, b, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propB);

        DiplomacyEngine::respondToOffer(state, m, propA, true);
        DiplomacyEngine::respondToOffer(state, m, propB, true);

        // Major 1's friendship is broken prior to settlement
        state.minorStates.front().friendshipTurns[1] = -1;

        const auto delta = DiplomacyEngine::resolveTurn(state, ctx);

        int transfers = 0;
        for (const auto& op : delta.operations) {
            if (op.opType == DiplomaticOpType::TransferMinorMembership) transfers++;
        }
        DIPLOMACY_ASSERT(transfers == 1, "Exactly ONE transfer operation emitted");
        DIPLOMACY_ASSERT(state.minorStates.front().membershipOwner == 2,
                         "Eligible major 2 must win membership; broken friendship major 1 must not suppress major 2");
        DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "Major 2 proposal accepted");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Major 1 proposal rejected due to broken friendship");
    }

    // Scenario 4: Unaffordable top candidate (95 offer 100 with bal 50 vs 90 offer 0 with bal 100)
    // Overdraft on top candidate must NOT suppress affordable valid candidate
    {
        DiplomacyState state;
        MinorDiplomacyState minor;
        minor.minorId = 101;
        minor.systemId = 101;
        minor.raceName = "Overdraft Probe Minor";
        minor.attitudes[1] = 95;
        minor.attitudes[2] = 90;
        minor.friendshipTurns[1] = 0;
        minor.friendshipTurns[2] = 0;
        state.minorStates.push_back(minor);

        SimpleDiplomacyContext budgetCtx = ctx;
        budgetCtx.setBalance(a, 100); // Allow submission of 100
        budgetCtx.setBalance(b, 100);

        uint64_t propA = 0, propB = 0;
        DiplomacyEngine::submitProposal(state, budgetCtx, a, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 100, 0, &propA);
        DiplomacyEngine::submitProposal(state, budgetCtx, b, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propB);

        DiplomacyEngine::respondToOffer(state, m, propA, true);
        DiplomacyEngine::respondToOffer(state, m, propB, true);

        // Major 1 treasury drops to 50 before settlement (cannot afford 100)
        budgetCtx.setBalance(a, 50);

        const auto delta = DiplomacyEngine::resolveTurn(state, budgetCtx);

        int transfers = 0;
        for (const auto& op : delta.operations) {
            if (op.opType == DiplomaticOpType::TransferMinorMembership) transfers++;
        }
        DIPLOMACY_ASSERT(transfers == 1, "Exactly ONE transfer operation emitted");
        DIPLOMACY_ASSERT(state.minorStates.front().membershipOwner == 2,
                         "Affordable major 2 must win membership; overdraft major 1 must not suppress major 2");
        DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "Major 2 proposal accepted");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Major 1 proposal rejected due to overdraft");
    }

    // Scenario 5: Both candidates unaffordable -> both rejected, minor remains independent
    {
        DiplomacyState state;
        MinorDiplomacyState minor;
        minor.minorId = 101;
        minor.systemId = 101;
        minor.raceName = "Double Overdraft Minor";
        minor.attitudes[1] = 95;
        minor.attitudes[2] = 90;
        minor.friendshipTurns[1] = 0;
        minor.friendshipTurns[2] = 0;
        state.minorStates.push_back(minor);

        SimpleDiplomacyContext budgetCtx = ctx;
        budgetCtx.setBalance(a, 100);
        budgetCtx.setBalance(b, 100);

        uint64_t propA = 0, propB = 0;
        DiplomacyEngine::submitProposal(state, budgetCtx, a, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 100, 0, &propA);
        DiplomacyEngine::submitProposal(state, budgetCtx, b, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 100, 0, &propB);

        DiplomacyEngine::respondToOffer(state, m, propA, true);
        DiplomacyEngine::respondToOffer(state, m, propB, true);

        // Both majors cannot afford 100
        budgetCtx.setBalance(a, 50);
        budgetCtx.setBalance(b, 50);

        const auto delta = DiplomacyEngine::resolveTurn(state, budgetCtx);

        int transfers = 0;
        for (const auto& op : delta.operations) {
            if (op.opType == DiplomaticOpType::TransferMinorMembership) transfers++;
        }
        DIPLOMACY_ASSERT(transfers == 0, "No transfers when all candidates overdraft");
        DIPLOMACY_ASSERT(state.minorStates.front().membershipOwner == 0, "Minor remains independent");
        DIPLOMACY_ASSERT(delta.acceptedProposals.empty(), "No proposals accepted");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 2, "Both proposals rejected due to overdraft");
    }

    // Scenario 6: Already-rejected attitude95 offer must NOT suppress accepted eligible attitude90 offer
    {
        DiplomacyState state;
        MinorDiplomacyState minor;
        minor.minorId = 101;
        minor.systemId = 101;
        minor.raceName = "PreRejected Probe Minor";
        minor.attitudes[1] = 95;
        minor.attitudes[2] = 90;
        minor.friendshipTurns[1] = 0;
        minor.friendshipTurns[2] = 0;
        state.minorStates.push_back(minor);

        uint64_t propA = 0, propB = 0;
        DiplomacyEngine::submitProposal(state, ctx, a, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propA);
        DiplomacyEngine::submitProposal(state, ctx, b, m, TreatyType::Membership,
            TreatyDuration::Indefinite, DiplomaticTone::Neutral, 0, 0, &propB);

        // Offer A is rejected by minor, Offer B is accepted by minor
        DiplomacyEngine::respondToOffer(state, m, propA, false, "Not interested in empire 1");
        DiplomacyEngine::respondToOffer(state, m, propB, true);

        const auto delta = DiplomacyEngine::resolveTurn(state, ctx);

        int transfers = 0;
        for (const auto& op : delta.operations) {
            if (op.opType == DiplomaticOpType::TransferMinorMembership) transfers++;
        }
        DIPLOMACY_ASSERT(transfers == 1, "Exactly ONE transfer operation emitted");
        DIPLOMACY_ASSERT(state.minorStates.front().membershipOwner == 2,
                         "Accepted major 2 must win membership; pre-rejected major 1 must not suppress major 2");
        DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "Major 2 proposal accepted");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "Major 1 proposal rejected");
    }

    std::cout << "  PASS: simultaneous minor membership competition and RNG tie-break verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 12: Intra-Turn Treaty Downgrade & War Proposal Execution
// -----------------------------------------------------------------------------
void testIntraTurnTreatyDowngradeAndWar() {
    std::cout << "Testing: intra-turn treaty downgrade prevention and war proposal..." << std::endl;

    const auto a = ParticipantId::major(1);
    const auto b = ParticipantId::major(2);

    SimpleDiplomacyContext ctx;
    ctx.setContact(a, b, true);
    ctx.setWar(a, b, false);
    ctx.setBalance(a, 500);
    ctx.setBalance(b, 500);

    // Part A: Treaty downgrade prevention
    {
        DiplomacyState state;
        uint64_t first = 0, second = 0;
        DiplomacyEngine::submitProposal(state, ctx, a, b, TreatyType::Friendship,
            TreatyDuration::Turns50, DiplomaticTone::Neutral, 0, 0, &first);
        DiplomacyEngine::submitProposal(state, ctx, a, b, TreatyType::NonAggression,
            TreatyDuration::Turns25, DiplomaticTone::Neutral, 0, 0, &second);

        DiplomacyEngine::respondToOffer(state, b, first, true);
        DiplomacyEngine::respondToOffer(state, b, second, true);

        const auto delta = DiplomacyEngine::resolveTurn(state, ctx);

        DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "Only Friendship should be accepted");
        DIPLOMACY_ASSERT(delta.rejectedProposals.size() == 1, "NonAggression must be rejected as downgrade");
        DIPLOMACY_ASSERT(state.activeTreaties.size() == 1, "Exactly one active treaty");
        DIPLOMACY_ASSERT(state.activeTreaties.front().type == TreatyType::Friendship,
                         "Active treaty must be Friendship, not downgraded to NonAggression");
    }

    // Part B: War proposal execution
    {
        DiplomacyState state;
        uint64_t warProp = 0;
        bool ok = DiplomacyEngine::submitProposal(state, ctx, a, b, TreatyType::War,
            TreatyDuration::Indefinite, DiplomaticTone::Hostile, 0, 0, &warProp);
        DIPLOMACY_ASSERT(ok, "War proposal submission valid when not already at war");

        DiplomacyEngine::respondToOffer(state, b, warProp, true);
        const auto delta = DiplomacyEngine::resolveTurn(state, ctx);

        int warOps = 0;
        for (const auto& op : delta.operations) {
            if (op.opType == DiplomaticOpType::SetWarState) {
                DIPLOMACY_ASSERT(op.setAtWar == true, "War proposal SetWarState operation must explicitly carry setAtWar = true");
                warOps++;
            }
        }
        DIPLOMACY_ASSERT(warOps == 1, "War proposal must emit SetWarState operation");
        DIPLOMACY_ASSERT(delta.acceptedProposals.size() == 1, "War proposal accepted");
    }

    std::cout << "  PASS: intra-turn treaty downgrade and war proposal verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 13: Strict Save/Reload Roundtrip and Comprehensive Invariant Matrix
// -----------------------------------------------------------------------------
void testStrictSaveReloadAndInvariantMatrix() {
    std::cout << "Testing: strict save/reload and comprehensive invariant matrix..." << std::endl;

    const auto a = ParticipantId::major(1);
    const auto b = ParticipantId::major(2);
    const auto m = ParticipantId::minor(102);

    SimpleDiplomacyContext ctx;
    ctx.setContact(a, b, true);
    ctx.setContact(a, m, true);

    DiplomacyState state;
    state.currentTurn = 15;
    state.nextProposalId = 10;
    state.nextTreatyId = 5;

    // Add proposal with rejection reason containing spaces
    Proposal p;
    p.id = 1;
    p.proposer = a;
    p.target = b;
    p.treatyType = TreatyType::Friendship;
    p.duration = TreatyDuration::Turns50;
    p.tone = DiplomaticTone::Neutral;
    p.creditOffer = 200;
    p.creditDemand = 50;
    p.submittedTurn = 14;
    p.dueTurn = 15;
    p.status = ProposalStatus::Rejected;
    p.resolved = true;
    p.rejectionReason = "Proposer overdraft: insufficient funds in resolution ledger";
    state.proposals.push_back(p);

    // Add active treaty
    ActiveTreaty t;
    t.id = 2;
    t.partyA = a;
    t.partyB = m;
    t.type = TreatyType::Friendship;
    t.duration = TreatyDuration::Indefinite;
    t.startTurn = 8;
    t.turnsRemaining = -1;
    t.status = TreatyStatus::Active;
    state.activeTreaties.push_back(t);

    // Add minor state with spaced race name
    MinorDiplomacyState min;
    min.minorId = 102;
    min.systemId = 44;
    min.raceName = "Bajoran People";
    min.membershipOwner = 0;
    min.membershipTurn = -1;
    min.attitudes[1] = 72;
    min.contacts[1] = true;
    min.friendshipTurns[1] = 8;
    state.minorStates.push_back(min);

    // Add event with spaced description
    DiplomaticEvent e;
    e.turn = 8;
    e.type = DiplomaticEventType::TreatyEstablished;
    e.partyA = a;
    e.partyB = m;
    e.numericValue = 100;
    e.description = "Friendship ratified with Bajoran People";
    state.eventHistory.push_back(e);

    // 1. Happy-path exact roundtrip
    std::string serialized = serializeDiplomacyState(state);
    DiplomacyState loaded;
    std::string err;
    bool ok = deserializeDiplomacyState(serialized, loaded, &err);
    DIPLOMACY_ASSERT(ok, "Valid state must deserialize cleanly");
    DIPLOMACY_ASSERT(loaded == state, "Loaded state must match original state identically");
    DIPLOMACY_ASSERT(loaded.proposals.front().rejectionReason == p.rejectionReason,
                     "Rejection reason must be preserved identically");
    DIPLOMACY_ASSERT(loaded.minorStates.front().raceName == min.raceName,
                     "Race name with spaces must be preserved identically");
    DIPLOMACY_ASSERT(loaded.eventHistory.front().description == e.description,
                     "Event description with spaces must be preserved identically");

    // 2. Named Invariant Rejection Cases:
    DiplomacyState badState;

    // Bad Enum TreatyType 999
    {
        DiplomacyState s = state;
        s.proposals.front().treatyType = static_cast<TreatyType>(999);
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "TreatyType 999 must be rejected");
    }

    // Bad Duration 99
    {
        DiplomacyState s = state;
        s.proposals.front().duration = static_cast<TreatyDuration>(99);
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "TreatyDuration 99 must be rejected");
    }

    // Bad DiplomaticTone 99
    {
        DiplomacyState s = state;
        s.proposals.front().tone = static_cast<DiplomaticTone>(99);
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "DiplomaticTone 99 must be rejected");
    }

    // Bad ProposalStatus 99
    {
        DiplomacyState s = state;
        s.proposals.front().status = static_cast<ProposalStatus>(99);
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "ProposalStatus 99 must be rejected");
    }

    // Duplicate Proposal ID
    {
        DiplomacyState s = state;
        s.proposals.push_back(s.proposals.front());
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "Duplicate proposal ID must be rejected");
    }

    // Duplicate Treaty ID
    {
        DiplomacyState s = state;
        s.activeTreaties.push_back(s.activeTreaties.front());
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "Duplicate treaty ID must be rejected");
    }

    // Non-monotonic nextProposalId
    {
        DiplomacyState s = state;
        s.nextProposalId = s.proposals.front().id;
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "Non-monotonic nextProposalId must be rejected");
    }

    // Non-monotonic nextTreatyId
    {
        DiplomacyState s = state;
        s.nextTreatyId = s.activeTreaties.front().id;
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(serializeDiplomacyState(s), badState, &err),
                         "Non-monotonic nextTreatyId must be rejected");
    }

    // Trailing junk
    {
        std::string junk = serialized + "JUNK_TOKEN\n";
        DIPLOMACY_ASSERT(!deserializeDiplomacyState(junk, badState, &err),
                         "Trailing junk tokens must be rejected");
    }

    // 3. Literal Underscores & Mixed Spaces/Underscores Roundtrip
    {
        DiplomacyState s = state;
        s.minorStates.front().raceName = "Vulcan_High_Council With Mixed_Spaces";
        s.proposals.front().rejectionReason = "Not_ready For_Peace With Mixed_Underscores and Spaces";
        s.eventHistory.front().description = "Event_Log_With_Literal_Underscores and Spaces";

        std::string sData = serializeDiplomacyState(s);
        DiplomacyState loadedS;
        ok = deserializeDiplomacyState(sData, loadedS, &err);
        DIPLOMACY_ASSERT(ok, "Mixed underscores and spaces must deserialize cleanly");
        DIPLOMACY_ASSERT(loadedS.minorStates.front().raceName == "Vulcan_High_Council With Mixed_Spaces",
                         "Literal underscores and spaces in raceName must survive roundtrip");
        DIPLOMACY_ASSERT(loadedS.proposals.front().rejectionReason == "Not_ready For_Peace With Mixed_Underscores and Spaces",
                         "Literal underscores and spaces in rejectionReason must survive roundtrip");
        DIPLOMACY_ASSERT(loadedS.eventHistory.front().description == "Event_Log_With_Literal_Underscores and Spaces",
                         "Literal underscores and spaces in event description must survive roundtrip");
    }

    // 4. Empty Text Roundtrip
    {
        DiplomacyState s = state;
        s.minorStates.front().raceName = "";
        s.proposals.front().rejectionReason = "";
        s.eventHistory.front().description = "";

        std::string sData = serializeDiplomacyState(s);
        DiplomacyState loadedS;
        ok = deserializeDiplomacyState(sData, loadedS, &err);
        DIPLOMACY_ASSERT(ok, "Empty string fields must deserialize cleanly");
        DIPLOMACY_ASSERT(loadedS.minorStates.front().raceName.empty(), "Empty raceName must roundtrip as empty");
        DIPLOMACY_ASSERT(loadedS.proposals.front().rejectionReason.empty(), "Empty rejectionReason must roundtrip as empty");
        DIPLOMACY_ASSERT(loadedS.eventHistory.front().description.empty(), "Empty description must roundtrip as empty");
    }

    // 5. Malformed Escape Sequences Rejection
    {
        // Truncated percent escape (%2)
        std::string badTrunc = serialized;
        size_t pos = badTrunc.find("Bajoran%20People");
        if (pos != std::string::npos) {
            badTrunc.replace(pos, 16, "Bajoran%2");
            DIPLOMACY_ASSERT(!deserializeDiplomacyState(badTrunc, badState, &err),
                             "Truncated percent escape must be rejected");
        }

        // Non-hex percent escape (%2G)
        std::string badHex = serialized;
        pos = badHex.find("Bajoran%20People");
        if (pos != std::string::npos) {
            badHex.replace(pos, 16, "Bajoran%2GPeople");
            DIPLOMACY_ASSERT(!deserializeDiplomacyState(badHex, badState, &err),
                             "Non-hex percent escape must be rejected");
        }
    }

    // 6. Explicit Version Handling
    {
        // Unsupported future version
        std::string badVer = serialized;
        size_t vPos = badVer.find("BOTF_DIPLOMACY_V2");
        if (vPos != std::string::npos) {
            badVer.replace(vPos, 17, "BOTF_DIPLOMACY_V99");
            DIPLOMACY_ASSERT(!deserializeDiplomacyState(badVer, badState, &err),
                             "Unsupported save version V99 must be rejected");
        }

        // Backward compatibility with legacy V1 saves
        std::string legacyV1 = serialized;
        vPos = legacyV1.find("BOTF_DIPLOMACY_V2");
        if (vPos != std::string::npos) {
            legacyV1.replace(vPos, 17, "BOTF_DIPLOMACY_V1");
            DiplomacyState v1Loaded;
            DIPLOMACY_ASSERT(deserializeDiplomacyState(legacyV1, v1Loaded, &err),
                             "Legacy BOTF_DIPLOMACY_V1 saves must still parse cleanly");
        }
    }

    std::cout << "  PASS: strict save/reload and invariant matrix verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Test 14: Deterministic Seeded AI Decisions and Personality Differentiation
// -----------------------------------------------------------------------------
void testDeterministicAiDecisions() {
    std::cout << "Testing: deterministic seeded AI decisions and personality differentiation..." << std::endl;

    DiplomacyState state;
    SimpleDiplomacyContext ctx;

    ParticipantId fed = ParticipantId::major(1);
    ParticipantId klingon = ParticipantId::major(2);
    ParticipantId ferengi = ParticipantId::major(5);

    ctx.setContact(fed, klingon, true);
    ctx.setContact(fed, ferengi, true);
    ctx.setTrust(klingon, fed, 50);
    ctx.setTrust(ferengi, fed, 50);
    ctx.setBalance(fed, 2000);
    ctx.setBalance(klingon, 1000);
    ctx.setBalance(ferengi, 1000);

    Proposal propTrade;
    propTrade.id = 42;
    propTrade.proposer = fed;
    propTrade.target = ferengi;
    propTrade.treatyType = TreatyType::Trade;
    propTrade.tone = DiplomaticTone::Cordial;
    propTrade.creditOffer = 500;

    AiDecisionResult r1 = evaluateAiProposal(state, ctx, propTrade, 99999ULL, MajorPersonality::Ferengi);
    AiDecisionResult r2 = evaluateAiProposal(state, ctx, propTrade, 99999ULL, MajorPersonality::Ferengi);

    DIPLOMACY_ASSERT(r1.decision == r2.decision, "Identical inputs & seed must produce identical decision");
    DIPLOMACY_ASSERT(r1.score == r2.score, "Identical inputs & seed must produce identical score");
    DIPLOMACY_ASSERT(r1.decision == AiDecision::Accept, "Ferengi must accept trade with credit sweetener");

    Proposal propKlingon;
    propKlingon.id = 43;
    propKlingon.proposer = fed;
    propKlingon.target = klingon;
    propKlingon.treatyType = TreatyType::NonAggression;
    propKlingon.tone = DiplomaticTone::Demanding;
    propKlingon.creditOffer = 0;

    AiDecisionResult rKlingon = evaluateAiProposal(state, ctx, propKlingon, 99999ULL, MajorPersonality::Klingon);
    DIPLOMACY_ASSERT(rKlingon.decision != AiDecision::Accept, "Klingons must reject demanding proposal from peer");

    std::cout << "  PASS: deterministic AI decisions and personality differentiation verified." << std::endl;
}

// -----------------------------------------------------------------------------
// Main Test Runner
// -----------------------------------------------------------------------------
int main() {
    std::cout << "=======================================================" << std::endl;
    std::cout << "BOTF Diplomacy Engine Test Suite (botf::diplomacy)" << std::endl;
    std::cout << "=======================================================" << std::endl;

    try {
        testValidPeaceNegotiation();
        testResponseAuthorization();
        testRejectedIncompatibleTreaty();
        testIncomingPlayerAcceptRejectIgnore();
        testUnaffordablePaymentAtomicReject();
        testTreatyExpiry();
        testWarBreaksIncompatibleTreaties();
        testMinorMembershipRevalidationAtSettlement();
        testMultiOfferWorkingLedgerOverdraw();
        testThreePartyMixedPaymentsWorkingLedger();
        testSimultaneousMinorMembershipCompetition();
        testIntraTurnTreatyDowngradeAndWar();
        testStrictSaveReloadAndInvariantMatrix();
        testDeterministicAiDecisions();

        std::cout << "\n>>> ALL 14 DIPLOMACY TEST SUITES PASSED CLEANLY! <<<" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAILED with exception: " << ex.what() << std::endl;
        return 1;
    }
}
