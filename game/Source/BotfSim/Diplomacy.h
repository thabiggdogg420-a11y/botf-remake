#pragma once

// =============================================================================
// BOTF Diplomacy Subsystem (Modern Reconstruction / Beta Implementation)
// =============================================================================
//
// Documentation & Fidelity References:
// - research/BOTF-RESEARCH.md (§ DIP-01 Diplomatic state & pending actions,
//                              § DIP-02 Minor membership policy,
//                              § S19 Minor gifts, § S20 AI diplomacy)
// - research/BETA-FIDELITY.md
// - setup/output/claude-beta-ui-contract.txt (§ D Diplomacy D1–D5)
// - Original BOTF Tutorial (1999), pp. 11–15, 21–26 (modes, treaties, tones,
//   durations 25/50/100/indefinite, credit payments/demands, accept/reject)
// - game/Build/DiplomacyReview/RemainingFindings.txt (Review corrections)
//
// Mechanistic Classification:
// - VERIFIED: Treaty types (NonAggression, Friendship, Affiliation, Alliance,
//   Membership, Peace, Trade, War) and durations (25, 50, 100, Indefinite).
// - VERIFIED: Atomic proposal lifecycle (Pending -> Accepted / Rejected / Ignored / Expired).
// - VERIFIED: Response authorization gates acceptance to proposal.target strictly.
// - VERIFIED: War breaks all incompatible treaties; expired treaties move to bounded history.
// - VERIFIED: Minor membership lifecycle gated by contact, peace, prior-turn CURRENT friendship,
//   and attitude threshold (>= 80). Independent minors only; no major annexation.
// - VERIFIED: Concurrent eligible membership offers arbitrated by current attitude,
//   with deterministic seeded RNG tie-break. Exactly one ownership transfer emitted.
// - VERIFIED: Credit payment attached to proposal charged only upon successful acceptance.
// - MODERN RECONSTRUCTION / PROVISIONAL: Numeric AI scoring weights, tone modifiers,
//   and gift attitude scaling (4% per 1,000 credits baseline per S19).
//
// Architectural Invariants:
// - Standalone header; completely independent of Simulation.h (no circular dependencies).
// - Root/campaign can embed State; resolution produces immutable operations/deltas.
// - Working resolution ledger prevents multi-offer overdraws and guarantees atomic settlement.
// - Working state updates intra-turn so concurrent membership, treaty upgrades, or war changes
//   revalidate and invalidate subsequent incompatible offers immediately within the same step.
// - SetWarState carries explicit typed boolean `setAtWar`.
// =============================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace botf::diplomacy {

// -----------------------------------------------------------------------------
// Enums & String Conversions
// -----------------------------------------------------------------------------

enum class TreatyType : int {
    None = 0,
    NonAggression = 1,
    Friendship = 2,
    Affiliation = 3,
    Alliance = 4,
    Membership = 5,
    Peace = 6,
    Trade = 7,
    War = 8
};

inline bool isValidTreatyType(int val) {
    return val >= 0 && val <= 8;
}

inline const char* treatyTypeName(TreatyType type) {
    switch (type) {
        case TreatyType::None: return "None";
        case TreatyType::NonAggression: return "Non-Aggression Pact";
        case TreatyType::Friendship: return "Friendship Treaty";
        case TreatyType::Affiliation: return "Affiliation Treaty";
        case TreatyType::Alliance: return "Full Alliance";
        case TreatyType::Membership: return "Membership";
        case TreatyType::Peace: return "Peace Treaty";
        case TreatyType::Trade: return "Trade Pact";
        case TreatyType::War: return "War";
        default: return "Unknown";
    }
}

inline int getTreatyRank(TreatyType type) {
    switch (type) {
        case TreatyType::NonAggression: return 1;
        case TreatyType::Friendship: return 2;
        case TreatyType::Affiliation: return 3;
        case TreatyType::Alliance: return 4;
        default: return 0;
    }
}

enum class TreatyDuration : int {
    Indefinite = -1,
    Turns25 = 25,
    Turns50 = 50,
    Turns100 = 100
};

inline bool isValidTreatyDuration(int val) {
    return val == -1 || val == 25 || val == 50 || val == 100;
}

inline const char* treatyDurationName(TreatyDuration duration) {
    switch (duration) {
        case TreatyDuration::Turns25: return "25 Turns";
        case TreatyDuration::Turns50: return "50 Turns";
        case TreatyDuration::Turns100: return "100 Turns";
        case TreatyDuration::Indefinite: return "Indefinite";
        default: return "Custom";
    }
}

enum class DiplomaticTone : int {
    Cordial = 0,
    Neutral = 1,
    Demanding = 2,
    Hostile = 3
};

inline bool isValidDiplomaticTone(int val) {
    return val >= 0 && val <= 3;
}

inline const char* toneName(DiplomaticTone tone) {
    switch (tone) {
        case DiplomaticTone::Cordial: return "Cordial";
        case DiplomaticTone::Neutral: return "Neutral";
        case DiplomaticTone::Demanding: return "Demanding";
        case DiplomaticTone::Hostile: return "Hostile";
        default: return "Unknown";
    }
}

enum class ProposalStatus : int {
    Pending = 0,
    Accepted = 1,
    Rejected = 2,
    Ignored = 3,
    Expired = 4,
    VoidedByWar = 5,
    Cancelled = 6
};

inline bool isValidProposalStatus(int val) {
    return val >= 0 && val <= 6;
}

inline const char* proposalStatusName(ProposalStatus status) {
    switch (status) {
        case ProposalStatus::Pending: return "Pending";
        case ProposalStatus::Accepted: return "Accepted";
        case ProposalStatus::Rejected: return "Rejected";
        case ProposalStatus::Ignored: return "Ignored";
        case ProposalStatus::Expired: return "Expired";
        case ProposalStatus::VoidedByWar: return "Voided by War";
        case ProposalStatus::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

enum class TreatyStatus : int {
    Active = 0,
    Expired = 1,
    BrokenByWar = 2,
    Superseded = 3
};

inline bool isValidTreatyStatus(int val) {
    return val >= 0 && val <= 3;
}

inline const char* treatyStatusName(TreatyStatus status) {
    switch (status) {
        case TreatyStatus::Active: return "Active";
        case TreatyStatus::Expired: return "Expired";
        case TreatyStatus::BrokenByWar: return "Broken by War";
        case TreatyStatus::Superseded: return "Superseded";
        default: return "Unknown";
    }
}

enum class AiDecision : int {
    Accept = 0,
    Reject = 1,
    Ignore = 2
};

inline const char* aiDecisionName(AiDecision decision) {
    switch (decision) {
        case AiDecision::Accept: return "Accept";
        case AiDecision::Reject: return "Reject";
        case AiDecision::Ignore: return "Ignore";
        default: return "Unknown";
    }
}

enum class DiplomaticOpType : int {
    None = 0,
    TransferCredits = 1,
    ApplyTreaty = 2,
    TerminateTreaty = 3,
    SetWarState = 4,
    TransferMinorMembership = 5,
    UpdateMinorAttitude = 6,
    RecordEvent = 7
};

enum class DiplomaticEventType : int {
    ProposalSubmitted = 0,
    ProposalAccepted = 1,
    ProposalRejected = 2,
    ProposalIgnored = 3,
    TreatyEstablished = 4,
    TreatyExpired = 5,
    TreatyBrokenByWar = 6,
    TreatySuperseded = 7,
    WarDeclared = 8,
    PeaceEstablished = 9,
    MinorMembershipJoined = 10,
    GiftDelivered = 11
};

inline bool isValidEventType(int val) {
    return val >= 0 && val <= 11;
}

inline const char* eventTypeName(DiplomaticEventType type) {
    switch (type) {
        case DiplomaticEventType::ProposalSubmitted: return "Proposal Submitted";
        case DiplomaticEventType::ProposalAccepted: return "Proposal Accepted";
        case DiplomaticEventType::ProposalRejected: return "Proposal Rejected";
        case DiplomaticEventType::ProposalIgnored: return "Proposal Ignored";
        case DiplomaticEventType::TreatyEstablished: return "Treaty Established";
        case DiplomaticEventType::TreatyExpired: return "Treaty Expired";
        case DiplomaticEventType::TreatyBrokenByWar: return "Treaty Broken by War";
        case DiplomaticEventType::TreatySuperseded: return "Treaty Superseded";
        case DiplomaticEventType::WarDeclared: return "War Declared";
        case DiplomaticEventType::PeaceEstablished: return "Peace Established";
        case DiplomaticEventType::MinorMembershipJoined: return "Minor Joined Empire";
        case DiplomaticEventType::GiftDelivered: return "Gift Delivered";
        default: return "Unknown";
    }
}

// -----------------------------------------------------------------------------
// Identity & Data Types
// -----------------------------------------------------------------------------

struct ParticipantId {
    int id = 0;
    bool isMinor = false;

    constexpr bool operator==(const ParticipantId& other) const = default;
    constexpr auto operator<=>(const ParticipantId& other) const = default;

    static constexpr ParticipantId major(int empireId) {
        return ParticipantId{empireId, false};
    }
    static constexpr ParticipantId minor(int raceOrSystemId) {
        return ParticipantId{raceOrSystemId, true};
    }
};

struct Proposal {
    uint64_t id = 0;
    ParticipantId proposer;
    ParticipantId target;
    TreatyType treatyType = TreatyType::None;
    TreatyDuration duration = TreatyDuration::Indefinite;
    DiplomaticTone tone = DiplomaticTone::Neutral;
    int64_t creditOffer = 0;   // Credits offered by proposer to target upon acceptance
    int64_t creditDemand = 0;  // Credits demanded by proposer from target upon acceptance
    int submittedTurn = 0;
    int dueTurn = 0;           // Turn at which proposal expires/resolves
    ProposalStatus status = ProposalStatus::Pending;
    bool resolved = false;     // True once proposal outcome has been finalized
    std::string rejectionReason;

    constexpr bool operator==(const Proposal& other) const = default;
};

struct ActiveTreaty {
    uint64_t id = 0;
    ParticipantId partyA;
    ParticipantId partyB;
    TreatyType type = TreatyType::None;
    TreatyDuration duration = TreatyDuration::Indefinite;
    int startTurn = 0;
    int turnsRemaining = -1; // -1 for Indefinite, or countdown value
    TreatyStatus status = TreatyStatus::Active;

    constexpr bool operator==(const ActiveTreaty& other) const = default;

    bool involves(ParticipantId p) const {
        return partyA == p || partyB == p;
    }
    bool involvesBoth(ParticipantId a, ParticipantId b) const {
        return (partyA == a && partyB == b) || (partyA == b && partyB == a);
    }
    ParticipantId getCounterparty(ParticipantId viewer) const {
        return (partyA == viewer) ? partyB : partyA;
    }
};

// Claude Beta UI Contract D4 representation
struct TreatyRecord {
    ParticipantId counterparty;
    TreatyType type = TreatyType::None;
    TreatyDuration duration = TreatyDuration::Indefinite;
    int turnsRemaining = -1;
    TreatyStatus status = TreatyStatus::Active;

    constexpr bool operator==(const TreatyRecord& other) const = default;
};

struct MinorDiplomacyState {
    int minorId = 0;
    int systemId = 0;
    std::string raceName;
    int membershipOwner = 0; // 0 if independent, or major empire ID (1..5)
    int membershipTurn = -1;
    std::map<int, int> attitudes;       // majorId -> attitude [0..100]
    std::map<int, bool> contacts;       // majorId -> contact discovered
    std::map<int, int> friendshipTurns; // majorId -> turn friendship signed (-1 if none/broken)

    int getAttitude(int majorId) const {
        auto it = attitudes.find(majorId);
        return (it != attitudes.end()) ? it->second : 50;
    }
    bool hasContact(int majorId) const {
        auto it = contacts.find(majorId);
        return (it != contacts.end()) ? it->second : false;
    }
    int getFriendshipTurn(int majorId) const {
        auto it = friendshipTurns.find(majorId);
        return (it != friendshipTurns.end()) ? it->second : -1;
    }

    bool operator==(const MinorDiplomacyState& other) const = default;
};

struct DiplomaticEvent {
    int turn = 0;
    DiplomaticEventType type = DiplomaticEventType::ProposalSubmitted;
    ParticipantId partyA;
    ParticipantId partyB;
    int64_t numericValue = 0;
    std::string description;

    bool operator==(const DiplomaticEvent& other) const = default;
};

// -----------------------------------------------------------------------------
// Operations / Deltas for Campaign Integration
// -----------------------------------------------------------------------------

struct DiplomacyOperation {
    DiplomaticOpType opType = DiplomaticOpType::None;
    ParticipantId partyA;
    ParticipantId partyB;
    TreatyType treatyType = TreatyType::None;
    TreatyDuration duration = TreatyDuration::Indefinite;
    int turnsRemaining = -1;
    int64_t credits = 0;        // Transferred from partyA to partyB
    int attitudeDelta = 0;
    int newOwner = 0;
    bool setAtWar = false;      // Explicit typed boolean for SetWarState (true = War, false = Peace)
    std::string notes;

    bool operator==(const DiplomacyOperation& other) const = default;
};

struct TurnResolutionDelta {
    std::vector<DiplomacyOperation> operations;
    std::vector<DiplomaticEvent> events;
    std::vector<uint64_t> acceptedProposals;
    std::vector<uint64_t> rejectedProposals;
    std::vector<uint64_t> ignoredProposals;
    std::vector<uint64_t> expiredTreaties;
    std::vector<uint64_t> brokenTreaties;
};

// -----------------------------------------------------------------------------
// Context Interface
// -----------------------------------------------------------------------------

class DiplomacyContext {
public:
    virtual ~DiplomacyContext() = default;

    virtual bool hasContact(ParticipantId a, ParticipantId b) const = 0;
    virtual bool isAtWar(ParticipantId a, ParticipantId b) const = 0;
    virtual int64_t creditBalance(ParticipantId p) const = 0;
    virtual int64_t militaryPower(ParticipantId p) const = 0;
    virtual int64_t economicPower(ParticipantId p) const = 0;
    virtual int trust(ParticipantId a, ParticipantId b) const = 0; // 0..100
    virtual bool isMinor(int id) const = 0;
    virtual bool isIndependentMinor(int minorId) const = 0;
    virtual int minorMembershipOwner(int minorId) const = 0;
    virtual bool isHuman(ParticipantId p) const {
        return p == ParticipantId::major(1);
    }
};

// Default in-memory context for standalone execution and tests
class SimpleDiplomacyContext : public DiplomacyContext {
public:
    ParticipantId humanPlayer = ParticipantId::major(1);
    std::map<std::pair<ParticipantId, ParticipantId>, bool> contacts;
    std::map<std::pair<ParticipantId, ParticipantId>, bool> wars;
    std::map<std::pair<ParticipantId, ParticipantId>, int> trusts;
    std::map<ParticipantId, int64_t> balances;
    std::map<ParticipantId, int64_t> militaryPowers;
    std::map<ParticipantId, int64_t> economicPowers;
    std::map<int, int> minorOwners; // minorId -> owner (0 if independent)

    void setContact(ParticipantId a, ParticipantId b, bool val) {
        contacts[{a, b}] = val;
        contacts[{b, a}] = val;
    }
    void setWar(ParticipantId a, ParticipantId b, bool val) {
        wars[{a, b}] = val;
        wars[{b, a}] = val;
    }
    void setTrust(ParticipantId a, ParticipantId b, int val) {
        trusts[{a, b}] = val;
        trusts[{b, a}] = val;
    }
    void setBalance(ParticipantId p, int64_t val) {
        balances[p] = val;
    }
    void setPower(ParticipantId p, int64_t mil, int64_t econ) {
        militaryPowers[p] = mil;
        economicPowers[p] = econ;
    }
    void setMinorOwner(int minorId, int ownerId) {
        minorOwners[minorId] = ownerId;
    }
    void setHuman(ParticipantId p) {
        humanPlayer = p;
    }

    bool hasContact(ParticipantId a, ParticipantId b) const override {
        auto it = contacts.find({a, b});
        return (it != contacts.end()) ? it->second : false;
    }
    bool isAtWar(ParticipantId a, ParticipantId b) const override {
        auto it = wars.find({a, b});
        return (it != wars.end()) ? it->second : false;
    }
    int64_t creditBalance(ParticipantId p) const override {
        auto it = balances.find(p);
        return (it != balances.end()) ? it->second : 0;
    }
    int64_t militaryPower(ParticipantId p) const override {
        auto it = militaryPowers.find(p);
        return (it != militaryPowers.end()) ? it->second : 100;
    }
    int64_t economicPower(ParticipantId p) const override {
        auto it = economicPowers.find(p);
        return (it != economicPowers.end()) ? it->second : 100;
    }
    int trust(ParticipantId a, ParticipantId b) const override {
        auto it = trusts.find({a, b});
        return (it != trusts.end()) ? it->second : 50;
    }
    bool isMinor(int id) const override {
        return minorOwners.find(id) != minorOwners.end();
    }
    bool isIndependentMinor(int minorId) const override {
        auto it = minorOwners.find(minorId);
        return (it != minorOwners.end()) ? (it->second == 0) : false;
    }
    int minorMembershipOwner(int minorId) const override {
        auto it = minorOwners.find(minorId);
        return (it != minorOwners.end()) ? it->second : 0;
    }
    bool isHuman(ParticipantId p) const override {
        return p == humanPlayer;
    }
};

// -----------------------------------------------------------------------------
// AI Personality & Decision Models
// -----------------------------------------------------------------------------

enum class MajorPersonality : int {
    Federation = 0, // Trusting, peaceful, respects treaties
    Klingon = 1,    // Militaristic, respects honor/power, detests weakness/demands
    Romulan = 2,    // Cautious, deceitful, high non-aggression threshold
    Cardassian = 3, // Aggressive, imperialist, demands tribute
    Ferengi = 4     // Mercantile, highly influenced by credit sweeteners
};

struct AiDecisionResult {
    AiDecision decision = AiDecision::Reject;
    int score = 0;
    int threshold = 50;
    std::string explanation;
};

// -----------------------------------------------------------------------------
// Diplomacy State
// -----------------------------------------------------------------------------

struct DiplomacyState {
    static constexpr size_t MaxPendingProposals = 128;
    static constexpr size_t MaxActiveTreaties = 128;
    static constexpr size_t MaxTreatyHistory = 256;
    static constexpr size_t MaxEventHistory = 256;
    static constexpr size_t MaxMinorStates = 64;

    int currentTurn = 1;
    uint64_t nextProposalId = 1;
    uint64_t nextTreatyId = 1;

    std::vector<Proposal> proposals;
    std::vector<ActiveTreaty> activeTreaties;
    std::vector<ActiveTreaty> treatyHistory;
    std::vector<MinorDiplomacyState> minorStates;
    std::vector<DiplomaticEvent> eventHistory;

    bool operator==(const DiplomacyState& other) const = default;

    const Proposal* findProposal(uint64_t id) const {
        for (const auto& p : proposals) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }

    Proposal* findProposal(uint64_t id) {
        for (auto& p : proposals) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }

    const ActiveTreaty* findActiveTreaty(ParticipantId a, ParticipantId b, TreatyType type) const {
        for (const auto& t : activeTreaties) {
            if (t.status == TreatyStatus::Active && t.type == type && t.involvesBoth(a, b)) {
                return &t;
            }
        }
        return nullptr;
    }

    const MinorDiplomacyState* getMinorState(int minorId) const {
        for (const auto& m : minorStates) {
            if (m.minorId == minorId) return &m;
        }
        return nullptr;
    }

    MinorDiplomacyState* getMinorState(int minorId) {
        for (auto& m : minorStates) {
            if (m.minorId == minorId) return &m;
        }
        return nullptr;
    }

    std::vector<TreatyRecord> getTreatiesFor(ParticipantId viewer) const {
        std::vector<TreatyRecord> result;
        for (const auto& t : activeTreaties) {
            if (t.status == TreatyStatus::Active && t.involves(viewer)) {
                TreatyRecord rec;
                rec.counterparty = t.getCounterparty(viewer);
                rec.type = t.type;
                rec.duration = t.duration;
                rec.turnsRemaining = t.turnsRemaining;
                rec.status = t.status;
                result.push_back(rec);
            }
        }
        return result;
    }
};

// -----------------------------------------------------------------------------
// Minor Attitude Labels (Claude Beta UI Contract D5)
// -----------------------------------------------------------------------------

inline const char* getAttitudeLabel(int attitude) {
    if (attitude >= 90) return "Enamored";
    if (attitude >= 75) return "Friendly";
    if (attitude >= 60) return "Receptive";
    if (attitude >= 40) return "Neutral";
    if (attitude >= 25) return "Suspicious";
    if (attitude >= 10) return "Hostile";
    return "Belligerent";
}

// -----------------------------------------------------------------------------
// Validation & Legality Matrix
// -----------------------------------------------------------------------------

inline bool validateProposalCreation(
    const DiplomacyState& state,
    const DiplomacyContext& ctx,
    const Proposal& p,
    std::string* outReason = nullptr)
{
    // 1. Proposer and Target must be distinct
    if (p.proposer == p.target) {
        if (outReason) *outReason = "Self-proposals are strictly illegal";
        return false;
    }

    // 2. Identity validation: IDs must be positive
    if (p.proposer.id <= 0 || p.target.id <= 0) {
        if (outReason) *outReason = "Invalid participant ID";
        return false;
    }

    // 3. Contact requirement
    if (!ctx.hasContact(p.proposer, p.target)) {
        if (outReason) *outReason = "Target is uncontacted";
        return false;
    }

    // 4. Bounded capacity check
    if (state.proposals.size() >= DiplomacyState::MaxPendingProposals) {
        if (outReason) *outReason = "Pending proposal queue limit reached";
        return false;
    }

    // 5. Duplicate pending proposal check
    for (const auto& existing : state.proposals) {
        if (existing.status == ProposalStatus::Pending &&
            existing.proposer == p.proposer &&
            existing.target == p.target &&
            existing.treatyType == p.treatyType)
        {
            if (outReason) *outReason = "Duplicate identical proposal already pending";
            return false;
        }
    }

    // 6. Non-negative credits
    if (p.creditOffer < 0 || p.creditDemand < 0) {
        if (outReason) *outReason = "Credit transfer amounts cannot be negative";
        return false;
    }

    // 7. Proposer affordability check at creation
    if (p.creditOffer > 0 && ctx.creditBalance(p.proposer) < p.creditOffer) {
        if (outReason) *outReason = "Proposer cannot afford the offered credit amount";
        return false;
    }

    // 8. Treaty Type Legality Matrix
    const bool currentlyAtWar = ctx.isAtWar(p.proposer, p.target);

    switch (p.treatyType) {
        case TreatyType::War: {
            if (currentlyAtWar) {
                if (outReason) *outReason = "Belligerents are already at war";
                return false;
            }
            break;
        }
        case TreatyType::Peace: {
            if (!currentlyAtWar) {
                if (outReason) *outReason = "Peace can only be negotiated between belligerents at war";
                return false;
            }
            break;
        }
        case TreatyType::Membership: {
            // Membership is ONLY for independent minors and a major empire
            if (p.proposer.isMinor) {
                if (outReason) *outReason = "Minors cannot propose membership to others";
                return false;
            }
            if (!p.target.isMinor) {
                if (outReason) *outReason = "Cannot annex or propose membership to a major empire";
                return false;
            }
            const auto* minor = state.getMinorState(p.target.id);
            if (!minor || minor->membershipOwner != 0 || !ctx.isIndependentMinor(p.target.id)) {
                if (outReason) *outReason = "Target minor is already a member of an empire or not independent";
                return false;
            }
            // Minor membership prerequisites (per DIP-02 in BOTF-RESEARCH):
            // - Established Friendship from a PRIOR turn
            int friendshipTurn = minor->getFriendshipTurn(p.proposer.id);
            if (friendshipTurn < 0 || friendshipTurn >= state.currentTurn) {
                if (outReason) *outReason = "Membership requires an established Friendship from a prior turn";
                return false;
            }
            // - Attitude must be at least 80
            if (minor->getAttitude(p.proposer.id) < 80) {
                if (outReason) *outReason = "Membership requires attitude of at least 80";
                return false;
            }
            break;
        }
        case TreatyType::NonAggression:
        case TreatyType::Friendship:
        case TreatyType::Affiliation:
        case TreatyType::Alliance: {
            if (currentlyAtWar) {
                if (outReason) *outReason = "Cannot negotiate non-war treaties while at war; negotiate peace first";
                return false;
            }
            if (p.target.isMinor && p.treatyType == TreatyType::Alliance) {
                if (outReason) *outReason = "Alliance is for major empires; minors advance to Membership";
                return false;
            }
            // Check if equal or higher treaty is already active
            int proposedRank = getTreatyRank(p.treatyType);
            for (const auto& t : state.activeTreaties) {
                if (t.status == TreatyStatus::Active && t.involvesBoth(p.proposer, p.target)) {
                    int activeRank = getTreatyRank(t.type);
                    if (activeRank >= proposedRank && activeRank > 0) {
                        if (outReason) *outReason = "An equal or stronger treaty is already active";
                        return false;
                    }
                }
            }
            break;
        }
        case TreatyType::Trade: {
            if (currentlyAtWar) {
                if (outReason) *outReason = "Cannot sign trade pacts while at war";
                return false;
            }
            for (const auto& t : state.activeTreaties) {
                if (t.status == TreatyStatus::Active && t.type == TreatyType::Trade && t.involvesBoth(p.proposer, p.target)) {
                    if (outReason) *outReason = "Trade pact is already in effect";
                    return false;
                }
            }
            break;
        }
        case TreatyType::None: {
            // Standalone gift / tribute
            break;
        }
        default: {
            if (outReason) *outReason = "Unknown treaty type";
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// AI Evaluation Function
// -----------------------------------------------------------------------------

inline AiDecisionResult evaluateAiProposal(
    const DiplomacyState& state,
    const DiplomacyContext& ctx,
    const Proposal& p,
    uint64_t seed,
    MajorPersonality personality = MajorPersonality::Federation)
{
    AiDecisionResult res;
    res.threshold = 50;

    int score = 50; // Neutral baseline
    std::ostringstream expl;

    // 1. Relationship & Trust
    if (p.target.isMinor) {
        const auto* minor = state.getMinorState(p.target.id);
        int attitude = minor ? minor->getAttitude(p.proposer.id) : 50;
        score = attitude;
        expl << "Minor attitude (" << attitude << ") ";
    } else {
        int tr = ctx.trust(p.target, p.proposer);
        score = tr;
        expl << "Base trust (" << tr << ") ";
    }

    // 2. Power Balance
    int64_t proposerMil = ctx.militaryPower(p.proposer);
    int64_t targetMil = ctx.militaryPower(p.target);
    if (targetMil > 0) {
        double milRatio = static_cast<double>(proposerMil) / static_cast<double>(targetMil);
        if (milRatio > 2.0) {
            score += 10;
            expl << "+ Proposer superior fleet (+10) ";
        } else if (milRatio < 0.5) {
            score -= 10;
            expl << "- Proposer inferior fleet (-10) ";
        }
    }

    // 3. Tone Modifiers
    switch (p.tone) {
        case DiplomaticTone::Cordial:
            score += 10;
            expl << "+ Cordial tone (+10) ";
            break;
        case DiplomaticTone::Neutral:
            expl << "+ Neutral tone (+0) ";
            break;
        case DiplomaticTone::Demanding:
            if (personality == MajorPersonality::Klingon || personality == MajorPersonality::Cardassian) {
                score -= 25;
                expl << "- Demanding tone resisted (-25) ";
            } else {
                score -= 15;
                expl << "- Demanding tone penalty (-15) ";
            }
            break;
        case DiplomaticTone::Hostile:
            score -= 30;
            expl << "- Hostile tone penalty (-30) ";
            break;
    }

    // 4. Financial Terms
    if (p.creditOffer > 0) {
        int bonus = static_cast<int>(std::min<int64_t>(30, p.creditOffer / 100));
        if (personality == MajorPersonality::Ferengi) {
            bonus = static_cast<int>(std::min<int64_t>(50, p.creditOffer / 60));
        }
        score += bonus;
        expl << "+ Credit sweetener (" << bonus << ") ";
    }
    if (p.creditDemand > 0) {
        int penalty = static_cast<int>(std::min<int64_t>(40, p.creditDemand / 80));
        score -= penalty;
        expl << "- Demanded tribute (-" << penalty << ") ";
    }

    // 5. Treaty Type Demands & Personalities
    switch (p.treatyType) {
        case TreatyType::Peace: {
            if (personality == MajorPersonality::Klingon) {
                res.threshold = 60;
            } else if (personality == MajorPersonality::Federation) {
                res.threshold = 45;
            } else {
                res.threshold = 50;
            }
            break;
        }
        case TreatyType::NonAggression: {
            res.threshold = 45;
            break;
        }
        case TreatyType::Friendship: {
            res.threshold = 55;
            break;
        }
        case TreatyType::Affiliation: {
            res.threshold = 65;
            break;
        }
        case TreatyType::Alliance: {
            res.threshold = 75;
            if (personality == MajorPersonality::Romulan) {
                res.threshold = 85;
            }
            break;
        }
        case TreatyType::Membership: {
            res.threshold = 80;
            break;
        }
        case TreatyType::Trade: {
            res.threshold = 50;
            if (personality == MajorPersonality::Ferengi) {
                res.threshold = 40;
            }
            break;
        }
        default:
            break;
    }

    // 6. Deterministic Pseudo-Random Variance (Seed + Turn + Proposal ID)
    uint64_t combinedSeed = seed ^ (static_cast<uint64_t>(state.currentTurn) * 2654435761ULL) ^ (p.id * 1013904223ULL);
    int variance = static_cast<int>(combinedSeed % 11) - 5; // [-5 .. +5]
    score += variance;
    expl << "+ Variance (" << variance << ") ";

    res.score = score;
    expl << "= Final Score " << score << " vs Threshold " << res.threshold;
    res.explanation = expl.str();

    if (score >= res.threshold) {
        res.decision = AiDecision::Accept;
    } else if (score < res.threshold - 20) {
        res.decision = AiDecision::Ignore;
    } else {
        res.decision = AiDecision::Reject;
    }

    return res;
}

// -----------------------------------------------------------------------------
// Engine Core Operations
// -----------------------------------------------------------------------------

class DiplomacyEngine {
public:
    // Submits a new proposal atomically
    static bool submitProposal(
        DiplomacyState& state,
        const DiplomacyContext& ctx,
        ParticipantId proposer,
        ParticipantId target,
        TreatyType treatyType,
        TreatyDuration duration,
        DiplomaticTone tone,
        int64_t creditOffer,
        int64_t creditDemand,
        uint64_t* outProposalId = nullptr,
        std::string* outError = nullptr)
    {
        Proposal p;
        p.id = state.nextProposalId;
        p.proposer = proposer;
        p.target = target;
        p.treatyType = treatyType;
        p.duration = duration;
        p.tone = tone;
        p.creditOffer = creditOffer;
        p.creditDemand = creditDemand;
        p.submittedTurn = state.currentTurn;
        p.dueTurn = state.currentTurn + 1;
        p.status = ProposalStatus::Pending;
        p.resolved = false;

        if (!validateProposalCreation(state, ctx, p, outError)) {
            return false;
        }

        state.nextProposalId++;
        state.proposals.push_back(p);
        if (outProposalId) *outProposalId = p.id;

        DiplomaticEvent evt;
        evt.turn = state.currentTurn;
        evt.type = DiplomaticEventType::ProposalSubmitted;
        evt.partyA = proposer;
        evt.partyB = target;
        evt.numericValue = creditOffer;
        std::ostringstream desc;
        desc << "Proposal " << p.id << " (" << treatyTypeName(treatyType) << ") submitted.";
        evt.description = desc.str();
        recordEvent(state, evt);

        return true;
    }

    // Authorized player or manual response: responder MUST equal proposal.target strictly
    static bool respondToOffer(
        DiplomacyState& state,
        ParticipantId responder,
        uint64_t proposalId,
        bool accept,
        const std::string& reason = "")
    {
        Proposal* p = state.findProposal(proposalId);
        if (!p) return false;
        if (p->status != ProposalStatus::Pending || p->resolved) return false;

        // Authorization check: responder identity MUST match proposal target
        if (p->target != responder) {
            return false;
        }

        p->status = accept ? ProposalStatus::Accepted : ProposalStatus::Rejected;
        p->rejectionReason = reason;
        return true;
    }

    // Backward-compatible overload for harness calls
    static bool respondToOffer(
        DiplomacyState& state,
        uint64_t proposalId,
        bool accept,
        const std::string& reason = "")
    {
        Proposal* p = state.findProposal(proposalId);
        if (!p) return false;
        return respondToOffer(state, p->target, proposalId, accept, reason);
    }

    // Immediately breaks all incompatible treaties upon war
    static void declareWar(
        DiplomacyState& state,
        ParticipantId aggressor,
        ParticipantId defender,
        TurnResolutionDelta& delta)
    {
        DiplomacyOperation op;
        op.opType = DiplomaticOpType::SetWarState;
        op.partyA = aggressor;
        op.partyB = defender;
        op.setAtWar = true; // Explicit typed boolean
        op.notes = "War declared";
        delta.operations.push_back(op);

        DiplomaticEvent evt;
        evt.turn = state.currentTurn;
        evt.type = DiplomaticEventType::WarDeclared;
        evt.partyA = aggressor;
        evt.partyB = defender;
        evt.description = "War declared between belligerents.";
        recordEvent(state, evt);
        delta.events.push_back(evt);

        for (auto& p : state.proposals) {
            if (p.status == ProposalStatus::Pending &&
                ((p.proposer == aggressor && p.target == defender) ||
                 (p.proposer == defender && p.target == aggressor)) &&
                p.treatyType != TreatyType::War)
            {
                p.status = ProposalStatus::Rejected;
                p.rejectionReason = "Cancelled due to declaration of war";
                delta.rejectedProposals.push_back(p.id);

                DiplomaticEvent rejEvt;
                rejEvt.turn = state.currentTurn;
                rejEvt.type = DiplomaticEventType::ProposalRejected;
                rejEvt.partyA = p.proposer;
                rejEvt.partyB = p.target;
                rejEvt.description = "Proposal cancelled due to declaration of war.";
                recordEvent(state, rejEvt);
                delta.events.push_back(rejEvt);
            }
        }

        breakIncompatibleTreaties(state, aggressor, defender, delta);
    }

    // Resolves turn boundary: decrements treaties, resolves pending offers, produces operations
    static TurnResolutionDelta resolveTurn(
        DiplomacyState& state,
        const DiplomacyContext& ctx,
        uint64_t aiSeed = 1337)
    {
        TurnResolutionDelta delta;

        // ---------------------------------------------------------------------
        // Step 1: Tick Active Treaties & Check Expiry
        // ---------------------------------------------------------------------
        std::vector<ActiveTreaty> remainingActive;
        remainingActive.reserve(state.activeTreaties.size());

        for (auto& treaty : state.activeTreaties) {
            if (treaty.status != TreatyStatus::Active) continue;

            if (treaty.duration != TreatyDuration::Indefinite && treaty.turnsRemaining > 0) {
                treaty.turnsRemaining--;
            }

            if (treaty.duration != TreatyDuration::Indefinite && treaty.turnsRemaining <= 0) {
                treaty.status = TreatyStatus::Expired;
                archiveTreaty(state, treaty);
                delta.expiredTreaties.push_back(treaty.id);

                // If expired treaty was friendship with minor, update minor's friendship status
                if (treaty.type == TreatyType::Friendship) {
                    if (treaty.partyA.isMinor) {
                        auto* m = state.getMinorState(treaty.partyA.id);
                        if (m) m->friendshipTurns[treaty.partyB.id] = -1;
                    }
                    if (treaty.partyB.isMinor) {
                        auto* m = state.getMinorState(treaty.partyB.id);
                        if (m) m->friendshipTurns[treaty.partyA.id] = -1;
                    }
                }

                DiplomacyOperation op;
                op.opType = DiplomaticOpType::TerminateTreaty;
                op.partyA = treaty.partyA;
                op.partyB = treaty.partyB;
                op.treatyType = treaty.type;
                op.notes = "Treaty reached duration expiry";
                delta.operations.push_back(op);

                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::TreatyExpired;
                evt.partyA = treaty.partyA;
                evt.partyB = treaty.partyB;
                evt.description = "Treaty expired naturally.";
                recordEvent(state, evt);
                delta.events.push_back(evt);
            } else {
                remainingActive.push_back(treaty);
            }
        }
        state.activeTreaties = std::move(remainingActive);

        // ---------------------------------------------------------------------
        // Step 2: Initialize Working Resolution Ledger & Working State
        // ---------------------------------------------------------------------
        std::map<ParticipantId, int64_t> workingBalances;
        std::map<std::pair<ParticipantId, ParticipantId>, bool> workingWars;

        auto getWorkingBalance = [&](ParticipantId p) -> int64_t {
            auto it = workingBalances.find(p);
            if (it != workingBalances.end()) return it->second;
            int64_t initial = ctx.creditBalance(p);
            workingBalances[p] = initial;
            return initial;
        };

        auto isWorkingAtWar = [&](ParticipantId a, ParticipantId b) -> bool {
            auto it = workingWars.find({a, b});
            if (it != workingWars.end()) return it->second;
            bool initial = ctx.isAtWar(a, b);
            workingWars[{a, b}] = initial;
            workingWars[{b, a}] = initial;
            return initial;
        };

        auto setWorkingAtWar = [&](ParticipantId a, ParticipantId b, bool val) {
            workingWars[{a, b}] = val;
            workingWars[{b, a}] = val;
        };

        // Collect indices of pending proposals to resolve
        std::vector<size_t> proposalIndices;
        for (size_t i = 0; i < state.proposals.size(); ++i) {
            if (!state.proposals[i].resolved) {
                proposalIndices.push_back(i);
            }
        }

        // ---------------------------------------------------------------------
        // ---------------------------------------------------------------------
        // Step 3: Rank Concurrent Membership Offers by Preference (DIP-02)
        // Group membership proposals by target minor:
        // Highest CURRENT attitude gets priority.
        // On tie, deterministic seeded diplomacy RNG draw breaks the tie.
        // ---------------------------------------------------------------------
        std::map<int, std::vector<size_t>> minorMembershipOffers;
        for (size_t idx : proposalIndices) {
            const auto& p = state.proposals[idx];
            if (p.target.isMinor && p.treatyType == TreatyType::Membership &&
                (p.status == ProposalStatus::Pending || p.status == ProposalStatus::Accepted))
            {
                minorMembershipOffers[p.target.id].push_back(idx);
            }
        }

        for (auto& [minorId, offers] : minorMembershipOffers) {
            if (offers.size() <= 1) continue;

            const auto* minor = state.getMinorState(minorId);

            // Group offers by attitude descending
            std::map<int, std::vector<size_t>, std::greater<int>> attitudeGroups;
            for (size_t idx : offers) {
                const auto& p = state.proposals[idx];
                int att = minor ? minor->getAttitude(p.proposer.id) : 50;
                attitudeGroups[att].push_back(idx);
            }

            std::vector<size_t> rankedOffers;
            rankedOffers.reserve(offers.size());

            for (auto& [att, group] : attitudeGroups) {
                if (group.size() == 1) {
                    rankedOffers.push_back(group.front());
                } else {
                    // Tie-breaker using seeded diplomacy RNG
                    uint64_t tieSeed = aiSeed ^ (static_cast<uint64_t>(state.currentTurn) * 6364136223846793005ULL)
                                             ^ (static_cast<uint64_t>(minorId) * 1442695040888963407ULL);
                    size_t tieWinnerIdx = static_cast<size_t>(tieSeed % group.size());
                    rankedOffers.push_back(group[tieWinnerIdx]);
                    for (size_t i = 0; i < group.size(); ++i) {
                        if (i != tieWinnerIdx) {
                            rankedOffers.push_back(group[i]);
                        }
                    }
                }
            }

            // Find the positions where this minor's candidate membership offers appear in proposalIndices
            std::vector<size_t> positionsInIndices;
            for (size_t pos = 0; pos < proposalIndices.size(); ++pos) {
                size_t pIdx = proposalIndices[pos];
                const auto& p = state.proposals[pIdx];
                if (p.target.isMinor && p.target.id == minorId && p.treatyType == TreatyType::Membership &&
                    (p.status == ProposalStatus::Pending || p.status == ProposalStatus::Accepted))
                {
                    positionsInIndices.push_back(pos);
                }
            }

            // Replace in those positions in the newly ranked order
            for (size_t i = 0; i < positionsInIndices.size() && i < rankedOffers.size(); ++i) {
                proposalIndices[positionsInIndices[i]] = rankedOffers[i];
            }
        }

        // ---------------------------------------------------------------------
        // Step 4: Resolve Proposals against Cumulative State & Working Ledger
        // ---------------------------------------------------------------------
        std::set<int> minorsJoinedThisTurn;

        for (size_t idx : proposalIndices) {
            auto& p = state.proposals[idx];
            if (p.resolved) continue;

            // Check if minor has already joined an empire during this turn's resolution
            if (p.target.isMinor && p.treatyType == TreatyType::Membership) {
                if (minorsJoinedThisTurn.count(p.target.id) > 0) {
                    p.resolved = true;
                    p.status = ProposalStatus::Rejected;
                    p.rejectionReason = "Minor accepted competing membership offer from another empire";
                    delta.rejectedProposals.push_back(p.id);

                    DiplomaticEvent evt;
                    evt.turn = state.currentTurn;
                    evt.type = DiplomaticEventType::ProposalRejected;
                    evt.partyA = p.proposer;
                    evt.partyB = p.target;
                    evt.description = "Minor rejected membership in favor of rival empire.";
                    recordEvent(state, evt);
                    delta.events.push_back(evt);
                    continue;
                }
            }

            if (p.status == ProposalStatus::Pending) {
                if (!ctx.isHuman(p.target)) {
                    AiDecisionResult aiRes = evaluateAiProposal(state, ctx, p, aiSeed);
                    if (aiRes.decision == AiDecision::Accept) {
                        p.status = ProposalStatus::Accepted;
                    } else if (aiRes.decision == AiDecision::Reject) {
                        p.status = ProposalStatus::Rejected;
                        p.rejectionReason = aiRes.explanation;
                    } else {
                        p.status = ProposalStatus::Ignored;
                        p.rejectionReason = "Proposal ignored by recipient";
                    }
                } else {
                    if (state.currentTurn >= p.dueTurn) {
                        p.status = ProposalStatus::Ignored;
                        p.rejectionReason = "Proposal lapsed without player response";
                    } else {
                        continue;
                    }
                }
            }

            p.resolved = true;

            if (p.status == ProposalStatus::Accepted) {
                // REVALIDATION AGAINST CUMULATIVE INTRA-TURN STATE:

                // 1. Working war check
                bool atWarNow = isWorkingAtWar(p.proposer, p.target);
                if (p.treatyType != TreatyType::Peace && p.treatyType != TreatyType::War && p.treatyType != TreatyType::None && atWarNow) {
                    p.status = ProposalStatus::VoidedByWar;
                    p.rejectionReason = "Voided by intervening outbreak of war";
                    delta.rejectedProposals.push_back(p.id);
                    continue;
                }
                if (p.treatyType == TreatyType::Peace && !atWarNow) {
                    p.status = ProposalStatus::Rejected;
                    p.rejectionReason = "Belligerents are no longer at war";
                    delta.rejectedProposals.push_back(p.id);
                    continue;
                }

                // 2. Working Minor Membership Revalidation at Settlement
                if (p.treatyType == TreatyType::Membership) {
                    const auto* minor = state.getMinorState(p.target.id);
                    if (!minor) {
                        p.status = ProposalStatus::Rejected;
                        p.rejectionReason = "Minor not found";
                        delta.rejectedProposals.push_back(p.id);
                        continue;
                    }
                    if (minorsJoinedThisTurn.count(p.target.id) > 0) {
                        p.status = ProposalStatus::Rejected;
                        p.rejectionReason = "Minor accepted competing membership offer from another empire";
                        delta.rejectedProposals.push_back(p.id);

                        DiplomaticEvent evt;
                        evt.turn = state.currentTurn;
                        evt.type = DiplomaticEventType::ProposalRejected;
                        evt.partyA = p.proposer;
                        evt.partyB = p.target;
                        evt.description = "Minor rejected membership in favor of rival empire.";
                        recordEvent(state, evt);
                        delta.events.push_back(evt);
                        continue;
                    }
                    if (minor->membershipOwner != 0) {
                        p.status = ProposalStatus::Rejected;
                        p.rejectionReason = "Minor is already a member of an empire";
                        delta.rejectedProposals.push_back(p.id);
                        continue;
                    }
                    int frTurn = minor->getFriendshipTurn(p.proposer.id);
                    if (frTurn < 0 || frTurn >= state.currentTurn) {
                        p.status = ProposalStatus::Rejected;
                        p.rejectionReason = "Membership requires an established active Friendship from a prior turn";
                        delta.rejectedProposals.push_back(p.id);
                        continue;
                    }
                    if (minor->getAttitude(p.proposer.id) < 80) {
                        p.status = ProposalStatus::Rejected;
                        p.rejectionReason = "Minor attitude at settlement fell below required 80 threshold";
                        delta.rejectedProposals.push_back(p.id);
                        continue;
                    }
                }

                // 3. Treaty Downgrade / Conflict Check against cumulative active treaties
                int proposedRank = getTreatyRank(p.treatyType);
                bool conflictFound = false;
                for (const auto& existing : state.activeTreaties) {
                    if (existing.status == TreatyStatus::Active && existing.involvesBoth(p.proposer, p.target)) {
                        if (existing.type == p.treatyType) {
                            p.status = ProposalStatus::Rejected;
                            p.rejectionReason = "Identical treaty is already active";
                            conflictFound = true;
                            break;
                        }
                        int existingRank = getTreatyRank(existing.type);
                        if (existingRank >= proposedRank && proposedRank > 0) {
                            p.status = ProposalStatus::Rejected;
                            p.rejectionReason = "An equal or stronger treaty is already active";
                            conflictFound = true;
                            break;
                        }
                    }
                }
                if (conflictFound) {
                    delta.rejectedProposals.push_back(p.id);
                    continue;
                }

                // 4. Working Ledger Overdraft Check
                int64_t propBal = getWorkingBalance(p.proposer);
                int64_t targetBal = getWorkingBalance(p.target);

                if (p.creditOffer > 0 && propBal < p.creditOffer) {
                    p.status = ProposalStatus::Rejected;
                    p.rejectionReason = "Proposer overdraft: insufficient funds in resolution ledger";
                    delta.rejectedProposals.push_back(p.id);

                    DiplomaticEvent evt;
                    evt.turn = state.currentTurn;
                    evt.type = DiplomaticEventType::ProposalRejected;
                    evt.partyA = p.proposer;
                    evt.partyB = p.target;
                    evt.description = "Proposal rejected: Proposer unable to pay offered credits.";
                    recordEvent(state, evt);
                    delta.events.push_back(evt);
                    continue;
                }

                if (p.creditDemand > 0 && targetBal < p.creditDemand) {
                    p.status = ProposalStatus::Rejected;
                    p.rejectionReason = "Target overdraft: insufficient funds in resolution ledger";
                    delta.rejectedProposals.push_back(p.id);

                    DiplomaticEvent evt;
                    evt.turn = state.currentTurn;
                    evt.type = DiplomaticEventType::ProposalRejected;
                    evt.partyA = p.proposer;
                    evt.partyB = p.target;
                    evt.description = "Proposal rejected: Target unable to pay demanded credits.";
                    recordEvent(state, evt);
                    delta.events.push_back(evt);
                    continue;
                }

                // -------------------------------------------------------------
                // ALL CHECKS PASSED: Commit Acceptance & Apply State Mutations
                // -------------------------------------------------------------
                delta.acceptedProposals.push_back(p.id);

                // A. Update Working Ledger
                if (p.creditOffer > 0) {
                    workingBalances[p.proposer] -= p.creditOffer;
                    workingBalances[p.target] += p.creditOffer;

                    DiplomacyOperation credOp;
                    credOp.opType = DiplomaticOpType::TransferCredits;
                    credOp.partyA = p.proposer;
                    credOp.partyB = p.target;
                    credOp.credits = p.creditOffer;
                    credOp.notes = "Proposal credit payment";
                    delta.operations.push_back(credOp);
                }
                if (p.creditDemand > 0) {
                    workingBalances[p.target] -= p.creditDemand;
                    workingBalances[p.proposer] += p.creditDemand;

                    DiplomacyOperation credOp;
                    credOp.opType = DiplomaticOpType::TransferCredits;
                    credOp.partyA = p.target;
                    credOp.partyB = p.proposer;
                    credOp.credits = p.creditDemand;
                    credOp.notes = "Proposal tribute demand";
                    delta.operations.push_back(credOp);
                }

                // B. Treaty Effects
                if (p.treatyType == TreatyType::War) {
                    setWorkingAtWar(p.proposer, p.target, true);

                    DiplomacyOperation warOp;
                    warOp.opType = DiplomaticOpType::SetWarState;
                    warOp.partyA = p.proposer;
                    warOp.partyB = p.target;
                    warOp.setAtWar = true; // Explicit typed boolean
                    warOp.notes = "War state established via proposal";
                    delta.operations.push_back(warOp);

                    DiplomaticEvent evt;
                    evt.turn = state.currentTurn;
                    evt.type = DiplomaticEventType::WarDeclared;
                    evt.partyA = p.proposer;
                    evt.partyB = p.target;
                    evt.description = "State of war declared between belligerents.";
                    recordEvent(state, evt);
                    delta.events.push_back(evt);

                    breakIncompatibleTreaties(state, p.proposer, p.target, delta);
                } else if (p.treatyType == TreatyType::Peace) {
                    setWorkingAtWar(p.proposer, p.target, false);

                    DiplomacyOperation peaceOp;
                    peaceOp.opType = DiplomaticOpType::SetWarState;
                    peaceOp.partyA = p.proposer;
                    peaceOp.partyB = p.target;
                    peaceOp.setAtWar = false; // Explicit typed boolean
                    peaceOp.notes = "Peace established";
                    delta.operations.push_back(peaceOp);

                    DiplomaticEvent evt;
                    evt.turn = state.currentTurn;
                    evt.type = DiplomaticEventType::PeaceEstablished;
                    evt.partyA = p.proposer;
                    evt.partyB = p.target;
                    evt.description = "Peace treaty concluded.";
                    recordEvent(state, evt);
                    delta.events.push_back(evt);
                } else if (p.treatyType == TreatyType::Membership) {
                    auto* minor = state.getMinorState(p.target.id);
                    if (minor) {
                        minor->membershipOwner = p.proposer.id;
                        minor->membershipTurn = state.currentTurn;
                        minorsJoinedThisTurn.insert(p.target.id);

                        DiplomacyOperation memOp;
                        memOp.opType = DiplomaticOpType::TransferMinorMembership;
                        memOp.partyA = p.target;
                        memOp.newOwner = p.proposer.id;
                        memOp.notes = "Minor joined major empire via diplomatic membership";
                        delta.operations.push_back(memOp);

                        DiplomaticEvent evt;
                        evt.turn = state.currentTurn;
                        evt.type = DiplomaticEventType::MinorMembershipJoined;
                        evt.partyA = p.proposer;
                        evt.partyB = p.target;
                        evt.description = "Minor race accepted full membership into empire.";
                        recordEvent(state, evt);
                        delta.events.push_back(evt);
                    }
                } else if (p.treatyType != TreatyType::None) {
                    for (auto& existing : state.activeTreaties) {
                        if (existing.status == TreatyStatus::Active && existing.involvesBoth(p.proposer, p.target)) {
                            if (p.treatyType != TreatyType::Trade && existing.type != TreatyType::Trade) {
                                existing.status = TreatyStatus::Superseded;
                                archiveTreaty(state, existing);

                                DiplomacyOperation termOp;
                                termOp.opType = DiplomaticOpType::TerminateTreaty;
                                termOp.partyA = existing.partyA;
                                termOp.partyB = existing.partyB;
                                termOp.treatyType = existing.type;
                                termOp.notes = "Superseded by higher treaty";
                                delta.operations.push_back(termOp);
                            }
                        }
                    }

                    ActiveTreaty newTreaty;
                    newTreaty.id = state.nextTreatyId++;
                    newTreaty.partyA = p.proposer;
                    newTreaty.partyB = p.target;
                    newTreaty.type = p.treatyType;
                    newTreaty.duration = p.duration;
                    newTreaty.startTurn = state.currentTurn;
                    newTreaty.turnsRemaining = (p.duration == TreatyDuration::Indefinite) ? -1 : static_cast<int>(p.duration);
                    newTreaty.status = TreatyStatus::Active;

                    if (state.activeTreaties.size() < DiplomacyState::MaxActiveTreaties) {
                        state.activeTreaties.push_back(newTreaty);
                    }

                    if (p.target.isMinor && p.treatyType == TreatyType::Friendship) {
                        auto* minor = state.getMinorState(p.target.id);
                        if (minor) {
                            minor->friendshipTurns[p.proposer.id] = state.currentTurn;
                        }
                    }

                    DiplomacyOperation treatOp;
                    treatOp.opType = DiplomaticOpType::ApplyTreaty;
                    treatOp.partyA = p.proposer;
                    treatOp.partyB = p.target;
                    treatOp.treatyType = p.treatyType;
                    treatOp.duration = p.duration;
                    treatOp.turnsRemaining = newTreaty.turnsRemaining;
                    treatOp.notes = "Treaty ratified";
                    delta.operations.push_back(treatOp);

                    DiplomaticEvent evt;
                    evt.turn = state.currentTurn;
                    evt.type = DiplomaticEventType::TreatyEstablished;
                    evt.partyA = p.proposer;
                    evt.partyB = p.target;
                    evt.description = "Treaty ratified between parties.";
                    recordEvent(state, evt);
                    delta.events.push_back(evt);
                }

                if (p.target.isMinor && p.creditOffer > 0) {
                    auto* minor = state.getMinorState(p.target.id);
                    if (minor) {
                        int gain = static_cast<int>((p.creditOffer * 4) / 1000);
                        if (gain < 1) gain = 1;
                        int oldAtt = minor->getAttitude(p.proposer.id);
                        int newAtt = std::clamp(oldAtt + gain, 0, 100);
                        minor->attitudes[p.proposer.id] = newAtt;

                        DiplomacyOperation attOp;
                        attOp.opType = DiplomaticOpType::UpdateMinorAttitude;
                        attOp.partyA = p.target;
                        attOp.partyB = p.proposer;
                        attOp.attitudeDelta = newAtt - oldAtt;
                        attOp.notes = "Attitude increased by gift";
                        delta.operations.push_back(attOp);
                    }
                }
            } else if (p.status == ProposalStatus::Rejected) {
                delta.rejectedProposals.push_back(p.id);

                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::ProposalRejected;
                evt.partyA = p.proposer;
                evt.partyB = p.target;
                evt.description = "Proposal rejected by target.";
                recordEvent(state, evt);
                delta.events.push_back(evt);
            } else if (p.status == ProposalStatus::Ignored) {
                delta.ignoredProposals.push_back(p.id);

                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::ProposalIgnored;
                evt.partyA = p.proposer;
                evt.partyB = p.target;
                evt.description = "Proposal lapsed without answer.";
                recordEvent(state, evt);
                delta.events.push_back(evt);
            }
        }

        pruneProposals(state);

        state.currentTurn++;

        return delta;
    }

private:
    static void breakIncompatibleTreaties(
        DiplomacyState& state,
        ParticipantId a,
        ParticipantId b,
        TurnResolutionDelta& delta)
    {
        std::vector<ActiveTreaty> remaining;
        remaining.reserve(state.activeTreaties.size());

        for (auto& t : state.activeTreaties) {
            if (t.status == TreatyStatus::Active && t.involvesBoth(a, b)) {
                t.status = TreatyStatus::BrokenByWar;
                archiveTreaty(state, t);
                delta.brokenTreaties.push_back(t.id);

                // Break friendship with minor if involved
                if (t.partyA.isMinor) {
                    auto* m = state.getMinorState(t.partyA.id);
                    if (m) m->friendshipTurns[t.partyB.id] = -1;
                }
                if (t.partyB.isMinor) {
                    auto* m = state.getMinorState(t.partyB.id);
                    if (m) m->friendshipTurns[t.partyA.id] = -1;
                }

                DiplomacyOperation op;
                op.opType = DiplomaticOpType::TerminateTreaty;
                op.partyA = t.partyA;
                op.partyB = t.partyB;
                op.treatyType = t.type;
                op.notes = "Broken by outbreak of war";
                delta.operations.push_back(op);

                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::TreatyBrokenByWar;
                evt.partyA = a;
                evt.partyB = b;
                evt.description = "Treaty terminated due to war.";
                recordEvent(state, evt);
                delta.events.push_back(evt);
            } else {
                remaining.push_back(t);
            }
        }
        state.activeTreaties = std::move(remaining);
    }

    static void archiveTreaty(DiplomacyState& state, const ActiveTreaty& treaty) {
        if (state.treatyHistory.size() >= DiplomacyState::MaxTreatyHistory) {
            state.treatyHistory.erase(state.treatyHistory.begin());
        }
        state.treatyHistory.push_back(treaty);
    }

    static void recordEvent(DiplomacyState& state, const DiplomaticEvent& evt) {
        if (state.eventHistory.size() >= DiplomacyState::MaxEventHistory) {
            state.eventHistory.erase(state.eventHistory.begin());
        }
        state.eventHistory.push_back(evt);
    }

    static void pruneProposals(DiplomacyState& state) {
        while (state.proposals.size() > DiplomacyState::MaxPendingProposals) {
            state.proposals.erase(state.proposals.begin());
        }
    }
};

// -----------------------------------------------------------------------------
// Serialization & Deserialization String Encoders
// -----------------------------------------------------------------------------

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
                if (outError) *outError = "Malformed escape sequence: truncated percent sequence";
                return false;
            }
            char h1 = token[i + 1];
            char h2 = token[i + 2];
            auto fromHex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int v1 = fromHex(h1);
            int v2 = fromHex(h2);
            if (v1 < 0 || v2 < 0) {
                if (outError) *outError = "Malformed escape sequence: non-hex characters after percent";
                return false;
            }
            unsigned char byteVal = static_cast<unsigned char>((v1 << 4) | v2);
            out.push_back(static_cast<char>(byteVal));
            i += 2;
        } else {
            out.push_back(token[i]);
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// Serialization & Deserialization
// -----------------------------------------------------------------------------

inline std::string serializeDiplomacyState(const DiplomacyState& state) {
    std::ostringstream ss;
    ss << "BOTF_DIPLOMACY_V2\n";
    ss << "currentTurn " << state.currentTurn << "\n";
    ss << "nextProposalId " << state.nextProposalId << "\n";
    ss << "nextTreatyId " << state.nextTreatyId << "\n";

    // Proposals
    ss << "proposals " << state.proposals.size() << "\n";
    for (const auto& p : state.proposals) {
        ss << "p " << p.id << " "
           << p.proposer.id << " " << (p.proposer.isMinor ? 1 : 0) << " "
           << p.target.id << " " << (p.target.isMinor ? 1 : 0) << " "
           << static_cast<int>(p.treatyType) << " "
           << static_cast<int>(p.duration) << " "
           << static_cast<int>(p.tone) << " "
           << p.creditOffer << " "
           << p.creditDemand << " "
           << p.submittedTurn << " "
           << p.dueTurn << " "
           << static_cast<int>(p.status) << " "
           << (p.resolved ? 1 : 0) << " "
           << encodeString(p.rejectionReason) << "\n";
    }

    // Active Treaties
    ss << "activeTreaties " << state.activeTreaties.size() << "\n";
    for (const auto& t : state.activeTreaties) {
        ss << "t " << t.id << " "
           << t.partyA.id << " " << (t.partyA.isMinor ? 1 : 0) << " "
           << t.partyB.id << " " << (t.partyB.isMinor ? 1 : 0) << " "
           << static_cast<int>(t.type) << " "
           << static_cast<int>(t.duration) << " "
           << t.startTurn << " "
           << t.turnsRemaining << " "
           << static_cast<int>(t.status) << "\n";
    }

    // Treaty History
    ss << "treatyHistory " << state.treatyHistory.size() << "\n";
    for (const auto& t : state.treatyHistory) {
        ss << "th " << t.id << " "
           << t.partyA.id << " " << (t.partyA.isMinor ? 1 : 0) << " "
           << t.partyB.id << " " << (t.partyB.isMinor ? 1 : 0) << " "
           << static_cast<int>(t.type) << " "
           << static_cast<int>(t.duration) << " "
           << t.startTurn << " "
           << t.turnsRemaining << " "
           << static_cast<int>(t.status) << "\n";
    }

    // Minor States
    ss << "minorStates " << state.minorStates.size() << "\n";
    for (const auto& m : state.minorStates) {
        ss << "m " << m.minorId << " " << m.systemId << " "
           << m.membershipOwner << " " << m.membershipTurn << " "
           << encodeString(m.raceName) << "\n";

        ss << "attitudes " << m.attitudes.size();
        for (const auto& [empire, att] : m.attitudes) {
            ss << " " << empire << " " << att;
        }
        ss << "\n";

        ss << "contacts " << m.contacts.size();
        for (const auto& [empire, c] : m.contacts) {
            ss << " " << empire << " " << (c ? 1 : 0);
        }
        ss << "\n";

        ss << "friendships " << m.friendshipTurns.size();
        for (const auto& [empire, turn] : m.friendshipTurns) {
            ss << " " << empire << " " << turn;
        }
        ss << "\n";
    }

    // Event History
    ss << "eventHistory " << state.eventHistory.size() << "\n";
    for (const auto& e : state.eventHistory) {
        ss << "e " << e.turn << " "
           << static_cast<int>(e.type) << " "
           << e.partyA.id << " " << (e.partyA.isMinor ? 1 : 0) << " "
           << e.partyB.id << " " << (e.partyB.isMinor ? 1 : 0) << " "
           << e.numericValue << " "
           << encodeString(e.description) << "\n";
    }

    ss << "END_BOTF_DIPLOMACY\n";
    return ss.str();
}

inline bool deserializeDiplomacyState(
    const std::string& data,
    DiplomacyState& outState,
    std::string* outError = nullptr)
{
    std::istringstream ss(data);
    std::string token;

    if (!(ss >> token)) {
        if (outError) *outError = "Empty diplomacy save data";
        return false;
    }

    int version = 0;
    if (token == "BOTF_DIPLOMACY_V2") {
        version = 2;
    } else if (token == "BOTF_DIPLOMACY_V1") {
        version = 1;
    } else {
        if (outError) *outError = "Invalid magic token; expected BOTF_DIPLOMACY_V1 or BOTF_DIPLOMACY_V2, got: " + token;
        return false;
    }

    DiplomacyState temp;

    if (!(ss >> token) || token != "currentTurn" || !(ss >> temp.currentTurn) || temp.currentTurn < 0) {
        if (outError) *outError = "Malformed currentTurn";
        return false;
    }

    if (!(ss >> token) || token != "nextProposalId" || !(ss >> temp.nextProposalId) || temp.nextProposalId == 0) {
        if (outError) *outError = "Malformed nextProposalId";
        return false;
    }

    if (!(ss >> token) || token != "nextTreatyId" || !(ss >> temp.nextTreatyId) || temp.nextTreatyId == 0) {
        if (outError) *outError = "Malformed nextTreatyId";
        return false;
    }

    // Proposals
    size_t propCount = 0;
    if (!(ss >> token) || token != "proposals" || !(ss >> propCount)) {
        if (outError) *outError = "Malformed proposals count";
        return false;
    }
    if (propCount > DiplomacyState::MaxPendingProposals) {
        if (outError) *outError = "Proposals count exceeds maximum limit";
        return false;
    }

    std::set<uint64_t> seenPropIds;
    for (size_t i = 0; i < propCount; ++i) {
        Proposal p;
        int propIsMinor = 0, targetIsMinor = 0;
        int tType = 0, dur = 0, tone = 0, stat = 0, res = 0;
        std::string rawReason;

        if (!(ss >> token) || token != "p" ||
            !(ss >> p.id >> p.proposer.id >> propIsMinor
                 >> p.target.id >> targetIsMinor
                 >> tType >> dur >> tone
                 >> p.creditOffer >> p.creditDemand
                 >> p.submittedTurn >> p.dueTurn >> stat >> res >> rawReason))
        {
            if (outError) *outError = "Malformed proposal row";
            return false;
        }

        if (p.id == 0 || p.proposer.id <= 0 || p.target.id <= 0) {
            if (outError) *outError = "Invalid participant or proposal ID in row";
            return false;
        }
        if (p.id >= temp.nextProposalId) {
            if (outError) *outError = "Proposal ID violates nextProposalId monotonicity";
            return false;
        }
        if (seenPropIds.find(p.id) != seenPropIds.end()) {
            if (outError) *outError = "Duplicate proposal ID detected";
            return false;
        }
        seenPropIds.insert(p.id);

        if ((propIsMinor != 0 && propIsMinor != 1) || (targetIsMinor != 0 && targetIsMinor != 1) || (res != 0 && res != 1)) {
            if (outError) *outError = "Invalid boolean field in proposal";
            return false;
        }

        if (!isValidTreatyType(tType)) {
            if (outError) *outError = "Invalid TreatyType enum value in proposal";
            return false;
        }
        if (!isValidTreatyDuration(dur)) {
            if (outError) *outError = "Invalid TreatyDuration enum value in proposal";
            return false;
        }
        if (!isValidDiplomaticTone(tone)) {
            if (outError) *outError = "Invalid DiplomaticTone enum value in proposal";
            return false;
        }
        if (!isValidProposalStatus(stat)) {
            if (outError) *outError = "Invalid ProposalStatus enum value in proposal";
            return false;
        }

        if (p.creditOffer < 0 || p.creditDemand < 0) {
            if (outError) *outError = "Negative credits in proposal";
            return false;
        }
        if (p.submittedTurn < 0 || p.dueTurn < p.submittedTurn) {
            if (outError) *outError = "Invalid proposal submittedTurn or dueTurn";
            return false;
        }

        p.proposer.isMinor = (propIsMinor != 0);
        p.target.isMinor = (targetIsMinor != 0);
        p.treatyType = static_cast<TreatyType>(tType);
        p.duration = static_cast<TreatyDuration>(dur);
        p.tone = static_cast<DiplomaticTone>(tone);
        p.status = static_cast<ProposalStatus>(stat);
        p.resolved = (res != 0);

        if (version >= 2) {
            if (!decodeString(rawReason, p.rejectionReason, outError)) {
                return false;
            }
        } else {
            if (rawReason != "None") {
                std::replace(rawReason.begin(), rawReason.end(), '_', ' ');
                p.rejectionReason = rawReason;
            } else {
                p.rejectionReason.clear();
            }
        }

        temp.proposals.push_back(p);
    }

    // Active Treaties
    size_t activeCount = 0;
    if (!(ss >> token) || token != "activeTreaties" || !(ss >> activeCount)) {
        if (outError) *outError = "Malformed activeTreaties count";
        return false;
    }
    if (activeCount > DiplomacyState::MaxActiveTreaties) {
        if (outError) *outError = "Active treaties count exceeds maximum limit";
        return false;
    }

    std::set<uint64_t> seenTreatyIds;
    for (size_t i = 0; i < activeCount; ++i) {
        ActiveTreaty t;
        int partyAMinor = 0, partyBMinor = 0;
        int tType = 0, dur = 0, stat = 0;

        if (!(ss >> token) || token != "t" ||
            !(ss >> t.id >> t.partyA.id >> partyAMinor
                 >> t.partyB.id >> partyBMinor
                 >> tType >> dur
                 >> t.startTurn >> t.turnsRemaining >> stat))
        {
            if (outError) *outError = "Malformed active treaty row";
            return false;
        }

        if (t.id == 0 || t.partyA.id <= 0 || t.partyB.id <= 0) {
            if (outError) *outError = "Invalid treaty ID or participant ID";
            return false;
        }
        if (t.id >= temp.nextTreatyId) {
            if (outError) *outError = "Active treaty ID violates nextTreatyId monotonicity";
            return false;
        }
        if (seenTreatyIds.find(t.id) != seenTreatyIds.end()) {
            if (outError) *outError = "Duplicate active treaty ID detected";
            return false;
        }
        seenTreatyIds.insert(t.id);

        if ((partyAMinor != 0 && partyAMinor != 1) || (partyBMinor != 0 && partyBMinor != 1)) {
            if (outError) *outError = "Invalid boolean in active treaty";
            return false;
        }
        if (!isValidTreatyType(tType) || tType == 0) {
            if (outError) *outError = "Invalid TreatyType in active treaty";
            return false;
        }
        if (!isValidTreatyDuration(dur)) {
            if (outError) *outError = "Invalid TreatyDuration in active treaty";
            return false;
        }
        if (!isValidTreatyStatus(stat)) {
            if (outError) *outError = "Invalid TreatyStatus in active treaty";
            return false;
        }

        t.partyA.isMinor = (partyAMinor != 0);
        t.partyB.isMinor = (partyBMinor != 0);
        t.type = static_cast<TreatyType>(tType);
        t.duration = static_cast<TreatyDuration>(dur);
        t.status = static_cast<TreatyStatus>(stat);

        temp.activeTreaties.push_back(t);
    }

    // Treaty History
    size_t histCount = 0;
    if (!(ss >> token) || token != "treatyHistory" || !(ss >> histCount)) {
        if (outError) *outError = "Malformed treatyHistory count";
        return false;
    }
    if (histCount > DiplomacyState::MaxTreatyHistory) {
        if (outError) *outError = "Treaty history count exceeds maximum limit";
        return false;
    }

    for (size_t i = 0; i < histCount; ++i) {
        ActiveTreaty t;
        int partyAMinor = 0, partyBMinor = 0;
        int tType = 0, dur = 0, stat = 0;

        if (!(ss >> token) || token != "th" ||
            !(ss >> t.id >> t.partyA.id >> partyAMinor
                 >> t.partyB.id >> partyBMinor
                 >> tType >> dur
                 >> t.startTurn >> t.turnsRemaining >> stat))
        {
            if (outError) *outError = "Malformed treaty history row";
            return false;
        }

        if (t.id == 0 || t.partyA.id <= 0 || t.partyB.id <= 0) {
            if (outError) *outError = "Invalid treaty ID or participant ID";
            return false;
        }
        if (t.id >= temp.nextTreatyId) {
            if (outError) *outError = "History treaty ID violates nextTreatyId monotonicity";
            return false;
        }
        if (seenTreatyIds.find(t.id) != seenTreatyIds.end()) {
            if (outError) *outError = "Duplicate treaty ID detected across active/history";
            return false;
        }
        seenTreatyIds.insert(t.id);

        if ((partyAMinor != 0 && partyAMinor != 1) || (partyBMinor != 0 && partyBMinor != 1)) {
            if (outError) *outError = "Invalid boolean in history treaty";
            return false;
        }
        if (!isValidTreatyType(tType) || tType == 0) {
            if (outError) *outError = "Invalid TreatyType in history treaty";
            return false;
        }
        if (!isValidTreatyDuration(dur)) {
            if (outError) *outError = "Invalid TreatyDuration in history treaty";
            return false;
        }
        if (!isValidTreatyStatus(stat)) {
            if (outError) *outError = "Invalid TreatyStatus in history treaty";
            return false;
        }

        t.partyA.isMinor = (partyAMinor != 0);
        t.partyB.isMinor = (partyBMinor != 0);
        t.type = static_cast<TreatyType>(tType);
        t.duration = static_cast<TreatyDuration>(dur);
        t.status = static_cast<TreatyStatus>(stat);

        temp.treatyHistory.push_back(t);
    }

    // Minor States
    size_t minorCount = 0;
    if (!(ss >> token) || token != "minorStates" || !(ss >> minorCount)) {
        if (outError) *outError = "Malformed minorStates count";
        return false;
    }
    if (minorCount > DiplomacyState::MaxMinorStates) {
        if (outError) *outError = "Minor states count exceeds maximum limit";
        return false;
    }

    std::set<int> seenMinorIds;
    for (size_t i = 0; i < minorCount; ++i) {
        MinorDiplomacyState m;
        std::string rawName;
        if (!(ss >> token) || token != "m" ||
            !(ss >> m.minorId >> m.systemId >> m.membershipOwner >> m.membershipTurn >> rawName))
        {
            if (outError) *outError = "Malformed minor state row";
            return false;
        }
        if (m.minorId <= 0 || m.systemId <= 0 || m.membershipOwner < 0) {
            if (outError) *outError = "Invalid IDs in minor state";
            return false;
        }
        if (seenMinorIds.find(m.minorId) != seenMinorIds.end()) {
            if (outError) *outError = "Duplicate minorId in minorStates";
            return false;
        }
        seenMinorIds.insert(m.minorId);

        if (version >= 2) {
            if (!decodeString(rawName, m.raceName, outError)) {
                return false;
            }
        } else {
            std::replace(rawName.begin(), rawName.end(), '_', ' ');
            m.raceName = rawName;
        }

        // Attitudes
        size_t attCount = 0;
        if (!(ss >> token) || token != "attitudes" || !(ss >> attCount)) {
            if (outError) *outError = "Malformed minor attitudes count";
            return false;
        }
        if (attCount > 64) {
            if (outError) *outError = "Minor attitude count exceeds maximum limit";
            return false;
        }
        for (size_t a = 0; a < attCount; ++a) {
            int emp = 0, val = 0;
            if (!(ss >> emp >> val) || val < 0 || val > 100 || emp <= 0) {
                if (outError) *outError = "Malformed or out-of-bounds attitude value";
                return false;
            }
            m.attitudes[emp] = val;
        }

        // Contacts
        size_t conCount = 0;
        if (!(ss >> token) || token != "contacts" || !(ss >> conCount)) {
            if (outError) *outError = "Malformed minor contacts count";
            return false;
        }
        if (conCount > 64) {
            if (outError) *outError = "Minor contacts count exceeds maximum limit";
            return false;
        }
        for (size_t c = 0; c < conCount; ++c) {
            int emp = 0, val = 0;
            if (!(ss >> emp >> val) || (val != 0 && val != 1) || emp <= 0) {
                if (outError) *outError = "Malformed contact entry";
                return false;
            }
            m.contacts[emp] = (val != 0);
        }

        // Friendships
        size_t frCount = 0;
        if (!(ss >> token) || token != "friendships" || !(ss >> frCount)) {
            if (outError) *outError = "Malformed minor friendships count";
            return false;
        }
        if (frCount > 64) {
            if (outError) *outError = "Minor friendships count exceeds maximum limit";
            return false;
        }
        for (size_t f = 0; f < frCount; ++f) {
            int emp = 0, val = 0;
            if (!(ss >> emp >> val) || emp <= 0) {
                if (outError) *outError = "Malformed friendship turn entry";
                return false;
            }
            m.friendshipTurns[emp] = val;
        }

        temp.minorStates.push_back(m);
    }

    // Event History
    size_t eventCount = 0;
    if (!(ss >> token) || token != "eventHistory" || !(ss >> eventCount)) {
        if (outError) *outError = "Malformed eventHistory count";
        return false;
    }
    if (eventCount > DiplomacyState::MaxEventHistory) {
        if (outError) *outError = "Event history count exceeds maximum limit";
        return false;
    }

    for (size_t i = 0; i < eventCount; ++i) {
        DiplomaticEvent e;
        int eType = 0, aMinor = 0, bMinor = 0;
        std::string desc;

        if (!(ss >> token) || token != "e" ||
            !(ss >> e.turn >> eType
                 >> e.partyA.id >> aMinor
                 >> e.partyB.id >> bMinor
                 >> e.numericValue >> desc))
        {
            if (outError) *outError = "Malformed event history row";
            return false;
        }

        if (!isValidEventType(eType)) {
            if (outError) *outError = "Invalid EventType enum value";
            return false;
        }
        if ((aMinor != 0 && aMinor != 1) || (bMinor != 0 && bMinor != 1)) {
            if (outError) *outError = "Invalid boolean in event row";
            return false;
        }

        if (version >= 2) {
            if (!decodeString(desc, e.description, outError)) {
                return false;
            }
        } else {
            std::replace(desc.begin(), desc.end(), '_', ' ');
            e.description = desc;
        }
        e.type = static_cast<DiplomaticEventType>(eType);
        e.partyA.isMinor = (aMinor != 0);
        e.partyB.isMinor = (bMinor != 0);

        temp.eventHistory.push_back(e);
    }

    if (!(ss >> token) || token != "END_BOTF_DIPLOMACY") {
        if (outError) *outError = "Missing END_BOTF_DIPLOMACY terminal token";
        return false;
    }

    std::string trailing;
    if (ss >> trailing) {
        if (outError) *outError = "Unexpected trailing tokens after END_BOTF_DIPLOMACY";
        return false;
    }

    outState = std::move(temp);
    return true;
}

} // namespace botf::diplomacy
