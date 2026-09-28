#include "Diplomacy.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace botf::diplomacy;

// -----------------------------------------------------------------------------
// Helper Assertions
// -----------------------------------------------------------------------------

// Throws (never compiled out under NDEBUG, unlike assert).
#define DIPLOMACY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::ostringstream diplomacyAssertMessage; \
            diplomacyAssertMessage << "ASSERTION FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")"; \
            throw std::runtime_error(diplomacyAssertMessage.str()); \
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

// =============================================================================
// Lane B regression and modern-rule suites
// =============================================================================

namespace {
SimpleDiplomacyContext fiveEmpireContext() {
    SimpleDiplomacyContext ctx;
    ctx.majors = {1, 2, 3, 4, 5};
    for (int a = 1; a <= 5; ++a) {
        for (int b = a + 1; b <= 5; ++b) ctx.setContact(ParticipantId::major(a), ParticipantId::major(b), true);
        ctx.setBalance(ParticipantId::major(a), 1000);
        ctx.setPower(ParticipantId::major(a), 100, 100);
    }
    return ctx;
}
}

// Bug 1: proposals were never pruned; creation refused at 128 stored rows forever.
void testProposalTableNeverLocksUp() {
    std::cout << "Testing: proposal table never locks up (bug 1)..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setTrust(ParticipantId::major(1), ParticipantId::major(2), 0); // Klingons refuse everything
    ctx.setHuman(ParticipantId::major(9));
    DiplomacyState state;
    for (int turn = 0; turn < 400; ++turn) {
        std::string err;
        const bool ok = DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(1), ParticipantId::major(2),
            TreatyType::Trade, TreatyDuration::Turns25, DiplomaticTone::Neutral, 0, 0, nullptr, &err);
        DIPLOMACY_ASSERT(ok, "Submission refused at turn " << turn << ": " << err);
        DiplomacyEngine::resolveTurn(state, ctx);
        DIPLOMACY_ASSERT(state.proposals.size() <= DiplomacyState::MaxPendingProposals, "Proposal table exceeded its bound");
    }
    DIPLOMACY_ASSERT(state.nextProposalId == 401, "Every submission must have been accepted");

    // A table full of resolved rows (e.g. from an old save) is purged on the next submission.
    DiplomacyState full;
    for (uint64_t i = 1; i <= DiplomacyState::MaxPendingProposals; ++i) {
        Proposal p;
        p.id = i;
        p.proposer = ParticipantId::major(3);
        p.target = ParticipantId::major(4);
        p.treatyType = TreatyType::Trade;
        p.status = ProposalStatus::Rejected;
        p.resolved = true;
        full.proposals.push_back(p);
    }
    full.nextProposalId = DiplomacyState::MaxPendingProposals + 1;
    std::string err;
    DIPLOMACY_ASSERT(DiplomacyEngine::submitProposal(full, ctx, ParticipantId::major(1), ParticipantId::major(2),
        TreatyType::NonAggression, TreatyDuration::Turns25, DiplomaticTone::Cordial, 0, 0, nullptr, &err),
        "Resolved rows must not block new proposals: " << err);
    DIPLOMACY_ASSERT(full.proposals.size() == 1, "Resolved rows must be purged when the table is full");

    // Open proposals per proposer are capped so one empire cannot exhaust the table.
    DiplomacyState spam;
    int accepted = 0;
    for (int target = 2; target <= 5; ++target) {
        for (TreatyType type : {TreatyType::NonAggression, TreatyType::Friendship, TreatyType::Affiliation,
                                TreatyType::Alliance, TreatyType::Trade}) {
            if (DiplomacyEngine::submitProposal(spam, ctx, ParticipantId::major(1), ParticipantId::major(target),
                    type, TreatyDuration::Turns25, DiplomaticTone::Cordial, 0, 0)) ++accepted;
        }
    }
    DIPLOMACY_ASSERT(accepted == static_cast<int>(DiplomacyState::MaxPendingPerProposer), "Per-proposer cap not enforced");
    std::cout << "  PASS: 400 turns of proposals, purge of resolved rows, per-proposer cap." << std::endl;
}

// Bug 2: declareWar marked proposals Rejected but left them unresolved.
void testDeclareWarResolvesVoidedProposals() {
    std::cout << "Testing: declareWar resolves the proposals it voids (bug 2)..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setHuman(ParticipantId::major(1));
    DiplomacyState state;
    uint64_t incoming = 0, outgoing = 0;
    DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(2), ParticipantId::major(1), TreatyType::NonAggression,
        TreatyDuration::Turns25, DiplomaticTone::Cordial, 0, 0, &incoming, nullptr, 3);
    DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(1), ParticipantId::major(2), TreatyType::Trade,
        TreatyDuration::Turns25, DiplomaticTone::Cordial, 0, 0, &outgoing);
    TurnResolutionDelta warDelta;
    DiplomacyEngine::declareWar(state, ParticipantId::major(1), ParticipantId::major(2), warDelta);
    for (uint64_t id : {incoming, outgoing}) {
        const auto* p = state.findProposal(id);
        DIPLOMACY_ASSERT(p && p->resolved && p->status == ProposalStatus::Cancelled, "Voided proposal must be resolved as Cancelled");
    }
    ctx.setWar(ParticipantId::major(1), ParticipantId::major(2), true);
    const auto next = DiplomacyEngine::resolveTurn(state, ctx);
    DIPLOMACY_ASSERT(next.rejectedProposals.empty() && next.ignoredProposals.empty(), "Voided proposals reported again next turn");
    for (const auto& e : next.events) {
        DIPLOMACY_ASSERT(e.type != DiplomaticEventType::ProposalRejected, "Voided proposal re-reported as rejected by target");
    }
    DIPLOMACY_ASSERT(state.findProposal(incoming) == nullptr, "Resolved proposals are purged on the following turn");
    std::cout << "  PASS: voided proposals resolved once and never re-reported." << std::endl;
}

// Bug 3: resolveTurn ignored faction personalities.
void testPersonalitiesDriveResolution() {
    std::cout << "Testing: faction personalities drive AI resolution (bug 3)..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setHuman(ParticipantId::major(9));
    DIPLOMACY_ASSERT(ctx.personality(ParticipantId::major(2)) == MajorPersonality::Klingon &&
                     ctx.personality(ParticipantId::major(5)) == MajorPersonality::Ferengi &&
                     personalityForEmpire(3) == MajorPersonality::Romulan &&
                     personalityForEmpire(4) == MajorPersonality::Cardassian &&
                     personalityForEmpire(1) == MajorPersonality::Federation, "Default empire personalities");
    for (int id = 2; id <= 5; ++id) ctx.setTrust(ParticipantId::major(1), ParticipantId::major(id), 40);

    // The same trade offer with a 300-credit sweetener: the Ferengi take it, the Klingons do not.
    DiplomacyState state;
    uint64_t toFerengi = 0, toKlingon = 0;
    DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(1), ParticipantId::major(5), TreatyType::Trade,
        TreatyDuration::Turns25, DiplomaticTone::Neutral, 300, 0, &toFerengi);
    DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(1), ParticipantId::major(2), TreatyType::Trade,
        TreatyDuration::Turns25, DiplomaticTone::Neutral, 300, 0, &toKlingon);
    const auto delta = DiplomacyEngine::resolveTurn(state, ctx);
    auto contains = [](const std::vector<uint64_t>& v, uint64_t id) { return std::find(v.begin(), v.end(), id) != v.end(); };
    DIPLOMACY_ASSERT(contains(delta.acceptedProposals, toFerengi), "Ferengi must accept a profitable trade pact");
    DIPLOMACY_ASSERT(!contains(delta.acceptedProposals, toKlingon), "Klingons must not be bought");

    // Personality overrides from the context are honoured.
    ctx.setPersonality(ParticipantId::major(2), MajorPersonality::Ferengi);
    DiplomacyState again;
    uint64_t retry = 0;
    DiplomacyEngine::submitProposal(again, ctx, ParticipantId::major(1), ParticipantId::major(2), TreatyType::Trade,
        TreatyDuration::Turns25, DiplomaticTone::Neutral, 300, 0, &retry);
    const auto d2 = DiplomacyEngine::resolveTurn(again, ctx);
    DIPLOMACY_ASSERT(contains(d2.acceptedProposals, retry), "Context personality must be used by resolveTurn");

    // Romulan suspicion: an alliance at trust 80 is still refused; the Federation accepts.
    Proposal alliance;
    alliance.id = 77;
    alliance.proposer = ParticipantId::major(4);
    alliance.treatyType = TreatyType::Alliance;
    alliance.tone = DiplomaticTone::Cordial;
    auto trusting = fiveEmpireContext();
    trusting.setTrust(ParticipantId::major(4), ParticipantId::major(3), 70);
    trusting.setTrust(ParticipantId::major(4), ParticipantId::major(1), 70);
    alliance.target = ParticipantId::major(3);
    DIPLOMACY_ASSERT(evaluateAiProposal(state, trusting, alliance, 5, MajorPersonality::Romulan).decision != AiDecision::Accept,
                     "Romulans must refuse an alliance at trust 70");
    alliance.target = ParticipantId::major(1);
    DIPLOMACY_ASSERT(evaluateAiProposal(state, trusting, alliance, 5, MajorPersonality::Federation).decision == AiDecision::Accept,
                     "The Federation must accept an alliance at trust 70");

    // Integer power comparison: no overflow or floating point at the extremes.
    SimpleDiplomacyContext extreme = fiveEmpireContext();
    extreme.setPower(ParticipantId::major(1), INT64_MAX, 0);
    extreme.setPower(ParticipantId::major(2), 1, 0);
    Proposal nap;
    nap.id = 3;
    nap.proposer = ParticipantId::major(1);
    nap.target = ParticipantId::major(2);
    nap.treatyType = TreatyType::NonAggression;
    const auto r = evaluateAiProposal(state, extreme, nap, 9, MajorPersonality::Klingon);
    DIPLOMACY_ASSERT(r.explanation.find("superior fleet") != std::string::npos, "Overwhelming power must register without overflow");
    std::cout << "  PASS: Ferengi profit, Klingon honor, Romulan suspicion, Federation cooperation." << std::endl;
}

// Bug 4: war required the target's acceptance.
void testWarIsUnilateral() {
    std::cout << "Testing: war declarations are unilateral (bug 4)..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setHuman(ParticipantId::major(1));
    ctx.setTrust(ParticipantId::major(3), ParticipantId::major(2), 100);

    DiplomacyState state;
    uint64_t onHuman = 0, onAi = 0;
    DIPLOMACY_ASSERT(DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(2), ParticipantId::major(1), TreatyType::War,
        TreatyDuration::Indefinite, DiplomaticTone::Hostile, 0, 0, &onHuman, nullptr, 5), "War declaration on the human");
    DIPLOMACY_ASSERT(DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(2), ParticipantId::major(3), TreatyType::War,
        TreatyDuration::Indefinite, DiplomaticTone::Hostile, 0, 0, &onAi), "War declaration on an AI");
    DIPLOMACY_ASSERT(!DiplomacyEngine::respondToOffer(state, ParticipantId::major(1), onHuman, false),
                     "A declaration of war cannot be answered");
    DIPLOMACY_ASSERT(!DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(4), ParticipantId::major(5), TreatyType::War,
        TreatyDuration::Indefinite, DiplomaticTone::Hostile, 0, 50), "War cannot carry credit terms");

    const auto delta = DiplomacyEngine::resolveTurn(state, ctx);
    int wars = 0;
    for (const auto& op : delta.operations) {
        if (op.opType == DiplomaticOpType::SetWarState && op.setAtWar) ++wars;
    }
    DIPLOMACY_ASSERT(wars == 2, "Both declarations take effect this turn without consent");
    DIPLOMACY_ASSERT(state.findProposal(onHuman)->status == ProposalStatus::Accepted, "Human target did not need to respond");
    std::cout << "  PASS: war takes effect at resolution whatever the target says." << std::endl;
}

// Bug 5: eliminated empires kept treaties and could negotiate.
void testEliminatedParticipantsDissolve() {
    std::cout << "Testing: eliminated participants are removed from diplomacy (bug 5)..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setHuman(ParticipantId::major(1));
    DiplomacyState state;
    TurnResolutionDelta setup;
    DIPLOMACY_ASSERT(DiplomacyEngine::establishTreaty(state, ParticipantId::major(1), ParticipantId::major(3),
        TreatyType::Friendship, TreatyDuration::Indefinite, setup), "Setup treaty");
    DIPLOMACY_ASSERT(DiplomacyEngine::establishTreaty(state, ParticipantId::major(2), ParticipantId::major(4),
        TreatyType::NonAggression, TreatyDuration::Turns50, setup), "Unrelated treaty");
    uint64_t pending = 0;
    DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(3), ParticipantId::major(1), TreatyType::Trade,
        TreatyDuration::Turns25, DiplomaticTone::Cordial, 0, 0, &pending, nullptr, 4);

    ctx.setEliminated(ParticipantId::major(3), true);
    std::string err;
    DIPLOMACY_ASSERT(!DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(1), ParticipantId::major(3),
        TreatyType::NonAggression, TreatyDuration::Turns25, DiplomaticTone::Cordial, 0, 0, nullptr, &err),
        "Proposals to an eliminated empire must be refused");
    const auto delta = DiplomacyEngine::resolveTurn(state, ctx);
    DIPLOMACY_ASSERT(state.activeTreaties.size() == 1 && state.activeTreaties[0].involvesBoth(ParticipantId::major(2), ParticipantId::major(4)),
                     "Only the unrelated treaty survives");
    bool dissolved = false;
    for (const auto& t : state.treatyHistory) dissolved |= t.status == TreatyStatus::Dissolved && t.involves(ParticipantId::major(3));
    DIPLOMACY_ASSERT(dissolved, "The eliminated empire's treaty is archived as Dissolved");
    DIPLOMACY_ASSERT(std::find(delta.rejectedProposals.begin(), delta.rejectedProposals.end(), pending) != delta.rejectedProposals.end(),
                     "Pending proposal from the eliminated empire is cancelled");
    bool terminated = false, eliminatedEvent = false;
    for (const auto& op : delta.operations) terminated |= op.opType == DiplomaticOpType::TerminateTreaty && op.treatyType == TreatyType::Friendship;
    for (const auto& e : delta.events) eliminatedEvent |= e.type == DiplomaticEventType::ParticipantEliminated;
    DIPLOMACY_ASSERT(terminated && eliminatedEvent, "Campaign receives termination ops and an elimination event");
    const auto quiet = DiplomacyEngine::resolveTurn(state, ctx);
    for (const auto& e : quiet.events) DIPLOMACY_ASSERT(e.type != DiplomaticEventType::ParticipantEliminated, "Elimination re-reported");
    std::cout << "  PASS: treaties dissolved, proposals cancelled, negotiation refused." << std::endl;
}

// Bug 6: treaty cap dropped the treaty but emitted its effects; non-active rows vanished; overflow.
void testTreatyCapArchiveAndOverflow() {
    std::cout << "Testing: treaty cap, archiving and credit overflow (bug 6)..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setHuman(ParticipantId::major(9));
    DiplomacyState state;
    for (size_t i = 0; i < DiplomacyState::MaxActiveTreaties; ++i) {
        ActiveTreaty t;
        t.id = state.nextTreatyId++;
        t.partyA = ParticipantId::major(3);
        t.partyB = ParticipantId::minor(static_cast<int>(100 + i));
        t.type = TreatyType::Trade;
        t.status = TreatyStatus::Active;
        state.activeTreaties.push_back(t);
    }
    uint64_t id = 0;
    DiplomacyEngine::submitProposal(state, ctx, ParticipantId::major(1), ParticipantId::major(2), TreatyType::Trade,
        TreatyDuration::Turns25, DiplomaticTone::Cordial, 200, 0, &id);
    DiplomacyEngine::respondToOffer(state, ParticipantId::major(2), id, true);
    const auto delta = DiplomacyEngine::resolveTurn(state, ctx);
    DIPLOMACY_ASSERT(state.findProposal(id)->status == ProposalStatus::Rejected &&
                     state.findProposal(id)->rejectionReason.find("capacity") != std::string::npos, "Cap must reject the proposal");
    for (const auto& op : delta.operations) {
        DIPLOMACY_ASSERT(op.opType != DiplomaticOpType::ApplyTreaty && op.opType != DiplomaticOpType::TransferCredits,
                         "No effects may be emitted for a treaty that was not created");
    }
    TurnResolutionDelta direct;
    std::string err;
    DIPLOMACY_ASSERT(!DiplomacyEngine::establishTreaty(state, ParticipantId::major(1), ParticipantId::major(2), TreatyType::Peace,
        TreatyDuration::Turns25, direct, &err) && direct.operations.empty(), "establishTreaty enforces the cap too");

    // Non-active rows (from an old save) are archived rather than silently dropped.
    DiplomacyState legacy;
    ActiveTreaty stale;
    stale.id = legacy.nextTreatyId++;
    stale.partyA = ParticipantId::major(1);
    stale.partyB = ParticipantId::major(2);
    stale.type = TreatyType::Friendship;
    stale.status = TreatyStatus::Superseded;
    legacy.activeTreaties.push_back(stale);
    DiplomacyEngine::resolveTurn(legacy, ctx);
    DIPLOMACY_ASSERT(legacy.activeTreaties.empty() && legacy.treatyHistory.size() == 1 && legacy.treatyHistory[0] == stale,
                     "Stale row archived");

    // Gift attitude gain cannot overflow for any credit amount (was creditOffer * 4 into int).
    DiplomacyState minorState;
    MinorDiplomacyState m;
    m.minorId = 50;
    m.systemId = 50;
    m.raceName = "Bynars";
    m.attitudes[1] = 10;
    m.contacts[1] = true;
    minorState.minorStates.push_back(m);
    SimpleDiplomacyContext rich = fiveEmpireContext();
    rich.setMinorOwner(50, 0);
    rich.setContact(ParticipantId::major(1), ParticipantId::minor(50), true);
    rich.setBalance(ParticipantId::major(1), 1000000000000000LL);
    uint64_t gift = 0;
    DIPLOMACY_ASSERT(DiplomacyEngine::submitProposal(minorState, rich, ParticipantId::major(1), ParticipantId::minor(50),
        TreatyType::None, TreatyDuration::Indefinite, DiplomaticTone::Cordial, 1000000000000000LL, 0, &gift), "Huge gift");
    DiplomacyEngine::resolveTurn(minorState, rich);
    DIPLOMACY_ASSERT(minorState.getMinorState(50)->getAttitude(1) == 100, "Huge gift saturates attitude at 100");
    std::cout << "  PASS: cap rejects before effects, stale rows archived, no overflow." << std::endl;
}

void testBetrayalAndCallsToArms() {
    std::cout << "Testing: treaty betrayal penalties and alliance calls to arms..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setHuman(ParticipantId::major(9));
    DiplomacyState state;
    TurnResolutionDelta setup;
    DiplomacyEngine::establishTreaty(state, ParticipantId::major(1), ParticipantId::major(2), TreatyType::NonAggression, TreatyDuration::Turns50, setup);
    DiplomacyEngine::establishTreaty(state, ParticipantId::major(2), ParticipantId::major(3), TreatyType::Alliance, TreatyDuration::Indefinite, setup);
    DiplomacyEngine::establishTreaty(state, ParticipantId::major(2), ParticipantId::major(5), TreatyType::Alliance, TreatyDuration::Indefinite, setup);

    TurnResolutionDelta delta;
    const auto result = DiplomacyEngine::declareWar(state, ParticipantId::major(1), ParticipantId::major(2), delta, &ctx);
    ctx.setWar(ParticipantId::major(1), ParticipantId::major(2), true);
    DIPLOMACY_ASSERT(result.brokeTreaty && result.strongestBroken == TreatyType::NonAggression, "Breaking a NAP is a betrayal");
    DIPLOMACY_ASSERT(state.betrayals(1) == 1 && state.grievances(2, 1) == 3, "Standing and grievance recorded");
    int galaxyPenalties = 0, victimPenalty = 0;
    for (const auto& op : delta.operations) {
        if (op.opType != DiplomaticOpType::AdjustTrust) continue;
        if (op.partyB == ParticipantId::major(2)) victimPenalty += op.attitudeDelta;
        else if (op.attitudeDelta == -10) ++galaxyPenalties;
    }
    DIPLOMACY_ASSERT(victimPenalty == -15 && galaxyPenalties == 3, "Victim -15 and every other contacted empire -10");
    DIPLOMACY_ASSERT(result.alliesCalled == std::vector<int>({3, 5}) && state.defenseCalls.size() == 2, "Both allies called to arms");

    // Empire 3 joins; empire 5 stays out past the deadline.
    ctx.setWar(ParticipantId::major(3), ParticipantId::major(1), true);
    auto d1 = DiplomacyEngine::resolveTurn(state, ctx);
    bool honored = false;
    for (const auto& e : d1.events) honored |= e.type == DiplomaticEventType::AllianceHonored && e.partyA == ParticipantId::major(3);
    DIPLOMACY_ASSERT(honored && state.defenseCalls.size() == 1, "Joining the war honors the call immediately");
    ctx.setTrust(ParticipantId::major(2), ParticipantId::major(5), 50);
    bool refused = false, dissolvedOp = false;
    int trustDelta = 0, resolutions = 0;
    while (!refused && resolutions < 6) {
        const auto d = DiplomacyEngine::resolveTurn(state, ctx);
        ++resolutions;
        for (const auto& e : d.events) refused |= e.type == DiplomaticEventType::AllianceRefused && e.partyA == ParticipantId::major(5);
        for (const auto& op : d.operations) {
            if (op.opType == DiplomaticOpType::AdjustTrust && op.partyA == ParticipantId::major(5)) trustDelta += op.attitudeDelta;
            dissolvedOp |= op.opType == DiplomaticOpType::TerminateTreaty && op.treatyType == TreatyType::Alliance;
        }
    }
    DIPLOMACY_ASSERT(resolutions == DiplomacyState::DefenseCallWindow, "Call falls due after the window, not before");
    DIPLOMACY_ASSERT(refused && trustDelta == -20 && state.defenseCalls.empty(), "Refusal costs trust once the call falls due");
    DIPLOMACY_ASSERT(dissolvedOp && !state.hasActiveTreaty(ParticipantId::major(2), ParticipantId::major(5), TreatyType::Alliance),
                     "An AI defender cancels the alliance with an ally it no longer trusts");

    // Peace between the belligerents lapses open calls.
    DiplomacyState peaceState;
    TurnResolutionDelta s2;
    DiplomacyEngine::establishTreaty(peaceState, ParticipantId::major(2), ParticipantId::major(3), TreatyType::Alliance, TreatyDuration::Indefinite, s2);
    const auto peaceCtx = fiveEmpireContext();
    DiplomacyEngine::declareWar(peaceState, ParticipantId::major(1), ParticipantId::major(2), s2, &peaceCtx);
    DIPLOMACY_ASSERT(peaceState.defenseCalls.size() == 1, "Call issued");
    DiplomacyEngine::recordPeace(peaceState, ParticipantId::major(1), ParticipantId::major(2), s2);
    DIPLOMACY_ASSERT(peaceState.defenseCalls.empty() && peaceState.findMemory(1, 2)->lastPeaceTurn == peaceState.currentTurn,
                     "Peace lapses the call and is remembered");
    std::cout << "  PASS: betrayal penalties, honored/refused calls, alliance cancellation." << std::endl;
}

void testWarAssessmentRules() {
    std::cout << "Testing: AI war assessment respects treaties, cool-downs and strength..." << std::endl;
    WarAssessmentInput in;
    in.aggressorId = 2;
    in.targetId = 1;
    in.personality = MajorPersonality::Klingon;
    in.currentTurn = 60;
    in.turnsSinceTransition = 60;
    in.aggressorPower = 100;
    in.targetPower = 100;
    in.trust = 20;
    in.seed = 42;
    DIPLOMACY_ASSERT(assessWarDeclaration(in).declare, "A distrustful Klingon goes to war");
    auto early = in; early.currentTurn = 20;
    DIPLOMACY_ASSERT(!assessWarDeclaration(early).declare, "No AI wars before turn 25");
    auto cooled = in; cooled.turnsSinceTransition = 5;
    DIPLOMACY_ASSERT(!assessWarDeclaration(cooled).declare, "Cool-down after peace");
    auto bound = in; bound.treatyRank = 1; bound.aggressorPower = 10000; bound.trust = 0;
    DIPLOMACY_ASSERT(!assessWarDeclaration(bound).declare, "Klingons never break a treaty");
    auto peaceBound = in; peaceBound.peaceTreaty = true;
    DIPLOMACY_ASSERT(!assessWarDeclaration(peaceBound).declare, "A timed peace binds");
    auto romulan = bound; romulan.personality = MajorPersonality::Romulan;
    const auto treachery = assessWarDeclaration(romulan);
    DIPLOMACY_ASSERT(treachery.declare && treachery.breaksTreaty, "Romulans break pacts when overwhelming and distrustful");
    auto weak = in; weak.aggressorPower = 30;
    DIPLOMACY_ASSERT(!assessWarDeclaration(weak).declare, "Hopelessly outmatched empires do not declare");
    auto federation = in; federation.personality = MajorPersonality::Federation; federation.trust = 0;
    DIPLOMACY_ASSERT(!assessWarDeclaration(federation).declare, "The Federation does not start wars without grievances");
    auto busy = in; busy.activeWars = 2;
    DIPLOMACY_ASSERT(!assessWarDeclaration(busy).declare, "No new fronts beyond the personality limit");
    auto friendly = in; friendly.trust = 60;
    DIPLOMACY_ASSERT(!assessWarDeclaration(friendly).declare, "Trusted neighbours are left alone");
    DIPLOMACY_ASSERT(assessWarDeclaration(in).score == assessWarDeclaration(in).score, "Deterministic");

    DIPLOMACY_ASSERT(aiHonorsDefenseCall(MajorPersonality::Klingon, 50, 100, false), "Klingon honour");
    DIPLOMACY_ASSERT(!aiHonorsDefenseCall(MajorPersonality::Ferengi, 150, 100, false), "Ferengi need overwhelming odds");
    DIPLOMACY_ASSERT(!aiHonorsDefenseCall(MajorPersonality::Federation, 500, 100, true), "Treaty with the aggressor binds");
    std::cout << "  PASS: war assessment and alliance obligations." << std::endl;
}

void testAiDiplomacyPlanner() {
    std::cout << "Testing: AI-initiated proposals by personality and situation..." << std::endl;
    auto ctx = fiveEmpireContext();
    ctx.setHuman(ParticipantId::major(1));
    for (int a = 1; a <= 5; ++a)
        for (int b = a + 1; b <= 5; ++b) ctx.setTrust(ParticipantId::major(a), ParticipantId::major(b), 30);
    ctx.setTrust(ParticipantId::major(5), ParticipantId::major(1), 60);   // Ferengi like the Federation...
    for (int other : {2, 3, 4}) ctx.setTrust(ParticipantId::major(5), ParticipantId::major(other), 10); // ...and nobody else
    ctx.setTrust(ParticipantId::major(3), ParticipantId::major(4), 75);   // Romulan-Cardassian understanding
    DiplomacyState state;
    state.currentTurn = 30;

    auto intents = planAiDiplomacy(state, ctx, {2, 3, 4, 5}, 7);
    auto find = [&](int from, int to) -> const AiDiplomaticIntent* {
        for (const auto& i : intents) if (i.proposer.id == from && i.target.id == to) return &i;
        return nullptr;
    };
    DIPLOMACY_ASSERT(find(5, 1) && find(5, 1)->treatyType == TreatyType::Trade, "Ferengi open with trade");
    DIPLOMACY_ASSERT(find(3, 4) && find(3, 4)->treatyType == TreatyType::NonAggression, "Trusting neighbours propose a NAP");
    DIPLOMACY_ASSERT(!find(2, 1), "Klingons do not court a neighbour they distrust");
    for (const auto& i : intents) DIPLOMACY_ASSERT(i.proposer.id != 1, "The human is never planned for");

    // Weak neighbours are extorted by demanding personalities.
    auto bully = ctx;
    bully.setPower(ParticipantId::major(2), 1000, 100);
    const auto demands = planAiDiplomacy(state, bully, {2}, 7);
    bool tribute = false;
    for (const auto& i : demands) tribute |= i.treatyType == TreatyType::None && i.creditDemand > 0 && i.tone == DiplomaticTone::Demanding;
    DIPLOMACY_ASSERT(tribute, "A much stronger Klingon demands tribute");

    // Long wars end: the losing side sues for peace.
    auto war = ctx;
    war.setWar(ParticipantId::major(2), ParticipantId::major(3), true);
    war.setWarDuration(ParticipantId::major(2), ParticipantId::major(3), 30);
    war.setPower(ParticipantId::major(3), 20, 100);
    const auto peace = planAiDiplomacy(state, war, {3}, 7);
    bool suesForPeace = false;
    for (const auto& i : peace) suesForPeace |= i.target.id == 2 && i.treatyType == TreatyType::Peace;
    DIPLOMACY_ASSERT(suesForPeace, "The losing side proposes peace");

    // Cool-down and refusal back-off.
    for (const auto& i : intents) {
        DIPLOMACY_ASSERT(DiplomacyEngine::submitProposal(state, ctx, i.proposer, i.target, i.treatyType, i.duration, i.tone,
            i.creditOffer, i.creditDemand, nullptr, nullptr, ctx.isHuman(i.target) ? 2 : 1), "Planned intents are valid");
    }
    state.currentTurn += 1;
    DIPLOMACY_ASSERT(planAiDiplomacy(state, ctx, {5}, 7).empty(), "Open proposals and cool-downs prevent spam");
    state.noteProposalOutcome(5, 1, false);
    state.noteProposalOutcome(5, 1, false);
    state.proposals.clear();
    state.currentTurn = 30 + AiHumanProposalCooldownTurns;
    for (const auto& i : planAiDiplomacy(state, ctx, {5}, 7)) DIPLOMACY_ASSERT(i.target.id != 1, "Back-off after refusals");
    state.currentTurn = 30 + 4 * AiHumanProposalCooldownTurns;
    DIPLOMACY_ASSERT(!planAiDiplomacy(state, ctx, {5}, 7).empty(), "Back-off is bounded");

    // Eliminated empires are ignored.
    auto gone = ctx;
    gone.setEliminated(ParticipantId::major(1), true);
    DiplomacyState fresh;
    fresh.currentTurn = 30;
    for (const auto& i : planAiDiplomacy(fresh, gone, {2, 3, 4, 5}, 7)) DIPLOMACY_ASSERT(i.target.id != 1, "No proposals to the eliminated");
    DIPLOMACY_ASSERT(planAiDiplomacy(fresh, ctx, {2, 3, 4, 5}, 7) == planAiDiplomacy(fresh, ctx, {2, 3, 4, 5}, 7), "Deterministic");
    std::cout << "  PASS: trade, pacts, tribute, peace, cool-downs and back-off." << std::endl;
}

void testDiplomaticVictoryPredicate() {
    std::cout << "Testing: diplomatic victory predicate..." << std::endl;
    DiplomacyState state;
    TurnResolutionDelta d;
    std::map<int, int64_t> pop{{1, 300}, {2, 200}, {3, 250}, {4, 100}};
    std::map<int, int64_t> minors{{40, 30}};
    auto alone = evaluateDiplomaticVictory(state, 1, pop, minors);
    DIPLOMACY_ASSERT(!alone.achieved && alone.galaxyPopulation == 880 && alone.blocPopulation == 300, "No bloc, no victory");
    DiplomacyEngine::establishTreaty(state, ParticipantId::major(1), ParticipantId::major(2), TreatyType::Alliance, TreatyDuration::Indefinite, d);
    auto two = evaluateDiplomaticVictory(state, 1, pop, minors);
    DIPLOMACY_ASSERT(two.achieved && two.blocPopulation == 500 && two.allies == std::vector<int>({2}), "500 of 880 is a majority");
    DIPLOMACY_ASSERT(!evaluateDiplomaticVictory(state, 3, pop, minors).achieved, "Rivals do not share the win");
    DiplomacyState affiliated;
    DiplomacyEngine::establishTreaty(affiliated, ParticipantId::major(1), ParticipantId::major(2), TreatyType::Affiliation, TreatyDuration::Indefinite, d);
    DIPLOMACY_ASSERT(!evaluateDiplomaticVictory(affiliated, 1, pop, minors).achieved, "Only full alliances count");
    auto withoutTwo = pop;
    withoutTwo.erase(2); // Eliminated allies do not count
    DIPLOMACY_ASSERT(!evaluateDiplomaticVictory(state, 1, withoutTwo, minors).achieved, "Eliminated allies excluded");
    DiplomacyState members;
    MinorDiplomacyState m;
    m.minorId = 41; m.systemId = 41; m.raceName = "Members"; m.membershipOwner = 4;
    members.minorStates.push_back(m);
    std::map<int, int64_t> bigFour{{1, 100}, {4, 400}};
    auto memberWin = evaluateDiplomaticVictory(members, 4, bigFour, {});
    DIPLOMACY_ASSERT(memberWin.achieved && memberWin.memberMinors == std::vector<int>({41}), "Member minors form a bloc");
    std::cout << "  PASS: alliance and membership majorities." << std::endl;
}

void testMemorySectionsRoundtrip() {
    std::cout << "Testing: relation memory, standings and calls to arms persist..." << std::endl;
    DiplomacyState state;
    state.currentTurn = 40;
    state.nextProposalId = 3;
    state.nextTreatyId = 2;
    const std::string plain = serializeDiplomacyState(state);
    DIPLOMACY_ASSERT(plain.find("relationMemory") == std::string::npos, "Empty sections are omitted (canonical old saves)");
    auto* mem = state.memory(3, 1);
    mem->lastWarTurn = 12; mem->lastPeaceTurn = 30; mem->lastProposalFirstToSecond = 39;
    state.addGrievance(3, 1, 4);
    state.noteProposalOutcome(1, 3, false);
    state.standing(2)->betrayals = 2;
    state.standing(2)->lastBetrayalTurn = 35;
    state.defenseCalls.push_back({4, 1, 2, 38, 41});
    const std::string text = serializeDiplomacyState(state);
    DiplomacyState loaded;
    std::string err;
    DIPLOMACY_ASSERT(deserializeDiplomacyState(text, loaded, &err), "Roundtrip: " << err);
    DIPLOMACY_ASSERT(loaded == state && serializeDiplomacyState(loaded) == text, "Byte-exact roundtrip");
    DIPLOMACY_ASSERT(loaded.findMemory(1, 3)->first == 1 && loaded.grievances(3, 1) == 4, "Unordered pair lookups");

    auto corrupt = [&](const std::string& from, const std::string& to) {
        std::string bad = text;
        const auto pos = bad.find(from);
        DIPLOMACY_ASSERT(pos != std::string::npos, "Fixture token missing: " << from);
        bad.replace(pos, from.size(), to);
        DiplomacyState out;
        return !deserializeDiplomacyState(bad, out, &err);
    };
    DIPLOMACY_ASSERT(corrupt("rm 1 3", "rm 3 1"), "Unordered pair rejected");
    DIPLOMACY_ASSERT(corrupt("st 2 2 35", "st 2 -1 35"), "Negative betrayals rejected");
    DIPLOMACY_ASSERT(corrupt("st 2 2 35", "st 2 2 99"), "Future betrayal turn rejected");
    DIPLOMACY_ASSERT(corrupt("dc 4 1 2", "dc 4 4 2"), "Self-referential call rejected");
    DIPLOMACY_ASSERT(corrupt("defenseCalls 1", "defenseCalls 0"), "Empty declared section rejected");
    DIPLOMACY_ASSERT(corrupt("END_BOTF_DIPLOMACY", "standings 1\nst 3 0 -1\nEND_BOTF_DIPLOMACY"), "Out-of-order section rejected");
    std::cout << "  PASS: optional sections roundtrip exactly and reject corruption." << std::endl;
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
        testProposalTableNeverLocksUp();
        testDeclareWarResolvesVoidedProposals();
        testPersonalitiesDriveResolution();
        testWarIsUnilateral();
        testEliminatedParticipantsDissolve();
        testTreatyCapArchiveAndOverflow();
        testBetrayalAndCallsToArms();
        testWarAssessmentRules();
        testAiDiplomacyPlanner();
        testDiplomaticVictoryPredicate();
        testMemorySectionsRoundtrip();

        std::cout << "\n>>> ALL 25 DIPLOMACY TEST SUITES PASSED CLEANLY! <<<" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAILED with exception: " << ex.what() << std::endl;
        return 1;
    }
}
