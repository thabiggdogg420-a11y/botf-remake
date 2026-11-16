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
// - MODERN (remaster): faction personalities, unilateral war, betrayal & calls to arms,
//   AI-initiated proposals, war assessment, elimination, diplomatic victory predicate.
//   All scoring is integer-only. Rules and numbers: remaster/docs/DIPLOMACY.md.
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
    Superseded = 3,
    Dissolved = 4 // Party eliminated, minor seceded, or alliance cancelled after a refused call to arms
};

inline bool isValidTreatyStatus(int val) {
    return val >= 0 && val <= 4;
}

inline const char* treatyStatusName(TreatyStatus status) {
    switch (status) {
        case TreatyStatus::Active: return "Active";
        case TreatyStatus::Expired: return "Expired";
        case TreatyStatus::BrokenByWar: return "Broken by War";
        case TreatyStatus::Superseded: return "Superseded";
        case TreatyStatus::Dissolved: return "Dissolved";
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
    RecordEvent = 7,
    AdjustTrust = 8 // Major-major trust delta (attitudeDelta) applied symmetrically to the campaign relation
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
    GiftDelivered = 11,
    // Appended for modern diplomacy (values are persisted; never renumber):
    TreatyBetrayed = 12,       // Aggressor declared war while bound by a treaty (galaxy-wide trust penalty)
    AllianceCalledToArms = 13, // Ally asked to join a defensive war
    AllianceHonored = 14,
    AllianceRefused = 15,
    MinorSeceded = 16,
    ParticipantEliminated = 17,
    TributeDemanded = 18
};

inline bool isValidEventType(int val) {
    return val >= 0 && val <= 18;
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
        case DiplomaticEventType::TreatyBetrayed: return "Treaty Betrayed";
        case DiplomaticEventType::AllianceCalledToArms: return "Alliance Called to Arms";
        case DiplomaticEventType::AllianceHonored: return "Alliance Honored";
        case DiplomaticEventType::AllianceRefused: return "Alliance Refused";
        case DiplomaticEventType::MinorSeceded: return "Minor Seceded";
        case DiplomaticEventType::ParticipantEliminated: return "Participant Eliminated";
        case DiplomaticEventType::TributeDemanded: return "Tribute Demanded";
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
// Major Personalities (campaign empire slots 1..5)
// -----------------------------------------------------------------------------

enum class MajorPersonality : int {
    Federation = 0, // Cooperative: low treaty thresholds, honors alliances, rarely starts wars
    Klingon = 1,    // Honor & aggression: respects strength, detests demands, never breaks its word
    Romulan = 2,    // Suspicion: demands more trust for everything, breaks pacts when overwhelming
    Cardassian = 3, // Order & expansion: demands tribute from the weak, expands by force
    Ferengi = 4,    // Profit: credits and trade pacts decide almost everything
    Dominion = 5,   // Control: the Founders' order, tribute from the weak, treaties only on its terms (schema 17)
    Borg = 6,       // Assimilation: cold, demanding, rarely friendly (schema 17; the full collective rules are lane MOD-5)
    Breen = 7       // Secretive confederacy: generic profile until its own lane (schema 17)
};

inline bool isValidPersonality(int val) {
    return val >= 0 && val <= 7;
}

inline const char* personalityName(MajorPersonality personality) {
    switch (personality) {
        case MajorPersonality::Federation: return "Federation";
        case MajorPersonality::Klingon: return "Klingon";
        case MajorPersonality::Romulan: return "Romulan";
        case MajorPersonality::Cardassian: return "Cardassian";
        case MajorPersonality::Ferengi: return "Ferengi";
        case MajorPersonality::Dominion: return "Dominion";
        case MajorPersonality::Borg: return "Borg";
        case MajorPersonality::Breen: return "Breen";
        default: return "Unknown";
    }
}

// Campaign empire slots follow the race order: 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi, 6 Dominion,
// 7 Borg, 8 Breen. Unknown slots behave like the Federation.
inline MajorPersonality personalityForEmpire(int empireId) {
    return (empireId >= 1 && empireId <= 8) ? static_cast<MajorPersonality>(empireId - 1)
                                            : MajorPersonality::Federation;
}

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
    // Optional campaign knowledge. Defaults keep standalone harnesses working unchanged.
    virtual MajorPersonality personality(ParticipantId p) const {
        return p.isMinor ? MajorPersonality::Federation : personalityForEmpire(p.id);
    }
    // Eliminated participants can neither negotiate nor hold treaties.
    virtual bool isEliminated(ParticipantId) const { return false; }
    // Surviving major empire IDs (ascending); used for shared-enemy reasoning.
    virtual std::vector<int> majorEmpires() const { return {}; }
    // Turns since the current war between a and b began (0 when at peace or unknown).
    virtual int warDurationTurns(ParticipantId, ParticipantId) const { return 0; }
    // Rivalry (PLAYTEST-FEEDBACK 16 Nov 2026, "no war"). Campaigns on the modern engines answer true: border friction, expansion and
    // opportunity then raise `rivalryPressure` (0..50, from `self`'s point of view), and an AI that is leaning towards war with a
    // neighbour will neither offer nor accept a cooperation treaty with it. Standalone harnesses keep the old rules (false, 0).
    virtual bool rivalryEnabled() const { return false; }
    virtual int rivalryPressure(ParticipantId, ParticipantId) const { return 0; }
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
    std::map<ParticipantId, MajorPersonality> personalities;
    std::set<ParticipantId> eliminated;
    std::vector<int> majors;
    std::map<std::pair<ParticipantId, ParticipantId>, int> warDurations;

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
    void setPersonality(ParticipantId p, MajorPersonality personality) {
        personalities[p] = personality;
    }
    void setEliminated(ParticipantId p, bool val) {
        if (val) eliminated.insert(p); else eliminated.erase(p);
    }
    void setWarDuration(ParticipantId a, ParticipantId b, int turns) {
        warDurations[{a, b}] = turns;
        warDurations[{b, a}] = turns;
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
    MajorPersonality personality(ParticipantId p) const override {
        auto it = personalities.find(p);
        return (it != personalities.end()) ? it->second : DiplomacyContext::personality(p);
    }
    bool isEliminated(ParticipantId p) const override {
        return eliminated.count(p) > 0;
    }
    std::vector<int> majorEmpires() const override {
        return majors;
    }
    int warDurationTurns(ParticipantId a, ParticipantId b) const override {
        auto it = warDurations.find({a, b});
        return (it != warDurations.end()) ? it->second : 0;
    }
};

// -----------------------------------------------------------------------------
// AI Personality & Decision Models
// -----------------------------------------------------------------------------

struct AiDecisionResult {
    AiDecision decision = AiDecision::Reject;
    int score = 0;
    int threshold = 50;
    std::string explanation;
};

// -----------------------------------------------------------------------------
// Diplomatic Memory (modern rules; persisted as optional save sections)
// -----------------------------------------------------------------------------

// Per unordered major pair (first < second). Remembers what AI empires need to
// behave consistently across turns: war/peace transitions for cool-downs, the
// last proposal each side made (anti-spam), and grievances each side holds.
struct RelationMemory {
    int first = 0;
    int second = 0;
    int lastWarTurn = -1;
    int lastPeaceTurn = -1;
    int lastProposalFirstToSecond = -1;
    int lastProposalSecondToFirst = -1;
    int grievancesOfFirst = 0;  // first's grievances against second
    int grievancesOfSecond = 0; // second's grievances against first
    int refusalsFirstToSecond = 0; // Consecutive proposals from first that second refused or ignored
    int refusalsSecondToFirst = 0;

    bool operator==(const RelationMemory& other) const = default;

    int refusalsFrom(int empireId) const {
        return empireId == first ? refusalsFirstToSecond : empireId == second ? refusalsSecondToFirst : 0;
    }

    int grievancesOf(int empireId) const {
        return empireId == first ? grievancesOfFirst : empireId == second ? grievancesOfSecond : 0;
    }
    int lastProposalFrom(int empireId) const {
        return empireId == first ? lastProposalFirstToSecond : empireId == second ? lastProposalSecondToFirst : -1;
    }
};

// Galaxy-wide reputation of one major.
struct EmpireStanding {
    int empireId = 0;
    int betrayals = 0;          // Wars declared while bound by a treaty
    int lastBetrayalTurn = -1;

    bool operator==(const EmpireStanding& other) const = default;
};

// An ally of `defender` is expected to be at war with `aggressor` by `dueTurn`.
struct DefenseCall {
    int ally = 0;
    int defender = 0;
    int aggressor = 0;
    int issuedTurn = 0;
    int dueTurn = 0;

    bool operator==(const DefenseCall& other) const = default;
};

// -----------------------------------------------------------------------------
// Diplomacy State
// -----------------------------------------------------------------------------

struct DiplomacyState {
    static constexpr size_t MaxPendingProposals = 128;   // Stored proposals (pending + resolved this turn)
    static constexpr size_t MaxPendingPerProposer = 16;  // Anti-spam: open proposals per proposer
    static constexpr size_t MaxActiveTreaties = 128;
    static constexpr size_t MaxTreatyHistory = 256;
    static constexpr size_t MaxEventHistory = 256;
    static constexpr size_t MaxMinorStates = 64;
    static constexpr size_t MaxRelationMemory = 32;
    static constexpr size_t MaxStandings = 16;
    static constexpr size_t MaxDefenseCalls = 64;
    static constexpr int MaxGrievances = 20;
    static constexpr int MaxRefusals = 5;
    static constexpr int DefenseCallWindow = 3;          // Turns an ally has to join a defensive war

    int currentTurn = 1;
    uint64_t nextProposalId = 1;
    uint64_t nextTreatyId = 1;

    std::vector<Proposal> proposals;
    std::vector<ActiveTreaty> activeTreaties;
    std::vector<ActiveTreaty> treatyHistory;
    std::vector<MinorDiplomacyState> minorStates;
    std::vector<DiplomaticEvent> eventHistory;
    std::vector<RelationMemory> relationMemory; // Sorted by (first, second)
    std::vector<EmpireStanding> standings;      // Sorted by empireId
    std::vector<DefenseCall> defenseCalls;

    bool operator==(const DiplomacyState& other) const = default;

    const RelationMemory* findMemory(int a, int b) const {
        if (a > b) std::swap(a, b);
        for (const auto& m : relationMemory) {
            if (m.first == a && m.second == b) return &m;
        }
        return nullptr;
    }

    // Returns nullptr only when the bounded memory table is full.
    RelationMemory* memory(int a, int b) {
        if (a == b || a <= 0 || b <= 0) return nullptr;
        if (a > b) std::swap(a, b);
        for (auto& m : relationMemory) {
            if (m.first == a && m.second == b) return &m;
        }
        if (relationMemory.size() >= MaxRelationMemory) return nullptr;
        RelationMemory m;
        m.first = a;
        m.second = b;
        auto pos = std::lower_bound(relationMemory.begin(), relationMemory.end(), m,
            [](const RelationMemory& x, const RelationMemory& y) {
                return x.first != y.first ? x.first < y.first : x.second < y.second;
            });
        return &*relationMemory.insert(pos, m);
    }

    int grievances(int holder, int against) const {
        const auto* m = findMemory(holder, against);
        return m ? m->grievancesOf(holder) : 0;
    }

    // Tracks consecutive refusals of `proposer`'s proposals by `target` (AI back-off).
    void noteProposalOutcome(int proposer, int target, bool accepted) {
        auto* m = memory(proposer, target);
        if (!m) return;
        int& r = (proposer == m->first) ? m->refusalsFirstToSecond : m->refusalsSecondToFirst;
        r = accepted ? 0 : std::min(MaxRefusals, r + 1);
    }

    void addGrievance(int holder, int against, int amount) {
        auto* m = memory(holder, against);
        if (!m) return;
        int& g = (holder == m->first) ? m->grievancesOfFirst : m->grievancesOfSecond;
        g = std::clamp(g + amount, 0, MaxGrievances);
    }

    const EmpireStanding* findStanding(int empireId) const {
        for (const auto& st : standings) {
            if (st.empireId == empireId) return &st;
        }
        return nullptr;
    }

    EmpireStanding* standing(int empireId) {
        if (empireId <= 0) return nullptr;
        for (auto& st : standings) {
            if (st.empireId == empireId) return &st;
        }
        if (standings.size() >= MaxStandings) return nullptr;
        EmpireStanding st;
        st.empireId = empireId;
        auto pos = std::lower_bound(standings.begin(), standings.end(), st,
            [](const EmpireStanding& x, const EmpireStanding& y) { return x.empireId < y.empireId; });
        return &*standings.insert(pos, st);
    }

    int betrayals(int empireId) const {
        const auto* st = findStanding(empireId);
        return st ? st->betrayals : 0;
    }

    size_t unresolvedProposalCount() const {
        return static_cast<size_t>(std::count_if(proposals.begin(), proposals.end(),
            [](const Proposal& p) { return !p.resolved; }));
    }

    // Highest active cooperation rank (NAP 1 .. Alliance 4) between two parties; 0 if none.
    int highestTreatyRank(ParticipantId a, ParticipantId b) const {
        int best = 0;
        for (const auto& t : activeTreaties) {
            if (t.status == TreatyStatus::Active && t.involvesBoth(a, b)) best = std::max(best, getTreatyRank(t.type));
        }
        return best;
    }

    bool hasActiveTreaty(ParticipantId a, ParticipantId b, TreatyType type) const {
        return findActiveTreaty(a, b, type) != nullptr;
    }

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

    // 3. Eliminated participants cannot negotiate
    if (ctx.isEliminated(p.proposer) || ctx.isEliminated(p.target)) {
        if (outReason) *outReason = "Eliminated empires cannot conduct diplomacy";
        return false;
    }

    // 3b. Contact requirement
    if (!ctx.hasContact(p.proposer, p.target)) {
        if (outReason) *outReason = "Target is uncontacted";
        return false;
    }

    // 4. Bounded capacity check. Only open proposals count: resolved ones are
    // purged before new submissions, so the table can never lock up permanently.
    if (state.unresolvedProposalCount() >= DiplomacyState::MaxPendingProposals) {
        if (outReason) *outReason = "Pending proposal queue limit reached";
        return false;
    }
    size_t openByProposer = 0;
    for (const auto& existing : state.proposals) {
        if (!existing.resolved && existing.proposer == p.proposer) ++openByProposer;
    }
    if (openByProposer >= DiplomacyState::MaxPendingPerProposer) {
        if (outReason) *outReason = "Too many open proposals from this proposer";
        return false;
    }

    // 5. Duplicate pending proposal check
    for (const auto& existing : state.proposals) {
        if (existing.status == ProposalStatus::Pending && !existing.resolved &&
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
            // War is a unilateral declaration: it takes effect at turn resolution
            // whatever the target says, so it cannot carry payments.
            if (p.proposer.isMinor || p.target.isMinor) {
                if (outReason) *outReason = "War declarations are only between major empires";
                return false;
            }
            if (currentlyAtWar) {
                if (outReason) *outReason = "Belligerents are already at war";
                return false;
            }
            if (p.creditOffer != 0 || p.creditDemand != 0) {
                if (outReason) *outReason = "War declarations cannot carry credit terms";
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
            if (p.proposer.isMinor || (p.target.isMinor && !ctx.isIndependentMinor(p.target.id))) {
                if (outReason) *outReason = "Treaties with minors require an independent minor as target";
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
            if (p.proposer.isMinor || (p.target.isMinor && !ctx.isIndependentMinor(p.target.id))) {
                if (outReason) *outReason = "Trade pacts with minors require an independent minor as target";
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
            // Standalone gift (creditOffer) or tribute demand (creditDemand).
            if (p.creditOffer <= 0 && p.creditDemand <= 0) {
                if (outReason) *outReason = "A gift or tribute must name a credit amount";
                return false;
            }
            if (p.creditDemand > 0 && (p.target.isMinor || p.proposer.isMinor)) {
                if (outReason) *outReason = "Tribute can only be demanded from a major empire";
                return false;
            }
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
// Deterministic Integer Helpers
// -----------------------------------------------------------------------------
//
// All AI scoring is integer-only so identical inputs produce identical results
// on every compiler and platform (no floating point anywhere in this header).

inline uint64_t diplomacyMix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

// Uniform integer in [-span, +span] from (seed, turn, salt).
inline int deterministicVariance(uint64_t seed, int turn, uint64_t salt, int span = 5) {
    if (span <= 0) return 0;
    const uint64_t h = diplomacyMix(seed ^ diplomacyMix(static_cast<uint64_t>(static_cast<int64_t>(turn)) ^ diplomacyMix(salt)));
    return static_cast<int>(h % static_cast<uint64_t>(2 * span + 1)) - span;
}

// Power comparison without division or floating point: returns 1 when a > factor*b
// (a clearly stronger), -1 when a*factor < b (a clearly weaker), 0 otherwise.
inline int comparePower(int64_t a, int64_t b, int64_t factor) {
    a = std::clamp<int64_t>(a, 0, int64_t{1} << 40);
    b = std::clamp<int64_t>(b, 0, int64_t{1} << 40);
    if (a > b * factor) return 1;
    if (a * factor < b) return -1;
    return 0;
}

// a/b expressed in percent, saturating at 1000% and safe for zero denominators.
inline int64_t powerRatioPercent(int64_t a, int64_t b) {
    a = std::clamp<int64_t>(a, 0, int64_t{1} << 40);
    b = std::clamp<int64_t>(b, 0, int64_t{1} << 40);
    if (b <= 0) return a > 0 ? 1000 : 100;
    return std::min<int64_t>(1000, (a * 100) / b);
}

// -----------------------------------------------------------------------------
// AI Evaluation Function
// -----------------------------------------------------------------------------
//
// Personalities (see remaster/docs/DIPLOMACY.md for the full tables):
// - Federation: cooperation. Lowest thresholds for cooperative treaties and peace.
// - Klingon: honor/aggression. Respects strength, punishes weakness and demands,
//   slow to accept peace, never pays tribute unless overwhelmed.
// - Romulan: suspicion. Every threshold is higher; loves nothing but itself.
// - Cardassian: order/expansion. Respects strength, resents demands, wants tribute.
// - Ferengi: profit. Credits count for far more; trade pacts come cheap.

struct PersonalityThresholds {
    int peace, nonAggression, friendship, affiliation, alliance, trade, tribute;
};

inline PersonalityThresholds thresholdsFor(MajorPersonality personality) {
    switch (personality) {
        case MajorPersonality::Federation: return {40, 40, 50, 60, 70, 45, 50};
        case MajorPersonality::Klingon:    return {60, 55, 60, 70, 75, 55, 75};
        case MajorPersonality::Romulan:    return {55, 55, 65, 75, 85, 55, 60};
        case MajorPersonality::Cardassian: return {50, 50, 60, 70, 80, 50, 65};
        case MajorPersonality::Ferengi:    return {45, 45, 55, 65, 80, 35, 60};
        case MajorPersonality::Dominion:   return {55, 55, 70, 80, 90, 50, 60};
        case MajorPersonality::Borg:       return {70, 65, 85, 95, 99, 70, 90};
        default: return {50, 45, 55, 65, 75, 50, 55};
    }
}

inline int countSharedEnemies(const DiplomacyContext& ctx, ParticipantId a, ParticipantId b) {
    if (a.isMinor || b.isMinor) return 0;
    int shared = 0;
    for (int id : ctx.majorEmpires()) {
        const ParticipantId z = ParticipantId::major(id);
        if (z == a || z == b || ctx.isEliminated(z)) continue;
        if (ctx.isAtWar(a, z) && ctx.isAtWar(b, z)) ++shared;
    }
    return shared;
}

struct PersonalityProfile {
    int aggression;          // War appetite added to every war score
    int maxWars;             // Will not open another front beyond this many wars
    int minWarTurns;         // Fights at least this long before seeking peace
    int trustBaseline;       // Peacetime trust drifts toward this value (modern rules)
    int napDesire;           // Desire needed before proposing a non-aggression pact
    int friendshipDesire;
    int affiliationDesire;
    int allianceDesire;
    int tradeDesire;
    bool treacherous;        // May break a treaty when overwhelmingly stronger
    bool demandsTribute;     // Extorts much weaker neighbours
    bool givesGifts;         // Sends goodwill gifts to improve relations
    int pactCeiling;         // Rivalry: at this war score or above the AI neither offers nor accepts a cooperation treaty
};

inline PersonalityProfile profileFor(MajorPersonality personality) {
    switch (personality) {
        case MajorPersonality::Federation: return {0, 1, 3, 60, 45, 55, 65, 75, 45, false, false, true, 100};
        case MajorPersonality::Klingon:    return {45, 3, 15, 40, 60, 65, 75, 80, 60, false, true, false, 40};
        case MajorPersonality::Romulan:    return {25, 2, 10, 35, 55, 70, 85, 95, 55, true, false, false, 45};
        case MajorPersonality::Cardassian: return {40, 2, 12, 40, 55, 65, 75, 85, 50, true, true, false, 40};
        case MajorPersonality::Ferengi:    return {10, 1, 4, 50, 50, 60, 70, 85, 30, false, false, false, 55};
        case MajorPersonality::Dominion:   return {45, 3, 12, 35, 50, 65, 80, 90, 50, true, true, false, 35};
        case MajorPersonality::Borg:       return {40, 3, 15, 40, 70, 80, 95, 99, 40, false, true, false, 20};
        default: return {0, 1, 3, 50, 50, 60, 70, 80, 45, false, false, false, 100};
    }
}

// How a personality turns the map into war pressure (percent weights; see rivalryPressure in Simulation.cpp).
struct RivalryProfile {
    int territorial;   // Border friction: its colonies near the other's colonies and ships
    int expansionist;  // Resentment of a neighbour that holds more colonies than it does
    int opportunist;   // Smells weakness: a neighbour much weaker than itself, or already at war with a third power
    int profit;        // Mercenary: a rich neighbour it has no trade pact with
    int patience;      // Turns beyond the early-game peace before it will open a war (plus a seeded 0..8 per pair)
};

inline RivalryProfile rivalryProfileFor(MajorPersonality personality) {
    switch (personality) {
        case MajorPersonality::Federation: return {0, 0, 0, 0, 0};
        case MajorPersonality::Klingon:    return {100, 80, 40, 0, 0};     // Honour is won at the border
        case MajorPersonality::Romulan:    return {80, 80, 100, 0, 12};     // Opportunist: waits for the weak or the distracted
        case MajorPersonality::Cardassian: return {100, 150, 40, 0, 5};    // Expansionist: resents every colony that is not its own
        case MajorPersonality::Ferengi:    return {40, 40, 0, 100, 20};     // Mercenary: friendly while the profit is
        case MajorPersonality::Dominion:   return {100, 100, 60, 0, 0};    // Hostile: everything near it is its to order
        case MajorPersonality::Borg:       return {100, 100, 0, 0, 0};     // Hostile: everything near it is to be assimilated
        default: return {40, 40, 0, 0, 10};
    }
}

// The pure war score (variance excluded): personality, trust, relative power, grievances, betrayals, restlessness,
// trade ties and rivalry pressure. assessWarDeclaration adds the variance; the treaty planner and the proposal
// evaluator read the same number, so an AI that is about to fight will not sign a pact.
inline int hostilityScore(const PersonalityProfile& prof, int currentTurn, int trust, int64_t ratioPercent, int grievances,
                          int targetBetrayals, bool tradePact, bool ferengi, int pressure) {
    int score = prof.aggression + (50 - trust) + 6 * std::min(grievances, 10) + std::min(15, 5 * targetBetrayals);
    if (ratioPercent >= 100) score += static_cast<int>(std::min<int64_t>(30, (ratioPercent - 100) / 5));
    else score -= static_cast<int>(std::min<int64_t>(40, (100 - ratioPercent) / 2));
    if (prof.aggression >= 20) {
        // Restlessness: an idle warlord looks for a war, but not one it cannot win (full at 60% of the target's power, half from 40%).
        const int restless = std::min(30, std::max(0, currentTurn - 25));
        const int fromPower = ratioPercent >= 60 ? restless : ratioPercent >= 40 ? restless / 2 : 0;
        score += trust <= 50 ? fromPower : trust <= 70 ? fromPower / 2 : 0; // ...nor one against a neighbour it trusts
    }
    if (tradePact) score -= ferengi ? 20 : 10;
    return score + pressure;
}

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
    expl << personalityName(personality) << " evaluation: ";

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

    // 2. Power Balance (integer comparison; no floating point)
    const int64_t proposerMil = ctx.militaryPower(p.proposer);
    const int64_t targetMil = ctx.militaryPower(p.target);
    if (targetMil > 0) {
        const int cmp = comparePower(proposerMil, targetMil, 2);
        int strongBonus = 10, weakPenalty = 10;
        if (personality == MajorPersonality::Klingon) { strongBonus = 15; weakPenalty = 20; }
        else if (personality == MajorPersonality::Cardassian) { strongBonus = 10; weakPenalty = 15; }
        if (cmp > 0) {
            score += strongBonus;
            expl << "+ Proposer superior fleet (+" << strongBonus << ") ";
        } else if (cmp < 0) {
            score -= weakPenalty;
            expl << "- Proposer inferior fleet (-" << weakPenalty << ") ";
        }
    }

    // 3. Tone Modifiers
    switch (p.tone) {
        case DiplomaticTone::Cordial: {
            const int bonus = (personality == MajorPersonality::Klingon || personality == MajorPersonality::Romulan) ? 5 : 10;
            score += bonus;
            expl << "+ Cordial tone (+" << bonus << ") ";
            break;
        }
        case DiplomaticTone::Neutral:
            expl << "+ Neutral tone (+0) ";
            break;
        case DiplomaticTone::Demanding:
            if (personality == MajorPersonality::Klingon || personality == MajorPersonality::Cardassian) {
                score -= 25;
                expl << "- Demanding tone resisted (-25) ";
            } else if (personality == MajorPersonality::Ferengi) {
                score -= 10;
                expl << "- Demanding tone tolerated (-10) ";
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
        if (personality == MajorPersonality::Ferengi) {
            penalty = static_cast<int>(std::min<int64_t>(60, p.creditDemand / 40));
        }
        score -= penalty;
        expl << "- Demanded tribute (-" << penalty << ") ";
    }

    // 5. History: treaty-breakers and old grievances are remembered
    if (!p.proposer.isMinor) {
        const int betrayals = state.betrayals(p.proposer.id);
        if (betrayals > 0) {
            const int penalty = std::min(30, betrayals * 10);
            score -= penalty;
            expl << "- Known treaty-breaker (-" << penalty << ") ";
        }
        if (!p.target.isMinor) {
            const int grievances = state.grievances(p.target.id, p.proposer.id);
            if (grievances > 0) {
                const int penalty = std::min(15, grievances * 3);
                score -= penalty;
                expl << "- Grievances (-" << penalty << ") ";
            }
        }
    }

    // 6. Shared enemies bind empires together
    const bool cooperative = getTreatyRank(p.treatyType) > 0;
    if (cooperative && !p.target.isMinor && !p.proposer.isMinor) {
        const int shared = countSharedEnemies(ctx, p.proposer, p.target);
        if (shared > 0) {
            const int bonus = std::min(20, shared * 10);
            score += bonus;
            expl << "+ Shared enemies (+" << bonus << ") ";
        }
    }

    // 6b. Rivalry: an AI that is leaning towards war with the proposer will not bind its hands (modern campaigns only).
    if (cooperative && !p.target.isMinor && !p.proposer.isMinor && ctx.rivalryEnabled()) {
        const PersonalityProfile prof = profileFor(personality);
        const int hostility = hostilityScore(prof, state.currentTurn, ctx.trust(p.target, p.proposer),
            powerRatioPercent(targetMil, proposerMil), state.grievances(p.target.id, p.proposer.id), state.betrayals(p.proposer.id),
            state.hasActiveTreaty(p.target, p.proposer, TreatyType::Trade), personality == MajorPersonality::Ferengi,
            ctx.rivalryPressure(p.target, p.proposer));
        if (hostility >= prof.pactCeiling) {
            const int penalty = hostility - prof.pactCeiling + 15;
            score -= penalty;
            expl << "- Wants a war, not a pact (-" << penalty << ") ";
        }
    }

    // 7. Treaty Type Demands & Personalities
    const PersonalityThresholds th = thresholdsFor(personality);
    switch (p.treatyType) {
        case TreatyType::Peace: {
            res.threshold = th.peace;
            const int weariness = std::min(20, std::max(0, ctx.warDurationTurns(p.proposer, p.target)) / 2);
            if (weariness > 0) {
                score += weariness;
                expl << "+ War weariness (+" << weariness << ") ";
            }
            break;
        }
        case TreatyType::NonAggression: {
            res.threshold = th.nonAggression;
            // A Klingon only binds its hands for a stronger partner.
            if (personality == MajorPersonality::Klingon && targetMil > 0 && comparePower(proposerMil, targetMil, 1) <= 0) {
                res.threshold += 5;
            }
            break;
        }
        case TreatyType::Friendship: res.threshold = p.target.isMinor ? 50 : th.friendship; break;
        case TreatyType::Affiliation: res.threshold = p.target.isMinor ? 60 : th.affiliation; break;
        case TreatyType::Alliance: res.threshold = th.alliance; break;
        case TreatyType::Membership: res.threshold = 80; break;
        case TreatyType::Trade: res.threshold = p.target.isMinor ? 45 : th.trade; break;
        case TreatyType::None: {
            if (p.creditDemand > 0) {
                res.threshold = th.tribute;
            } else {
                res.threshold = 0; // Unconditional gifts are always welcome
            }
            break;
        }
        case TreatyType::War: {
            res.threshold = 0; // Unilateral; never actually evaluated
            break;
        }
        default:
            break;
    }

    // 8. Deterministic Pseudo-Random Variance (Seed + Turn + Proposal ID)
    const int variance = deterministicVariance(seed, state.currentTurn, p.id * 1013904223ULL, 5);
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
// AI War & Alliance Decisions (pure functions; campaign supplies the facts)
// -----------------------------------------------------------------------------

// Mild cultural affinities between the five majors (symmetric), added to desire.
inline int personalityAffinity(MajorPersonality a, MajorPersonality b) {
    auto key = [](MajorPersonality x, MajorPersonality y) {
        int i = static_cast<int>(x), j = static_cast<int>(y);
        if (i > j) std::swap(i, j);
        return i * 10 + j;
    };
    switch (key(a, b)) {
        case 1: return 0;    // Federation-Klingon
        case 2: return -10;  // Federation-Romulan
        case 3: return -5;   // Federation-Cardassian
        case 4: return 5;    // Federation-Ferengi
        case 12: return -15; // Klingon-Romulan
        case 13: return -5;  // Klingon-Cardassian
        case 14: return -5;  // Klingon-Ferengi
        case 23: return 0;   // Romulan-Cardassian
        case 24: return 0;   // Romulan-Ferengi
        case 34: return 5;   // Cardassian-Ferengi
        case 5: return -10;  // Federation-Dominion
        case 15: return -15; // Klingon-Dominion
        case 25: return -5;  // Romulan-Dominion
        case 35: return 15;  // Cardassian-Dominion (allies in the war)
        case 6: case 16: case 26: case 36: case 46: case 56: return -20; // Everyone-Borg
        default: return 0;
    }
}

struct WarAssessmentInput {
    int aggressorId = 0;
    int targetId = 0;
    MajorPersonality personality = MajorPersonality::Federation;
    int currentTurn = 0;
    int turnsSinceTransition = 1000; // Turns since the last war/peace change between the pair
    int activeWars = 0;              // Aggressor's current wars
    int64_t aggressorPower = 0;
    int64_t targetPower = 0;         // Including the target's allies
    int trust = 50;
    int treatyRank = 0;              // Highest cooperation treaty rank in force
    bool peaceTreaty = false;        // A timed peace treaty is in force
    bool tradePact = false;
    int grievances = 0;              // Aggressor's grievances against the target
    int targetBetrayals = 0;
    int pressure = 0;                // Rivalry pressure (border friction, expansion, opportunity), 0..50
    uint64_t seed = 0;
};

struct WarAssessment {
    bool declare = false;
    bool breaksTreaty = false;
    int score = 0;
    int threshold = 60;
    std::string rationale;
};

inline constexpr int WarCooldownTurns = 20;  // No re-declaration within this many turns of peace
inline constexpr int EarlyPeaceTurns = 30;   // No AI-initiated wars before this campaign turn

inline WarAssessment assessWarDeclaration(const WarAssessmentInput& in) {
    WarAssessment out;
    const PersonalityProfile prof = profileFor(in.personality);
    std::ostringstream why;
    why << personalityName(in.personality) << " war assessment vs " << in.targetId << ": ";

    // Early-game peace, then each personality's patience: the Klingons and the Borg are ready at once, the Cardassians a little later,
    // the Romulans bide their time and the Ferengi wait for a profit. A seeded 0..8 turns spreads one empire's wars between its neighbours.
    const int patience = rivalryProfileFor(in.personality).patience +
        2 * std::abs(deterministicVariance(in.seed, 0, static_cast<uint64_t>(in.aggressorId) * 131ULL + static_cast<uint64_t>(in.targetId) * 7919ULL, 4));
    if (in.currentTurn < EarlyPeaceTurns + patience) {
        out.rationale = why.str() + "early-game peace";
        return out;
    }
    if (in.turnsSinceTransition < WarCooldownTurns) {
        out.rationale = why.str() + "cool-down after last war/peace transition";
        return out;
    }
    if (in.activeWars >= prof.maxWars) {
        out.rationale = why.str() + "already fighting enough wars";
        return out;
    }
    const int64_t ratio = powerRatioPercent(in.aggressorPower, in.targetPower);
    const bool bound = in.treatyRank > 0 || in.peaceTreaty;
    if (bound) {
        // Honourable empires never break their word; treacherous ones only when
        // overwhelmingly stronger and the relationship is already poisoned.
        if (!prof.treacherous || ratio < 300 || in.trust >= 20) {
            out.rationale = why.str() + "bound by treaty";
            return out;
        }
        out.breaksTreaty = true;
    }
    if (prof.aggression == 0 && (in.grievances < 10 || in.trust >= 15)) {
        out.rationale = why.str() + "does not start wars";
        return out;
    }

    int score = hostilityScore(prof, in.currentTurn, in.trust, ratio, in.grievances, in.targetBetrayals, in.tradePact,
                               in.personality == MajorPersonality::Ferengi, in.pressure);
    score += deterministicVariance(in.seed, in.currentTurn,
        static_cast<uint64_t>(in.aggressorId) * 131ULL + static_cast<uint64_t>(in.targetId) * 7919ULL, 5);

    out.score = score;
    out.threshold = 60;
    out.declare = score >= out.threshold;
    why << "score " << score << " vs " << out.threshold << " (power " << ratio << "%, trust " << in.trust
        << ", grievances " << in.grievances << ", pressure " << in.pressure << ")";
    out.rationale = why.str();
    if (!out.declare) out.breaksTreaty = false;
    return out;
}

// Whether an AI ally answers a call to arms against `aggressor`.
inline bool aiHonorsDefenseCall(MajorPersonality personality, int64_t allyPower, int64_t aggressorPower,
                                bool boundToAggressor) {
    if (boundToAggressor && personality != MajorPersonality::Klingon) return false;
    const int64_t ratio = powerRatioPercent(allyPower, aggressorPower);
    switch (personality) {
        case MajorPersonality::Klingon: return ratio >= 33;     // Honour: joins unless hopeless
        case MajorPersonality::Federation: return ratio >= 50;  // Cooperation: joins unless badly outmatched
        case MajorPersonality::Cardassian: return ratio >= 100;
        case MajorPersonality::Romulan: return ratio >= 150;
        case MajorPersonality::Ferengi: return ratio >= 200;
        default: return ratio >= 100;
    }
}

// -----------------------------------------------------------------------------
// AI-Initiated Diplomacy Planner
// -----------------------------------------------------------------------------

struct AiDiplomaticIntent {
    ParticipantId proposer;
    ParticipantId target;
    TreatyType treatyType = TreatyType::None;
    TreatyDuration duration = TreatyDuration::Turns50;
    DiplomaticTone tone = DiplomaticTone::Cordial;
    int64_t creditOffer = 0;
    int64_t creditDemand = 0;
    std::string rationale;

    bool operator==(const AiDiplomaticIntent& other) const = default;
};

inline constexpr int AiProposalCooldownTurns = 6;   // Between AI proposals to the same AI empire
inline constexpr int AiHumanProposalCooldownTurns = 8; // Between AI proposals to the human player
inline constexpr int AiDemandCooldownTurns = 15;
inline constexpr int AiMaxIntentsPerTurn = 2;

// Desire of `self` for closer ties with `other` (trust-based, integer).
inline int diplomaticDesire(const DiplomacyState& state, const DiplomacyContext& ctx, ParticipantId self, ParticipantId other) {
    const MajorPersonality selfP = ctx.personality(self);
    const MajorPersonality otherP = ctx.personality(other);
    int desire = ctx.trust(self, other) + personalityAffinity(selfP, otherP);
    desire += 10 * std::min(2, countSharedEnemies(ctx, self, other));
    desire -= 5 * std::min(4, state.grievances(self.id, other.id));
    desire -= 10 * std::min(3, state.betrayals(other.id));
    if (comparePower(ctx.militaryPower(other), ctx.militaryPower(self), 2) > 0) desire += 10; // Fear buys pacts
    return desire;
}

inline int tradePactAge(const DiplomacyState& state, ParticipantId a, ParticipantId b) {
    for (const auto& t : state.activeTreaties)
        if (t.status == TreatyStatus::Active && t.involvesBoth(a, b) && t.type == TreatyType::Trade) return state.currentTurn - t.startTurn;
    return 0;
}

inline int activeTreatyAge(const DiplomacyState& state, ParticipantId a, ParticipantId b, int rank) {
    for (const auto& t : state.activeTreaties) {
        if (t.status == TreatyStatus::Active && t.involvesBoth(a, b) && getTreatyRank(t.type) == rank) {
            return state.currentTurn - t.startTurn;
        }
    }
    return 0;
}

// Plans this turn's AI-initiated proposals (treaties, peace, tribute demands and
// goodwill gifts) for every AI major in `aiEmpires`. Pure and deterministic: the
// caller submits the intents through DiplomacyEngine::submitProposal.
inline std::vector<AiDiplomaticIntent> planAiDiplomacy(
    const DiplomacyState& state,
    const DiplomacyContext& ctx,
    const std::vector<int>& aiEmpires,
    uint64_t seed)
{
    std::vector<AiDiplomaticIntent> intents;
    std::vector<int> majors = ctx.majorEmpires();
    std::sort(majors.begin(), majors.end());
    std::vector<int> planners = aiEmpires;
    std::sort(planners.begin(), planners.end());

    for (int selfId : planners) {
        const ParticipantId self = ParticipantId::major(selfId);
        if (ctx.isEliminated(self) || ctx.isHuman(self)) continue;
        const MajorPersonality selfP = ctx.personality(self);
        const PersonalityProfile prof = profileFor(selfP);
        int planned = 0;

        for (int otherId : majors) {
            if (planned >= AiMaxIntentsPerTurn) break;
            if (otherId == selfId) continue;
            const ParticipantId other = ParticipantId::major(otherId);
            if (ctx.isEliminated(other) || !ctx.hasContact(self, other)) continue;

            // One open proposal per direction at a time, and a cool-down between them.
            const bool open = std::any_of(state.proposals.begin(), state.proposals.end(), [&](const Proposal& p) {
                return !p.resolved && p.proposer == self && p.target == other;
            });
            if (open) continue;
            const auto* mem = state.findMemory(selfId, otherId);
            const int lastProposal = mem ? mem->lastProposalFrom(selfId) : -1;
            // Back-off: every consecutive refusal (or silence) doubles the wait, up to 4x.
            const int refusals = mem ? mem->refusalsFrom(selfId) : 0;
            const int cooldown = (ctx.isHuman(other) ? AiHumanProposalCooldownTurns : AiProposalCooldownTurns) *
                                 (1 << std::min(refusals, 2));
            if (lastProposal >= 0 && state.currentTurn - lastProposal < cooldown) continue;

            AiDiplomaticIntent intent;
            intent.proposer = self;
            intent.target = other;
            const int64_t selfPower = ctx.militaryPower(self);
            const int64_t otherPower = ctx.militaryPower(other);
            const int64_t ratio = powerRatioPercent(selfPower, otherPower);
            const int trust = ctx.trust(self, other);
            const int jitter = deterministicVariance(seed, state.currentTurn,
                static_cast<uint64_t>(selfId) * 6151ULL + static_cast<uint64_t>(otherId) * 97ULL, 3);

            if (ctx.isAtWar(self, other)) {
                const int duration = ctx.warDurationTurns(self, other);
                const bool losing = ratio < 80;
                const bool weary = duration >= 3 * prof.minWarTurns;
                if (duration >= prof.minWarTurns && (losing || weary || trust + jitter >= 35)) {
                    intent.treatyType = TreatyType::Peace;
                    intent.duration = TreatyDuration::Indefinite;
                    intent.tone = DiplomaticTone::Cordial;
                    intent.rationale = losing ? "losing the war" : weary ? "war weariness" : "relations have thawed";
                    intents.push_back(intent);
                    ++planned;
                }
                continue;
            }

            const int desire = diplomaticDesire(state, ctx, self, other) + jitter;
            const int rank = state.highestTreatyRank(self, other);
            const bool hasTrade = state.hasActiveTreaty(self, other, TreatyType::Trade);
            const int shared = countSharedEnemies(ctx, self, other);
            // Rivalry: leaning towards war, the AI keeps its hands free (no new or deeper cooperation treaty).
            const bool warMinded = ctx.rivalryEnabled() &&
                hostilityScore(prof, state.currentTurn, trust, ratio, state.grievances(selfId, otherId), state.betrayals(otherId), hasTrade,
                               selfP == MajorPersonality::Ferengi, ctx.rivalryPressure(self, other)) >= prof.pactCeiling;
            // Mercenary: the Ferengi sign a non-aggression pact only with a customer, a trade partner of ten turns' standing.
            const bool customer = selfP != MajorPersonality::Ferengi || !ctx.rivalryEnabled() ||
                                  (hasTrade && tradePactAge(state, self, other) >= 10);

            // Profit first: Ferengi open with trade.
            if (!hasTrade && selfP == MajorPersonality::Ferengi && desire >= prof.tradeDesire) {
                intent.treatyType = TreatyType::Trade;
                intent.rationale = "profitable trade";
            } else if (rank == 0 && !warMinded && customer && desire >= prof.napDesire) {
                intent.treatyType = TreatyType::NonAggression;
                intent.rationale = "secure the border";
            } else if (rank == 1 && !warMinded && activeTreatyAge(state, self, other, 1) >= 5 && desire >= prof.friendshipDesire) {
                intent.treatyType = TreatyType::Friendship;
                intent.duration = TreatyDuration::Indefinite;
                intent.rationale = "deepen a proven pact";
            } else if (rank == 2 && !warMinded && activeTreatyAge(state, self, other, 2) >= 8 && desire >= prof.affiliationDesire) {
                intent.treatyType = TreatyType::Affiliation;
                intent.duration = TreatyDuration::Indefinite;
                intent.rationale = "share sensors and exploration";
            } else if (rank == 3 && !warMinded && activeTreatyAge(state, self, other, 3) >= 10 && desire >= prof.allianceDesire &&
                       (shared > 0 || selfP == MajorPersonality::Federation)) {
                intent.treatyType = TreatyType::Alliance;
                intent.duration = TreatyDuration::Indefinite;
                intent.rationale = shared > 0 ? "common enemies" : "lasting cooperation";
            } else if (!hasTrade && desire >= prof.tradeDesire) {
                intent.treatyType = TreatyType::Trade;
                intent.rationale = "mutual profit";
            } else if (prof.demandsTribute && rank == 0 && ratio >= 200 && trust < 50 &&
                       ctx.creditBalance(other) >= 100 &&
                       (lastProposal < 0 || state.currentTurn - lastProposal >= AiDemandCooldownTurns)) {
                intent.treatyType = TreatyType::None;
                intent.tone = DiplomaticTone::Demanding;
                intent.creditDemand = std::clamp<int64_t>(ctx.creditBalance(other) / 10, 25, 250);
                intent.duration = TreatyDuration::Indefinite;
                intent.rationale = "tribute from the weak";
            } else if (prof.givesGifts && rank == 0 && desire < prof.napDesire && desire >= prof.napDesire - 15 &&
                       ctx.creditBalance(self) >= 400) {
                intent.treatyType = TreatyType::None;
                intent.creditOffer = std::min<int64_t>(100, ctx.creditBalance(self) / 10);
                intent.duration = TreatyDuration::Indefinite;
                intent.rationale = "goodwill gift";
            } else {
                continue;
            }
            std::string error;
            Proposal probe;
            probe.id = state.nextProposalId;
            probe.proposer = intent.proposer;
            probe.target = intent.target;
            probe.treatyType = intent.treatyType;
            probe.duration = intent.duration;
            probe.tone = intent.tone;
            probe.creditOffer = intent.creditOffer;
            probe.creditDemand = intent.creditDemand;
            probe.submittedTurn = state.currentTurn;
            probe.dueTurn = state.currentTurn + 1;
            if (!validateProposalCreation(state, ctx, probe, &error)) continue;
            intents.push_back(intent);
            ++planned;
        }
    }
    return intents;
}

// -----------------------------------------------------------------------------
// Diplomatic Victory Predicate
// -----------------------------------------------------------------------------

inline constexpr int64_t IndependentMinorNominalPopulation = 30;

struct DiplomaticVictoryStatus {
    int empireId = 0;
    int64_t blocPopulation = 0;   // Own colonies (member minors included) + allied empires
    int64_t galaxyPopulation = 0; // Every colony + nominal population of independent minors
    std::vector<int> allies;      // Majors in a Full Alliance with empireId
    std::vector<int> memberMinors;
    bool achieved = false;

    bool operator==(const DiplomaticVictoryStatus& other) const = default;
};

// Pure predicate: an empire wins diplomatically when its alliance bloc (itself,
// its Full Alliance partners and its member minors) holds a strict majority of
// the galaxy's population and at least one ally or member minor contributes.
inline DiplomaticVictoryStatus evaluateDiplomaticVictory(
    const DiplomacyState& state,
    int empireId,
    const std::map<int, int64_t>& empirePopulation, // Surviving majors only
    const std::map<int, int64_t>& independentMinorPopulation)
{
    DiplomaticVictoryStatus st;
    st.empireId = empireId;
    for (const auto& [id, pop] : empirePopulation) st.galaxyPopulation += std::max<int64_t>(0, pop);
    for (const auto& [id, pop] : independentMinorPopulation) st.galaxyPopulation += std::max<int64_t>(0, pop);
    const auto self = empirePopulation.find(empireId);
    if (self == empirePopulation.end() || self->second <= 0) return st;
    st.blocPopulation = self->second;
    for (const auto& t : state.activeTreaties) {
        if (t.status != TreatyStatus::Active || t.type != TreatyType::Alliance) continue;
        const ParticipantId me = ParticipantId::major(empireId);
        if (!t.involves(me)) continue;
        const ParticipantId other = t.getCounterparty(me);
        if (other.isMinor) continue;
        const auto it = empirePopulation.find(other.id);
        if (it == empirePopulation.end() || it->second <= 0) continue;
        if (std::find(st.allies.begin(), st.allies.end(), other.id) != st.allies.end()) continue;
        st.allies.push_back(other.id);
        st.blocPopulation += it->second;
    }
    std::sort(st.allies.begin(), st.allies.end());
    for (const auto& m : state.minorStates) {
        if (m.membershipOwner == empireId) st.memberMinors.push_back(m.minorId);
    }
    std::sort(st.memberMinors.begin(), st.memberMinors.end());
    st.achieved = (!st.allies.empty() || !st.memberMinors.empty()) &&
                  st.galaxyPopulation > 0 && st.blocPopulation * 2 > st.galaxyPopulation;
    return st;
}

// -----------------------------------------------------------------------------
// Engine Core Operations
// -----------------------------------------------------------------------------

struct WarDeclarationResult {
    bool brokeTreaty = false;           // A cooperation treaty or timed peace was in force
    TreatyType strongestBroken = TreatyType::None;
    std::vector<int> alliesCalled;      // Allies of the defender asked to join
};

class DiplomacyEngine {
public:
    // Submits a new proposal atomically. `responseTurns` is the number of turn
    // resolutions a human target has before the proposal lapses (minimum 1).
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
        std::string* outError = nullptr,
        int responseTurns = 1)
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
        p.dueTurn = state.currentTurn + std::clamp(responseTurns, 1, 10);
        p.status = ProposalStatus::Pending;
        p.resolved = false;

        if (!validateProposalCreation(state, ctx, p, outError)) {
            return false;
        }

        // Resolved proposals are history (the event log keeps the record); drop
        // them before the table fills so it can never lock up permanently.
        if (state.proposals.size() >= DiplomacyState::MaxPendingProposals) {
            purgeResolvedProposals(state);
        }
        if (state.proposals.size() >= DiplomacyState::MaxPendingProposals) {
            if (outError) *outError = "Pending proposal queue limit reached";
            return false;
        }

        state.nextProposalId++;
        state.proposals.push_back(p);
        if (outProposalId) *outProposalId = p.id;

        if (!proposer.isMinor && !target.isMinor) {
            if (auto* mem = state.memory(proposer.id, target.id)) {
                (proposer.id == mem->first ? mem->lastProposalFirstToSecond : mem->lastProposalSecondToFirst) = state.currentTurn;
            }
        }

        DiplomaticEvent evt;
        evt.turn = state.currentTurn;
        evt.type = DiplomaticEventType::ProposalSubmitted;
        evt.partyA = proposer;
        evt.partyB = target;
        evt.numericValue = creditOffer;
        std::ostringstream desc;
        desc << "Proposal " << p.id << " (" << proposalLabel(p) << ") submitted.";
        evt.description = desc.str();
        recordEvent(state, evt);

        return true;
    }

    // Authorized player or manual response: responder MUST equal proposal.target strictly.
    // War declarations are unilateral and cannot be answered.
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
        if (p->treatyType == TreatyType::War) return false;

        // Authorization check: responder identity MUST match proposal target
        if (p->target != responder) {
            return false;
        }

        p->status = accept ? ProposalStatus::Accepted : ProposalStatus::Rejected;
        p->rejectionReason = reason;
        return true;
    }

    // Immediately declares war: cancels pending proposals between the parties,
    // breaks every treaty between them, records betrayal when a treaty was in
    // force (galaxy-wide trust penalty via AdjustTrust ops when `ctx` is given)
    // and calls the defender's allies to arms.
    static WarDeclarationResult declareWar(
        DiplomacyState& state,
        ParticipantId aggressor,
        ParticipantId defender,
        TurnResolutionDelta& delta,
        const DiplomacyContext* ctx = nullptr)
    {
        WarDeclarationResult result;

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
        evt.description = "War declared.";
        recordEvent(state, evt);
        delta.events.push_back(evt);

        for (auto& p : state.proposals) {
            if (!p.resolved && p.status == ProposalStatus::Pending &&
                ((p.proposer == aggressor && p.target == defender) ||
                 (p.proposer == defender && p.target == aggressor)))
            {
                p.status = ProposalStatus::Cancelled;
                p.resolved = true;
                p.rejectionReason = p.treatyType == TreatyType::War
                    ? "Superseded by an immediate declaration of war"
                    : "Cancelled due to declaration of war";
                delta.rejectedProposals.push_back(p.id);

                DiplomaticEvent rejEvt;
                rejEvt.turn = state.currentTurn;
                rejEvt.type = DiplomaticEventType::ProposalRejected;
                rejEvt.partyA = p.proposer;
                rejEvt.partyB = p.target;
                rejEvt.description = std::string(proposalLabel(p)) + " proposal cancelled by the declaration of war.";
                recordEvent(state, rejEvt);
                delta.events.push_back(rejEvt);
            }
        }

        for (const auto& t : state.activeTreaties) {
            if (t.status != TreatyStatus::Active || !t.involvesBoth(aggressor, defender)) continue;
            if (getTreatyRank(t.type) > 0 || t.type == TreatyType::Peace) {
                result.brokeTreaty = true;
                if (getTreatyRank(t.type) > getTreatyRank(result.strongestBroken) || result.strongestBroken == TreatyType::None) {
                    result.strongestBroken = t.type;
                }
            }
        }

        breakIncompatibleTreaties(state, aggressor, defender, delta);

        if (!aggressor.isMinor && !defender.isMinor) {
            if (auto* mem = state.memory(aggressor.id, defender.id)) mem->lastWarTurn = state.currentTurn;
            // A war declared between these two lapses any call to arms about them.
            dropDefenseCallsBetween(state, aggressor.id, defender.id, /*onlyAsBelligerents=*/false, aggressor.id);
        }

        if (result.brokeTreaty && !aggressor.isMinor) {
            if (auto* st = state.standing(aggressor.id)) {
                st->betrayals = std::min(1000, st->betrayals + 1);
                st->lastBetrayalTurn = state.currentTurn;
            }
            if (!defender.isMinor) state.addGrievance(defender.id, aggressor.id, 3);

            DiplomaticEvent betrayal;
            betrayal.turn = state.currentTurn;
            betrayal.type = DiplomaticEventType::TreatyBetrayed;
            betrayal.partyA = aggressor;
            betrayal.partyB = defender;
            betrayal.numericValue = static_cast<int64_t>(result.strongestBroken);
            betrayal.description = std::string("Broke the ") + treatyTypeName(result.strongestBroken) +
                                   " by declaring war; the galaxy takes note.";
            recordEvent(state, betrayal);
            delta.events.push_back(betrayal);

            if (ctx) {
                // Victim: extra -15 on top of the campaign's war penalty. Everyone else: -10.
                pushTrust(delta, aggressor, defender, -15, "Treaty betrayal");
                for (int id : ctx->majorEmpires()) {
                    const ParticipantId z = ParticipantId::major(id);
                    if (z == aggressor || z == defender || ctx->isEliminated(z) || !ctx->hasContact(aggressor, z)) continue;
                    pushTrust(delta, aggressor, z, -10, "Galaxy-wide reputation loss for treaty betrayal");
                }
            }
        }

        // Mutual defence: every ally of the defender is called to arms.
        if (!aggressor.isMinor && !defender.isMinor) {
            for (const auto& t : state.activeTreaties) {
                if (t.status != TreatyStatus::Active || t.type != TreatyType::Alliance || !t.involves(defender)) continue;
                const ParticipantId ally = t.getCounterparty(defender);
                if (ally.isMinor || ally == aggressor) continue;
                if (state.hasActiveTreaty(ally, aggressor, TreatyType::Alliance)) continue; // Torn loyalties: no call
                if (ctx && (ctx->isEliminated(ally) || ctx->isAtWar(ally, aggressor))) continue;
                if (std::find(result.alliesCalled.begin(), result.alliesCalled.end(), ally.id) != result.alliesCalled.end()) continue;
                bool duplicate = false;
                for (const auto& c : state.defenseCalls) {
                    if (c.ally == ally.id && c.defender == defender.id && c.aggressor == aggressor.id) duplicate = true;
                }
                if (duplicate || state.defenseCalls.size() >= DiplomacyState::MaxDefenseCalls) continue;
                DefenseCall call;
                call.ally = ally.id;
                call.defender = defender.id;
                call.aggressor = aggressor.id;
                call.issuedTurn = state.currentTurn;
                call.dueTurn = state.currentTurn + DiplomacyState::DefenseCallWindow;
                state.defenseCalls.push_back(call);
                result.alliesCalled.push_back(ally.id);

                DiplomaticEvent callEvt;
                callEvt.turn = state.currentTurn;
                callEvt.type = DiplomaticEventType::AllianceCalledToArms;
                callEvt.partyA = defender;
                callEvt.partyB = ally;
                callEvt.numericValue = aggressor.id;
                callEvt.description = "Called its ally to arms against empire " + std::to_string(aggressor.id) +
                                      "; join the war by turn " + std::to_string(call.dueTurn) + ".";
                recordEvent(state, callEvt);
                delta.events.push_back(callEvt);
            }
        }
        return result;
    }

    // Records a peace settlement reached outside proposal resolution (e.g. the
    // legacy peace offer) and emits the SetWarState(false) operation.
    static void recordPeace(DiplomacyState& state, ParticipantId a, ParticipantId b, TurnResolutionDelta& delta) {
        DiplomacyOperation peaceOp;
        peaceOp.opType = DiplomaticOpType::SetWarState;
        peaceOp.partyA = a;
        peaceOp.partyB = b;
        peaceOp.setAtWar = false;
        peaceOp.notes = "Peace established";
        delta.operations.push_back(peaceOp);

        DiplomaticEvent evt;
        evt.turn = state.currentTurn;
        evt.type = DiplomaticEventType::PeaceEstablished;
        evt.partyA = a;
        evt.partyB = b;
        evt.description = "Peace treaty concluded.";
        recordEvent(state, evt);
        delta.events.push_back(evt);

        if (!a.isMinor && !b.isMinor) {
            if (auto* mem = state.memory(a.id, b.id)) mem->lastPeaceTurn = state.currentTurn;
            dropDefenseCallsBetween(state, a.id, b.id, /*onlyAsBelligerents=*/true, 0);
        }
    }

    // Establishes a treaty, superseding weaker cooperation treaties between the
    // parties. Enforces duplicates, downgrades and the active-treaty cap before
    // anything is emitted: on failure nothing changes and false is returned.
    static bool establishTreaty(
        DiplomacyState& state,
        ParticipantId a,
        ParticipantId b,
        TreatyType type,
        TreatyDuration duration,
        TurnResolutionDelta& delta,
        std::string* outError = nullptr)
    {
        if (a == b || type == TreatyType::None || type == TreatyType::War || type == TreatyType::Membership) {
            if (outError) *outError = "Not a treaty that can be established";
            return false;
        }
        const int rank = getTreatyRank(type);
        size_t superseded = 0;
        for (const auto& existing : state.activeTreaties) {
            if (existing.status != TreatyStatus::Active || !existing.involvesBoth(a, b)) continue;
            if (existing.type == type) {
                if (outError) *outError = "Identical treaty is already active";
                return false;
            }
            const int existingRank = getTreatyRank(existing.type);
            if (rank > 0 && existingRank >= rank) {
                if (outError) *outError = "An equal or stronger treaty is already active";
                return false;
            }
            if (supersedes(type, existing.type)) ++superseded;
        }
        if (state.activeTreaties.size() - superseded >= DiplomacyState::MaxActiveTreaties) {
            if (outError) *outError = "Active treaty capacity reached";
            return false;
        }

        std::vector<ActiveTreaty> remaining;
        remaining.reserve(state.activeTreaties.size() + 1);
        for (auto& existing : state.activeTreaties) {
            if (existing.status == TreatyStatus::Active && existing.involvesBoth(a, b) && supersedes(type, existing.type)) {
                existing.status = TreatyStatus::Superseded;
                archiveTreaty(state, existing);

                DiplomacyOperation termOp;
                termOp.opType = DiplomaticOpType::TerminateTreaty;
                termOp.partyA = existing.partyA;
                termOp.partyB = existing.partyB;
                termOp.treatyType = existing.type;
                termOp.notes = "Superseded by higher treaty";
                delta.operations.push_back(termOp);

                DiplomaticEvent sup;
                sup.turn = state.currentTurn;
                sup.type = DiplomaticEventType::TreatySuperseded;
                sup.partyA = existing.partyA;
                sup.partyB = existing.partyB;
                sup.numericValue = static_cast<int64_t>(existing.type);
                sup.description = std::string(treatyTypeName(existing.type)) + " superseded by " + treatyTypeName(type) + ".";
                recordEvent(state, sup);
                delta.events.push_back(sup);
            } else {
                remaining.push_back(existing);
            }
        }
        state.activeTreaties = std::move(remaining);

        ActiveTreaty newTreaty;
        newTreaty.id = state.nextTreatyId++;
        newTreaty.partyA = a;
        newTreaty.partyB = b;
        newTreaty.type = type;
        newTreaty.duration = duration;
        newTreaty.startTurn = state.currentTurn;
        newTreaty.turnsRemaining = (duration == TreatyDuration::Indefinite) ? -1 : static_cast<int>(duration);
        newTreaty.status = TreatyStatus::Active;
        state.activeTreaties.push_back(newTreaty);

        if (type == TreatyType::Friendship) {
            if (b.isMinor) {
                if (auto* minor = state.getMinorState(b.id)) minor->friendshipTurns[a.id] = state.currentTurn;
            } else if (a.isMinor) {
                if (auto* minor = state.getMinorState(a.id)) minor->friendshipTurns[b.id] = state.currentTurn;
            }
        }

        DiplomacyOperation treatOp;
        treatOp.opType = DiplomaticOpType::ApplyTreaty;
        treatOp.partyA = a;
        treatOp.partyB = b;
        treatOp.treatyType = type;
        treatOp.duration = duration;
        treatOp.turnsRemaining = newTreaty.turnsRemaining;
        treatOp.notes = "Treaty ratified";
        delta.operations.push_back(treatOp);

        DiplomaticEvent evt;
        evt.turn = state.currentTurn;
        evt.type = DiplomaticEventType::TreatyEstablished;
        evt.partyA = a;
        evt.partyB = b;
        evt.numericValue = static_cast<int64_t>(type);
        evt.description = std::string(treatyTypeName(type)) + " ratified.";
        recordEvent(state, evt);
        delta.events.push_back(evt);
        return true;
    }

    // Dissolves one active treaty (e.g. an alliance after a refused call to arms).
    static bool dissolveTreaty(
        DiplomacyState& state,
        ParticipantId a,
        ParticipantId b,
        TreatyType type,
        const std::string& reason,
        TurnResolutionDelta& delta)
    {
        bool found = false;
        std::vector<ActiveTreaty> remaining;
        remaining.reserve(state.activeTreaties.size());
        for (auto& t : state.activeTreaties) {
            if (!found && t.status == TreatyStatus::Active && t.type == type && t.involvesBoth(a, b)) {
                found = true;
                t.status = TreatyStatus::Dissolved;
                archiveTreaty(state, t);
                clearMinorFriendship(state, t);
                DiplomacyOperation op;
                op.opType = DiplomaticOpType::TerminateTreaty;
                op.partyA = t.partyA;
                op.partyB = t.partyB;
                op.treatyType = t.type;
                op.notes = reason;
                delta.operations.push_back(op);

                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::TreatyExpired;
                evt.partyA = t.partyA;
                evt.partyB = t.partyB;
                evt.numericValue = static_cast<int64_t>(t.type);
                evt.description = std::string(treatyTypeName(t.type)) + " dissolved: " + reason;
                recordEvent(state, evt);
                delta.events.push_back(evt);
            } else {
                remaining.push_back(t);
            }
        }
        state.activeTreaties = std::move(remaining);
        return found;
    }

    // Removes an eliminated participant from diplomacy: its treaties dissolve,
    // its open proposals are cancelled and calls to arms about it lapse.
    // Returns the number of treaties and proposals affected.
    static int dissolveParticipant(DiplomacyState& state, ParticipantId gone, TurnResolutionDelta& delta) {
        int affected = 0;
        std::vector<ActiveTreaty> remaining;
        remaining.reserve(state.activeTreaties.size());
        for (auto& t : state.activeTreaties) {
            if (t.involves(gone)) {
                ++affected;
                t.status = TreatyStatus::Dissolved;
                archiveTreaty(state, t);
                clearMinorFriendship(state, t);
                DiplomacyOperation op;
                op.opType = DiplomaticOpType::TerminateTreaty;
                op.partyA = t.partyA;
                op.partyB = t.partyB;
                op.treatyType = t.type;
                op.notes = "Party eliminated";
                delta.operations.push_back(op);
            } else {
                remaining.push_back(t);
            }
        }
        state.activeTreaties = std::move(remaining);
        for (auto& p : state.proposals) {
            if (!p.resolved && (p.proposer == gone || p.target == gone)) {
                ++affected;
                p.status = ProposalStatus::Cancelled;
                p.resolved = true;
                p.rejectionReason = "Party eliminated";
                delta.rejectedProposals.push_back(p.id);
            }
        }
        if (!gone.isMinor) {
            const auto before = state.defenseCalls.size();
            std::erase_if(state.defenseCalls, [&](const DefenseCall& c) {
                return c.ally == gone.id || c.defender == gone.id || c.aggressor == gone.id;
            });
            affected += static_cast<int>(before - state.defenseCalls.size());
        }
        if (affected > 0) {
            DiplomaticEvent evt;
            evt.turn = state.currentTurn;
            evt.type = DiplomaticEventType::ParticipantEliminated;
            evt.partyA = gone;
            evt.partyB = gone;
            evt.numericValue = affected;
            evt.description = "Eliminated; all treaties dissolved and negotiations cancelled.";
            recordEvent(state, evt);
            delta.events.push_back(evt);
        }
        return affected;
    }

    // Resolves turn boundary: decrements treaties, resolves pending offers, produces operations
    static TurnResolutionDelta resolveTurn(
        DiplomacyState& state,
        const DiplomacyContext& ctx,
        uint64_t aiSeed = 1337)
    {
        TurnResolutionDelta delta;

        // ---------------------------------------------------------------------
        // Step 0: Purge proposals resolved on earlier turns (the event history
        // keeps their record) and remove eliminated participants.
        // ---------------------------------------------------------------------
        purgeResolvedProposals(state);
        {
            std::set<ParticipantId> gone;
            for (const auto& t : state.activeTreaties) {
                if (ctx.isEliminated(t.partyA)) gone.insert(t.partyA);
                if (ctx.isEliminated(t.partyB)) gone.insert(t.partyB);
            }
            for (const auto& p : state.proposals) {
                if (ctx.isEliminated(p.proposer)) gone.insert(p.proposer);
                if (ctx.isEliminated(p.target)) gone.insert(p.target);
            }
            for (const auto& c : state.defenseCalls) {
                for (int id : {c.ally, c.defender, c.aggressor}) {
                    if (ctx.isEliminated(ParticipantId::major(id))) gone.insert(ParticipantId::major(id));
                }
            }
            for (const auto& p : gone) dissolveParticipant(state, p, delta);
        }

        // ---------------------------------------------------------------------
        // Step 1: Tick Active Treaties & Check Expiry. Non-active rows (e.g.
        // from an old save) are archived rather than silently dropped.
        // ---------------------------------------------------------------------
        std::vector<ActiveTreaty> remainingActive;
        remainingActive.reserve(state.activeTreaties.size());

        for (auto& treaty : state.activeTreaties) {
            if (treaty.status != TreatyStatus::Active) {
                archiveTreaty(state, treaty);
                continue;
            }

            if (treaty.duration != TreatyDuration::Indefinite && treaty.turnsRemaining > 0) {
                treaty.turnsRemaining--;
            }

            if (treaty.duration != TreatyDuration::Indefinite && treaty.turnsRemaining <= 0) {
                treaty.status = TreatyStatus::Expired;
                archiveTreaty(state, treaty);
                delta.expiredTreaties.push_back(treaty.id);

                // If expired treaty was friendship with minor, update minor's friendship status
                if (treaty.type == TreatyType::Friendship) {
                    clearMinorFriendship(state, treaty);
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
                evt.numericValue = static_cast<int64_t>(treaty.type);
                evt.description = std::string(treatyTypeName(treaty.type)) + " expired.";
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

        auto rejectWithEvent = [&](Proposal& p, ProposalStatus status, const std::string& reason, const std::string& text) {
            p.status = status;
            p.rejectionReason = reason;
            delta.rejectedProposals.push_back(p.id);
            if (!text.empty()) {
                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::ProposalRejected;
                evt.partyA = p.proposer;
                evt.partyB = p.target;
                evt.numericValue = static_cast<int64_t>(p.treatyType);
                evt.description = text;
                recordEvent(state, evt);
                delta.events.push_back(evt);
            }
        };

        for (size_t idx : proposalIndices) {
            auto& p = state.proposals[idx];
            if (p.resolved) continue; // e.g. cancelled by a war declared earlier this step

            // Check if minor has already joined an empire during this turn's resolution
            if (p.target.isMinor && p.treatyType == TreatyType::Membership) {
                if (minorsJoinedThisTurn.count(p.target.id) > 0) {
                    p.resolved = true;
                    rejectWithEvent(p, ProposalStatus::Rejected,
                        "Minor accepted competing membership offer from another empire",
                        "Minor rejected membership in favor of rival empire.");
                    continue;
                }
            }

            if (p.status == ProposalStatus::Pending) {
                if (p.treatyType == TreatyType::War) {
                    // Unilateral: a declaration of war needs nobody's consent.
                    p.status = ProposalStatus::Accepted;
                } else if (!ctx.isHuman(p.target)) {
                    AiDecisionResult aiRes = evaluateAiProposal(state, ctx, p, aiSeed, ctx.personality(p.target));
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
                if (p.treatyType == TreatyType::War) {
                    if (atWarNow) {
                        rejectWithEvent(p, ProposalStatus::Rejected, "Belligerents are already at war", "");
                        continue;
                    }
                    delta.acceptedProposals.push_back(p.id);
                    setWorkingAtWar(p.proposer, p.target, true);
                    declareWar(state, p.proposer, p.target, delta, &ctx);
                    continue;
                }
                if (p.treatyType != TreatyType::Peace && p.treatyType != TreatyType::None && atWarNow) {
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
                        rejectWithEvent(p, ProposalStatus::Rejected, "Minor not found", "");
                        continue;
                    }
                    if (minorsJoinedThisTurn.count(p.target.id) > 0) {
                        rejectWithEvent(p, ProposalStatus::Rejected,
                            "Minor accepted competing membership offer from another empire",
                            "Minor rejected membership in favor of rival empire.");
                        continue;
                    }
                    if (minor->membershipOwner != 0) {
                        rejectWithEvent(p, ProposalStatus::Rejected, "Minor is already a member of an empire", "");
                        continue;
                    }
                    int frTurn = minor->getFriendshipTurn(p.proposer.id);
                    if (frTurn < 0 || frTurn >= state.currentTurn) {
                        rejectWithEvent(p, ProposalStatus::Rejected,
                            "Membership requires an established active Friendship from a prior turn", "");
                        continue;
                    }
                    if (minor->getAttitude(p.proposer.id) < 80) {
                        rejectWithEvent(p, ProposalStatus::Rejected,
                            "Minor attitude at settlement fell below required 80 threshold", "");
                        continue;
                    }
                }

                // 3. Treaty Downgrade / Conflict / Capacity Check against cumulative active treaties
                const bool createsTreaty = p.treatyType != TreatyType::None && p.treatyType != TreatyType::Peace &&
                                           p.treatyType != TreatyType::Membership;
                if (createsTreaty) {
                    int proposedRank = getTreatyRank(p.treatyType);
                    std::string conflict;
                    size_t superseded = 0;
                    for (const auto& existing : state.activeTreaties) {
                        if (existing.status == TreatyStatus::Active && existing.involvesBoth(p.proposer, p.target)) {
                            if (existing.type == p.treatyType) {
                                conflict = "Identical treaty is already active";
                                break;
                            }
                            int existingRank = getTreatyRank(existing.type);
                            if (existingRank >= proposedRank && proposedRank > 0) {
                                conflict = "An equal or stronger treaty is already active";
                                break;
                            }
                            if (supersedes(p.treatyType, existing.type)) ++superseded;
                        }
                    }
                    if (conflict.empty() && state.activeTreaties.size() - superseded >= DiplomacyState::MaxActiveTreaties) {
                        conflict = "Active treaty capacity reached";
                    }
                    if (!conflict.empty()) {
                        rejectWithEvent(p, ProposalStatus::Rejected, conflict, "");
                        continue;
                    }
                }

                // 4. Working Ledger Overdraft Check
                int64_t propBal = getWorkingBalance(p.proposer);
                int64_t targetBal = getWorkingBalance(p.target);

                if (p.creditOffer > 0 && propBal < p.creditOffer) {
                    rejectWithEvent(p, ProposalStatus::Rejected,
                        "Proposer overdraft: insufficient funds in resolution ledger",
                        "Proposal rejected: Proposer unable to pay offered credits.");
                    continue;
                }

                if (p.creditDemand > 0 && targetBal < p.creditDemand) {
                    rejectWithEvent(p, ProposalStatus::Rejected,
                        "Target overdraft: insufficient funds in resolution ledger",
                        "Proposal rejected: Target unable to pay demanded credits.");
                    continue;
                }

                // -------------------------------------------------------------
                // ALL CHECKS PASSED: Commit Acceptance & Apply State Mutations
                // -------------------------------------------------------------
                delta.acceptedProposals.push_back(p.id);
                if (!p.proposer.isMinor && !p.target.isMinor) state.noteProposalOutcome(p.proposer.id, p.target.id, true);

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
                if (p.treatyType == TreatyType::Peace) {
                    setWorkingAtWar(p.proposer, p.target, false);
                    recordPeace(state, p.proposer, p.target, delta);
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
                } else if (p.treatyType == TreatyType::None) {
                    if (p.creditDemand > 0) {
                        DiplomaticEvent evt;
                        evt.turn = state.currentTurn;
                        evt.type = DiplomaticEventType::TributeDemanded;
                        evt.partyA = p.proposer;
                        evt.partyB = p.target;
                        evt.numericValue = p.creditDemand;
                        evt.description = "Tribute of " + std::to_string(p.creditDemand) + " credits paid.";
                        recordEvent(state, evt);
                        delta.events.push_back(evt);
                        if (!p.target.isMinor) {
                            pushTrust(delta, p.proposer, p.target, -5, "Resentment over paid tribute");
                            state.addGrievance(p.target.id, p.proposer.id, 1);
                        }
                    } else if (p.creditOffer > 0) {
                        DiplomaticEvent evt;
                        evt.turn = state.currentTurn;
                        evt.type = DiplomaticEventType::GiftDelivered;
                        evt.partyA = p.proposer;
                        evt.partyB = p.target;
                        evt.numericValue = p.creditOffer;
                        evt.description = "Gift of " + std::to_string(p.creditOffer) + " credits delivered.";
                        recordEvent(state, evt);
                        delta.events.push_back(evt);
                        if (!p.target.isMinor && !p.proposer.isMinor) {
                            const int gain = static_cast<int>(std::clamp<int64_t>(p.creditOffer / 50, 1, 10));
                            pushTrust(delta, p.proposer, p.target, gain, "Goodwill gift");
                        }
                    }
                } else {
                    std::string err;
                    if (establishTreaty(state, p.proposer, p.target, p.treatyType, p.duration, delta, &err)) {
                        if (!p.target.isMinor && !p.proposer.isMinor) {
                            pushTrust(delta, p.proposer, p.target, 5, "Treaty signed");
                        }
                    }
                }

                if (p.target.isMinor && p.creditOffer > 0) {
                    auto* minor = state.getMinorState(p.target.id);
                    if (minor) {
                        // S19 baseline: 4 attitude per 1,000 credits (no overflow for any credit value).
                        int gain = static_cast<int>(std::clamp<int64_t>(p.creditOffer / 250, 1, 100));
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

                        // Rival influence: courting another empire's member loosens its loyalty.
                        const int owner = minor->membershipOwner;
                        if (owner > 0 && owner != p.proposer.id) {
                            const int oldOwnerAtt = minor->getAttitude(owner);
                            const int newOwnerAtt = std::clamp(oldOwnerAtt - std::max(1, gain / 2), 0, 100);
                            minor->attitudes[owner] = newOwnerAtt;
                            DiplomacyOperation rivalOp;
                            rivalOp.opType = DiplomaticOpType::UpdateMinorAttitude;
                            rivalOp.partyA = p.target;
                            rivalOp.partyB = ParticipantId::major(owner);
                            rivalOp.attitudeDelta = newOwnerAtt - oldOwnerAtt;
                            rivalOp.notes = "Loyalty eroded by rival influence";
                            delta.operations.push_back(rivalOp);
                        }
                    }
                }
            } else if (p.status == ProposalStatus::Rejected) {
                delta.rejectedProposals.push_back(p.id);
                if (!p.proposer.isMinor && !p.target.isMinor) state.noteProposalOutcome(p.proposer.id, p.target.id, false);

                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::ProposalRejected;
                evt.partyA = p.proposer;
                evt.partyB = p.target;
                evt.numericValue = static_cast<int64_t>(p.treatyType);
                evt.description = std::string(proposalLabel(p)) + " proposal rejected by target.";
                recordEvent(state, evt);
                delta.events.push_back(evt);
                if (p.treatyType == TreatyType::None && p.creditDemand > 0 && !p.proposer.isMinor && !p.target.isMinor) {
                    // A refused demand is an insult the demander remembers.
                    state.addGrievance(p.proposer.id, p.target.id, 2);
                    pushTrust(delta, p.proposer, p.target, -3, "Tribute refused");
                }
            } else if (p.status == ProposalStatus::Ignored) {
                delta.ignoredProposals.push_back(p.id);
                if (!p.proposer.isMinor && !p.target.isMinor) state.noteProposalOutcome(p.proposer.id, p.target.id, false);

                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.type = DiplomaticEventType::ProposalIgnored;
                evt.partyA = p.proposer;
                evt.partyB = p.target;
                evt.numericValue = static_cast<int64_t>(p.treatyType);
                evt.description = std::string(proposalLabel(p)) + " proposal lapsed without answer.";
                recordEvent(state, evt);
                delta.events.push_back(evt);
                if (p.treatyType == TreatyType::None && p.creditDemand > 0 && !p.proposer.isMinor && !p.target.isMinor) {
                    state.addGrievance(p.proposer.id, p.target.id, 1);
                }
            }
        }

        // ---------------------------------------------------------------------
        // Step 5: Calls to arms that fall due are honored or refused.
        // ---------------------------------------------------------------------
        {
            std::vector<DefenseCall> stillOpen;
            for (const auto& call : state.defenseCalls) {
                const ParticipantId ally = ParticipantId::major(call.ally);
                const ParticipantId defender = ParticipantId::major(call.defender);
                const ParticipantId aggressor = ParticipantId::major(call.aggressor);
                if (!isWorkingAtWar(defender, aggressor) || !state.hasActiveTreaty(ally, defender, TreatyType::Alliance)) {
                    continue; // Peace was made or the alliance ended: the call lapses.
                }
                const bool joined = isWorkingAtWar(ally, aggressor);
                if (!joined && state.currentTurn < call.dueTurn) {
                    stillOpen.push_back(call);
                    continue;
                }
                DiplomaticEvent evt;
                evt.turn = state.currentTurn;
                evt.partyA = ally;
                evt.partyB = defender;
                evt.numericValue = call.aggressor;
                if (joined) {
                    evt.type = DiplomaticEventType::AllianceHonored;
                    evt.description = "Honored the alliance and joined the war against empire " + std::to_string(call.aggressor) + ".";
                    pushTrust(delta, ally, defender, 10, "Alliance honored");
                } else {
                    evt.type = DiplomaticEventType::AllianceRefused;
                    evt.description = "Refused the call to arms against empire " + std::to_string(call.aggressor) + ".";
                    pushTrust(delta, ally, defender, -20, "Alliance obligation refused");
                    state.addGrievance(defender.id, ally.id, 2);
                }
                recordEvent(state, evt);
                delta.events.push_back(evt);
                if (!joined && !ctx.isHuman(defender) && ctx.trust(defender, ally) - 20 < 40) {
                    dissolveTreaty(state, defender, ally, TreatyType::Alliance,
                        "Alliance cancelled after a refused call to arms", delta);
                }
            }
            state.defenseCalls = std::move(stillOpen);
        }

        // ---------------------------------------------------------------------
        // Step 6: Old grievances fade slowly (one point per ten turns).
        // ---------------------------------------------------------------------
        if (state.currentTurn > 0 && state.currentTurn % 10 == 0) {
            for (auto& m : state.relationMemory) {
                m.grievancesOfFirst = std::max(0, m.grievancesOfFirst - 1);
                m.grievancesOfSecond = std::max(0, m.grievancesOfSecond - 1);
            }
        }

        pruneProposals(state);

        state.currentTurn++;

        return delta;
    }

    // An ally explicitly refuses an open call to arms (the player's RefuseCallToArms
    // order) instead of letting it lapse. The consequences are those of a call that
    // falls due unanswered (resolveTurn step 5), applied at once: -20 trust with the
    // defender, +2 grievances for the defender, an AllianceRefused event, and an AI
    // defender whose trust would fall below 40 cancels the alliance. Returns false
    // (and changes nothing) when no such call is open.
    static bool refuseDefenseCall(DiplomacyState& state, const DiplomacyContext& ctx, int ally, int defender,
                                  int aggressor, TurnResolutionDelta& delta) {
        const auto call = std::find_if(state.defenseCalls.begin(), state.defenseCalls.end(), [&](const DefenseCall& c) {
            return c.ally == ally && c.defender == defender && c.aggressor == aggressor;
        });
        if (call == state.defenseCalls.end()) return false;
        state.defenseCalls.erase(call);
        const ParticipantId allyP = ParticipantId::major(ally);
        const ParticipantId defenderP = ParticipantId::major(defender);
        DiplomaticEvent evt;
        evt.turn = state.currentTurn;
        evt.type = DiplomaticEventType::AllianceRefused;
        evt.partyA = allyP;
        evt.partyB = defenderP;
        evt.numericValue = aggressor;
        evt.description = "Refused the call to arms against empire " + std::to_string(aggressor) + " (-20 trust).";
        pushTrust(delta, allyP, defenderP, -20, "Alliance obligation refused");
        state.addGrievance(defender, ally, 2);
        recordEvent(state, evt);
        delta.events.push_back(evt);
        if (!ctx.isHuman(defenderP) && ctx.trust(defenderP, allyP) - 20 < 40) {
            dissolveTreaty(state, defenderP, allyP, TreatyType::Alliance, "Alliance cancelled after a refused call to arms", delta);
        }
        return true;
    }

    // Appends to the bounded event history (oldest entries drop first).
    static void recordEvent(DiplomacyState& state, const DiplomaticEvent& evt) {
        if (state.eventHistory.size() >= DiplomacyState::MaxEventHistory) {
            state.eventHistory.erase(state.eventHistory.begin());
        }
        state.eventHistory.push_back(evt);
    }

    static const char* proposalLabel(const Proposal& p) {
        if (p.treatyType == TreatyType::None) {
            return p.creditDemand > 0 ? "Tribute Demand" : "Gift";
        }
        if (p.treatyType == TreatyType::War) return "Declaration of War";
        return treatyTypeName(p.treatyType);
    }

private:
    // Whether establishing `incoming` retires `existing` between the same parties.
    // Cooperation treaties (NAP..Alliance) and Membership replace each other;
    // Trade pacts and timed Peace treaties coexist with them.
    static bool supersedes(TreatyType incoming, TreatyType existing) {
        if (incoming == TreatyType::Trade || existing == TreatyType::Trade) return false;
        if (incoming == TreatyType::Peace) return false;
        if (existing == TreatyType::Peace) return getTreatyRank(incoming) > 0;
        return true;
    }

    static void pushTrust(TurnResolutionDelta& delta, ParticipantId a, ParticipantId b, int amount, const char* note) {
        if (a.isMinor || b.isMinor || a == b || amount == 0) return;
        DiplomacyOperation op;
        op.opType = DiplomaticOpType::AdjustTrust;
        op.partyA = a;
        op.partyB = b;
        op.attitudeDelta = amount;
        op.notes = note;
        delta.operations.push_back(op);
    }

    static void clearMinorFriendship(DiplomacyState& state, const ActiveTreaty& t) {
        if (t.type != TreatyType::Friendship) return;
        if (t.partyA.isMinor) {
            auto* m = state.getMinorState(t.partyA.id);
            if (m) m->friendshipTurns[t.partyB.id] = -1;
        }
        if (t.partyB.isMinor) {
            auto* m = state.getMinorState(t.partyB.id);
            if (m) m->friendshipTurns[t.partyA.id] = -1;
        }
    }

    // onlyAsBelligerents: drop calls where {defender, aggressor} == {a, b} (peace made).
    // Otherwise also drop calls asking `a`/`b` to fight each other once they are at war
    // (they are resolved as honored/refused normally when due, so only the
    // aggressor-side duplicates matter here).
    static void dropDefenseCallsBetween(DiplomacyState& state, int a, int b, bool onlyAsBelligerents, int newAggressor) {
        std::erase_if(state.defenseCalls, [&](const DefenseCall& c) {
            const bool belligerents = (c.defender == a && c.aggressor == b) || (c.defender == b && c.aggressor == a);
            if (onlyAsBelligerents) return belligerents;
            // The ally itself became the aggressor against the defender: the call is void.
            return newAggressor != 0 && c.ally == newAggressor && (c.defender == a || c.defender == b);
        });
    }

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
                evt.numericValue = static_cast<int64_t>(t.type);
                evt.description = std::string(treatyTypeName(t.type)) + " terminated due to war.";
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

    static void purgeResolvedProposals(DiplomacyState& state) {
        std::erase_if(state.proposals, [](const Proposal& p) { return p.resolved; });
    }

    static void pruneProposals(DiplomacyState& state) {
        // Resolved proposals go first; then the oldest open ones.
        if (state.proposals.size() > DiplomacyState::MaxPendingProposals) {
            size_t excess = state.proposals.size() - DiplomacyState::MaxPendingProposals;
            std::erase_if(state.proposals, [&](const Proposal& p) {
                if (excess > 0 && p.resolved) { --excess; return true; }
                return false;
            });
        }
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

    // Optional modern-rule sections: emitted only when non-empty so saves written
    // before they existed re-serialize byte-for-byte (canonical save check).
    if (!state.relationMemory.empty()) {
        ss << "relationMemory " << state.relationMemory.size() << "\n";
        for (const auto& m : state.relationMemory) {
            ss << "rm " << m.first << " " << m.second << " "
               << m.lastWarTurn << " " << m.lastPeaceTurn << " "
               << m.lastProposalFirstToSecond << " " << m.lastProposalSecondToFirst << " "
               << m.grievancesOfFirst << " " << m.grievancesOfSecond << " "
               << m.refusalsFirstToSecond << " " << m.refusalsSecondToFirst << "\n";
        }
    }
    if (!state.standings.empty()) {
        ss << "standings " << state.standings.size() << "\n";
        for (const auto& st : state.standings) {
            ss << "st " << st.empireId << " " << st.betrayals << " " << st.lastBetrayalTurn << "\n";
        }
    }
    if (!state.defenseCalls.empty()) {
        ss << "defenseCalls " << state.defenseCalls.size() << "\n";
        for (const auto& c : state.defenseCalls) {
            ss << "dc " << c.ally << " " << c.defender << " " << c.aggressor << " "
               << c.issuedTurn << " " << c.dueTurn << "\n";
        }
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

    // Optional modern-rule sections, in fixed order, each at most once.
    auto validMajor = [](int id) { return id >= 1 && id <= 8; };
    if (!(ss >> token)) {
        if (outError) *outError = "Missing END_BOTF_DIPLOMACY terminal token";
        return false;
    }
    if (token == "relationMemory") {
        size_t count = 0;
        if (!(ss >> count) || count == 0 || count > DiplomacyState::MaxRelationMemory) {
            if (outError) *outError = "Malformed relationMemory count";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            RelationMemory m;
            if (!(ss >> token) || token != "rm" ||
                !(ss >> m.first >> m.second >> m.lastWarTurn >> m.lastPeaceTurn
                     >> m.lastProposalFirstToSecond >> m.lastProposalSecondToFirst
                     >> m.grievancesOfFirst >> m.grievancesOfSecond
                     >> m.refusalsFirstToSecond >> m.refusalsSecondToFirst))
            {
                if (outError) *outError = "Malformed relation memory row";
                return false;
            }
            if (!validMajor(m.first) || !validMajor(m.second) || m.first >= m.second ||
                m.lastWarTurn < -1 || m.lastPeaceTurn < -1 ||
                m.lastProposalFirstToSecond < -1 || m.lastProposalSecondToFirst < -1 ||
                m.lastWarTurn > temp.currentTurn || m.lastPeaceTurn > temp.currentTurn ||
                m.lastProposalFirstToSecond > temp.currentTurn || m.lastProposalSecondToFirst > temp.currentTurn ||
                m.grievancesOfFirst < 0 || m.grievancesOfFirst > DiplomacyState::MaxGrievances ||
                m.grievancesOfSecond < 0 || m.grievancesOfSecond > DiplomacyState::MaxGrievances ||
                m.refusalsFirstToSecond < 0 || m.refusalsFirstToSecond > DiplomacyState::MaxRefusals ||
                m.refusalsSecondToFirst < 0 || m.refusalsSecondToFirst > DiplomacyState::MaxRefusals)
            {
                if (outError) *outError = "Out-of-range relation memory row";
                return false;
            }
            if (!temp.relationMemory.empty()) {
                const auto& prev = temp.relationMemory.back();
                if (prev.first > m.first || (prev.first == m.first && prev.second >= m.second)) {
                    if (outError) *outError = "Relation memory rows must be unique and sorted";
                    return false;
                }
            }
            temp.relationMemory.push_back(m);
        }
        if (!(ss >> token)) {
            if (outError) *outError = "Missing END_BOTF_DIPLOMACY terminal token";
            return false;
        }
    }
    if (token == "standings") {
        size_t count = 0;
        if (!(ss >> count) || count == 0 || count > DiplomacyState::MaxStandings) {
            if (outError) *outError = "Malformed standings count";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            EmpireStanding st;
            if (!(ss >> token) || token != "st" || !(ss >> st.empireId >> st.betrayals >> st.lastBetrayalTurn)) {
                if (outError) *outError = "Malformed standing row";
                return false;
            }
            if (!validMajor(st.empireId) || st.betrayals < 0 || st.betrayals > 1000 ||
                st.lastBetrayalTurn < -1 || st.lastBetrayalTurn > temp.currentTurn ||
                (!temp.standings.empty() && temp.standings.back().empireId >= st.empireId))
            {
                if (outError) *outError = "Out-of-range or unsorted standing row";
                return false;
            }
            temp.standings.push_back(st);
        }
        if (!(ss >> token)) {
            if (outError) *outError = "Missing END_BOTF_DIPLOMACY terminal token";
            return false;
        }
    }
    if (token == "defenseCalls") {
        size_t count = 0;
        if (!(ss >> count) || count == 0 || count > DiplomacyState::MaxDefenseCalls) {
            if (outError) *outError = "Malformed defenseCalls count";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            DefenseCall c;
            if (!(ss >> token) || token != "dc" || !(ss >> c.ally >> c.defender >> c.aggressor >> c.issuedTurn >> c.dueTurn)) {
                if (outError) *outError = "Malformed defense call row";
                return false;
            }
            if (!validMajor(c.ally) || !validMajor(c.defender) || !validMajor(c.aggressor) ||
                c.ally == c.defender || c.ally == c.aggressor || c.defender == c.aggressor ||
                c.issuedTurn < 0 || c.issuedTurn > temp.currentTurn || c.dueTurn < c.issuedTurn ||
                c.dueTurn > c.issuedTurn + 100)
            {
                if (outError) *outError = "Out-of-range defense call row";
                return false;
            }
            temp.defenseCalls.push_back(c);
        }
        if (!(ss >> token)) {
            if (outError) *outError = "Missing END_BOTF_DIPLOMACY terminal token";
            return false;
        }
    }
    if (token != "END_BOTF_DIPLOMACY") {
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
