// F5 balance evidence (20 Nov 2026): AI-vs-AI soaks on every registered preset. Not a test suite (no add_test): a report.
// F5 round 2: observer mode (GameState::observerMode) by default, so the AI plays the player's seat too and a campaign runs
// to a victory of any type; BOTF_SOAK_PASSIVE=1 restores round 1's passive player seat (no orders), where the rival AIs decide the campaign; every turn it reads victoryProgress for every
// empire still standing and records, per empire and condition, the best percent and the first turn it reached 100 or was
// achieved. A campaign stops at its outcome or the turn cap.
//   BotfAiSoak [turns=300] [seeds=6] [preset-id ...]
// Output: one "GAME" line per campaign, one "EMP" line per empire, then a "PRESET" summary per preset. Rules are untouched:
// this reads the core, it changes nothing.
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "EmpireRules.h"
#include "GameplayInternal.h"
#include "Presets.h"
#include "Simulation.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace botf;

struct EmpireTrack {
    int id = 0;
    std::string name;
    int best[5] = {0, 0, 0, 0, 0};
    int first[5] = {0, 0, 0, 0, 0};
    int colonies = 0, peakColonies = 0, eliminatedTurn = 0;
    // P2 breakdown (BOTF_SOAK_GAINS): how the empire's colonies came, and its economy at turn 100.
    std::array<int, 14> queueFront{}; std::map<int, int> structureFront; int warTurns = 0, warsDeclaredOn = 0, settled = 0, joined = 0, taken = 0, lost = 0, colonyShipsSeen = 0, members100 = 0, colonies100 = 0, pop100 = 0;
    long long credits100 = 0;
};

const char* condName(int c) {
    static const char* n[5] = {"conquest", "domination", "scientific", "diplomatic", "vendetta"};
    return n[c];
}

// Round 5 (22 Nov 2026): BOTF_SOAK_SCRIPT=1 seats a scripted aggressive player in place of the observer AI (a measurement tool
// for "how fast does a strong player win by conquest"; BALANCE-PROPOSALS-F5.md Round 8). It issues orders through submit()
// like the client, every turn:
// - economy: Balanced governors; research Construction-first to turn 40 (the cheapest Classic warships need Construction,
//   Biotech or Computers 2), Biotech-first after; a refinery on every dilithium deposit (each powered refinery is one empire
//   shipbuilding slot), shipyards on every colony from turn 40, ships only in the slots' worth of best yards, put at the head
//   of the queue and rushed with credits (300 kept back);
// - expansion: up to three colony ships before turn 40, one at a time to turn 200 while free systems remain; gifts,
//   friendship and membership offers to every minor in contact (the AI's own sequence);
// - war from turn 40: warships and transports (at least 4 transports, one per 3 warships; caps 40 and 16; no new warships
//   while the treasury is under 2,000 and falling). One strike force (BOTF_SOAK_STRIKE, default 6) gathers at the capital,
//   then war on the nearest contacted rival. It beats that rival's armed stacks first (a stack dropped after 15 turns of
//   chasing), then goes to its nearest reachable world; transports land together (C1) once three are there and no armed
//   defender is left; each world taken keeps one warship as garrison while the force is above strike size. Peace (with
//   credits under modern diplomacy) is offered to everyone else every 5 turns. When the rival is gone, the next.
// BOTF_SOAK_FLEETCAP=<n> changes the warship cap. BOTF_SOAK_TRACE_SCRIPT=1 prints the script's state every 25 turns.
// The SCRIPTEND line per campaign gives the end turn, first launch and war, invasions, rivals' elimination turns, worlds taken
// and lost by empire, and minor worlds that seceded.
struct ScriptMem { std::uint64_t nextId = 0; int target = 0; int targetSystem = 0; int warFrom = 0; int strikeSize = 6; int fleetCap = 40; bool launched = false; int firstLaunch = 0; int taken = 0; int lastCredits = 0; int dbgThere = 0, dbgDef = 0, dbgLift = 0, dbgLiftThere = 0, invBlocked = 0, invFailed = 0, invWon = 0, lastAtk = 0, lastDef = 0; bool dbgStation = false; int homeDefence = 0; int peaceProposals = 0; int huntFleet = 0; int huntSince = 0; std::map<int, int> huntSkip; std::map<int, int> garrison; std::set<int> takenFromTarget; std::map<int, int> lostTo, tookFrom; int seceded = 0; std::int64_t minCredits = 0; int debtTurns = 0; };

bool atWarWith(const GameState& s, int a, int b) {
    return std::any_of(s.majorRelations.begin(), s.majorRelations.end(), [&](const MajorRelation& r) {
        return r.atWar && ((r.first == a && r.second == b) || (r.first == b && r.second == a)); });
}

void scriptTurn(GameState& s, ScriptMem& m) {
    const int me = s.playerEmpire;
    m.minCredits = std::min<std::int64_t>(m.minCredits, s.credits); if (s.credits < 0) ++m.debtTurns;
    const std::int64_t creditsPrev = m.lastCredits; m.lastCredits = static_cast<int>(std::clamp<std::int64_t>(s.credits, INT_MIN, INT_MAX));
    auto send = [&](Command c) {
        m.nextId = std::max(m.nextId, s.lastAcceptedCommand) + 1; c.id = m.nextId;
        try { submit(s, c); return true; } catch (const std::exception& e) {
            if (std::getenv("BOTF_SOAK_TRACE_SCRIPT")) std::cout << "  order " << static_cast<int>(c.type) << " refused turn " << s.turn << ": " << e.what() << std::endl;
            return false; }
    };
    // Research: a warship needs Propulsion and Weapons 1 and a class whose tech is met. The cheapest Classic warship classes
    // need Construction 2, Biotech 2 or Computers 2, the heavier ones Biotech 3-9. Construction leads to turn 40, Biotech after.
    if (s.turn % 10 == 0) {
        Command r; r.type = CommandType::AllocateResearch;
        r.researchAllocation = s.turn < 40 ? std::array<int, ResearchFields>{20, 15, 45, 5, 5, 10} : std::array<int, ResearchFields>{40, 15, 20, 5, 10, 10};
        bool same = true; for (int i = 0; i < ResearchFields; ++i) same &= s.technologies[i].allocation == r.researchAllocation[i];
        if (!same) send(r);
    }
    // Minor races: gifts to 80, friendship from 50, then membership (the AI's own sequence; a strong player courts every minor).
    {
        const std::vector<Minor> minorsNow = s.minors;
        const std::vector<DiplomaticOffer> offers = s.diplomacy;
        for (const auto& mi : minorsNow) {
            if (!mi.contact[me] || mi.memberOf != 0 || std::any_of(offers.begin(), offers.end(), [&](const DiplomaticOffer& o) { return o.empireId == me && o.systemId == mi.systemId; })) continue;
            Command d; d.systemId = mi.systemId;
            if (mi.friendshipTurn[me] < 0 && mi.attitude[me] >= 50) d.type = CommandType::OfferFriendship;
            else if (mi.attitude[me] < 80 && s.credits >= minorGiftCost()) d.type = CommandType::SendGift;
            else if (mi.attitude[me] >= 80 && mi.friendshipTurn[me] >= 0 && mi.friendshipTurn[me] < s.turn) d.type = CommandType::OfferMembership;
            else continue;
            send(d);
        }
    }
    std::vector<const Colony*> mine;
    for (const auto& c : s.colonies) if (c.ownerId == me) mine.push_back(&c);
    if (mine.empty()) return;
    const Colony* capital = *std::max_element(mine.begin(), mine.end(), [](const Colony* a, const Colony* b) { return a->population < b->population; });
    const System* capSys = gameplay::systemById(s, capital->systemId);
    const int capX = capSys->x, capY = capSys->y, capId = capital->systemId;
    auto dist = [](int ax, int ay, int bx, int by) { return gameplay::chebyshev(ax, ay, bx, by); };
    // Economy. Snapshot ids first: submit() replaces the state, so pointers into it do not survive a send.
    struct ColSnap { int id, shipyards, queueSize; bool shipQueued, yardQueued; GovernorPriority w, q; int industry; bool refineryOk; };
    std::vector<ColSnap> cols;
    int colonyShips = 0, warships = 0, transports = 0, queuedColony = 0;
    for (const auto& f : s.fleets) if (f.ownerId == me && f.hull > 0) {
        colonyShips += f.colonyShip; warships += f.shipClass == Building::Destroyer; transports += f.shipClass == Building::Transport; }
    for (const auto* c : mine) {
        ColSnap k{c->systemId, c->shipyards, static_cast<int>(c->queue.size()), false, false, c->workforcePriority, c->queuePriority, std::max(1, economy(s, *c).industry), empire_rules::enabled(s) && empire_rules::refineryBlocker(s, *c).empty()};
        for (const auto& q : c->queue) { queuedColony += q.product == Building::ColonyShip; k.shipQueued |= isShip(q.product); k.yardQueued |= q.product == Building::Shipyard; }
        cols.push_back(k);
    }
    // Shipbuilding slots: one per powered dilithium refinery under empire rules (one in all before). A ship at the head of a
    // queue without a slot halts that colony's construction, so ships go only to the slots' worth of best yards.
    const int slots = empire_rules::enabled(s) ? empire_rules::dilithiumStatus(s, me).powered : 1;
    std::set<int> shipYards;
    {
        std::vector<const ColSnap*> yards;
        for (const auto& k : cols) if (k.shipyards > 0) yards.push_back(&k);
        std::sort(yards.begin(), yards.end(), [](const ColSnap* a, const ColSnap* b) { return a->industry != b->industry ? a->industry > b->industry : a->id < b->id; });
        for (int i = 0; i < static_cast<int>(yards.size()) && i < slots; ++i) shipYards.insert(yards[i]->id);
    }
    int freeSystems = 0;
    for (const auto& sys : s.systems)
        if (!colonyAt(s, sys.id) && !minorAt(s, sys.id) && explored(s, sys.x, sys.y) && systemCapacity(s, sys.id) > 0) ++freeSystems;
    for (const auto& k : cols) {
        // D1: in debt the queue governor is switched off (no new buildings with upkeep) until the treasury recovers.
        const auto wantQ = s.credits < 0 ? GovernorPriority::Manual : GovernorPriority::Balanced;
        if (k.w != GovernorPriority::Balanced || k.q != wantQ) {
            Command g; g.type = CommandType::SetColonyGovernor; g.systemId = k.id;
            g.governorWorkforce = GovernorPriority::Balanced; g.governorQueue = wantQ; send(g);
        }
        if (k.shipyards == 0) {
            if ((k.id == capId || s.turn >= 40) && !k.yardQueued) { Command e; e.type = CommandType::EnqueueBuilding; e.systemId = k.id; e.building = Building::Shipyard; send(e); }
            continue;
        }
        if (std::getenv("BOTF_SOAK_TRACE_SCRIPT") && s.turn % 25 == 0) std::cout << "  col " << k.id << " yards " << k.shipyards << " q " << k.queueSize << " shipq " << k.shipQueued << " block [" << constructionBlocker(s, *colonyAt(s, k.id)) << "] rush " << rushCost(s, k.id) << std::endl;
        if (k.refineryOk) {   // every dilithium deposit gets a refinery, first in the queue
            Command e; e.type = CommandType::EnqueueBuilding; e.systemId = k.id; e.building = Building::DilithiumRefinery;
            if (send(e) && k.queueSize > 0) { Command pr; pr.type = CommandType::PrioritizeLastConstruction; pr.systemId = k.id; send(pr); }
            continue;
        }
        if (!shipYards.count(k.id)) {   // a ship stuck at the head of a slotless yard is taken off so the colony keeps building
            const auto* c = colonyAt(s, k.id);
            if (c && !c->queue.empty() && isShip(c->queue.front().product) && !constructionBlocker(s, *c).empty()) {
                Command r; r.type = CommandType::RemoveConstruction; r.systemId = k.id; r.queueIndex = 0; send(r);
            }
            continue;
        }
        if (k.shipQueued) continue;
        Building pick = Building::Farm;
        if (s.turn < 40) { if (colonyShips + queuedColony < 3) pick = Building::ColonyShip; }
        else {
            // Transports: at least 4, and one per three warships. Warships stop while upkeep outruns income; transports do not.
            // Caps: 40 warships and 16 transports (upkeep, and the game's 256-fleet limit shared by every empire).
            const bool broke = s.credits < 2000 && (s.credits < creditsPrev || s.credits < 0);
            pick = transports < std::min(16, std::max(4, warships / 3)) ? Building::Transport
                : broke || warships >= m.fleetCap ? Building::Farm : Building::Destroyer;
            if (colonyShips + queuedColony < 1 && s.turn < 200 && k.id == capId && freeSystems > 0) pick = Building::ColonyShip;
        }
        if (pick == Building::Farm) continue;
        Command e; e.type = CommandType::EnqueueBuilding; e.systemId = k.id; e.building = pick;
        // A ship needs a named class. Colony ships and transports: the cheapest legal one. Warships: the costliest legal class
        // this yard finishes inside 12 turns of its industry, else the cheapest (a player builds the best hull it can afford).
        int bestCost = -1, cheapest = -1, cheapRef = 0;
        for (const auto& ch : constructionChoices(s, k.id)) {
            if (ch.product != pick || !ch.blocker.empty()) continue;
            if (cheapest < 0 || ch.industryCost < cheapest) { cheapest = ch.industryCost; cheapRef = ch.referenceId; }
            if (pick == Building::Destroyer && ch.industryCost <= 12 * k.industry && ch.industryCost > bestCost) { bestCost = ch.industryCost; e.referenceId = ch.referenceId; }
        }
        if (bestCost < 0 && cheapest >= 0) { bestCost = cheapest; e.referenceId = cheapRef; }
        if (std::getenv("BOTF_SOAK_TRACE_SCRIPT") && s.turn % 25 == 0) {
            std::cout << "  yard " << k.id << " pick " << static_cast<int>(pick) << " cost " << bestCost << " ind " << k.industry << " choices";
            for (const auto& ch : constructionChoices(s, k.id)) if (isShip(ch.product)) std::cout << " " << static_cast<int>(ch.product) << ":" << ch.referenceId << (ch.blocker.empty() ? "" : "x");
            for (const auto& ch : constructionChoices(s, k.id)) if (ch.product == Building::Destroyer && !ch.blocker.empty()) std::cout << " [" << ch.referenceId << ":" << ch.industryCost << ":" << ch.blocker << "]";
            std::cout << " tech"; for (int i = 0; i < ResearchFields; ++i) std::cout << " " << s.technologies[i].level << "/" << s.technologies[i].allocation;
            std::cout << " rp " << s.research;
            std::cout << std::endl;
        }
        if (bestCost < 0) continue;
        if (send(e)) {
            if (pick == Building::ColonyShip) ++queuedColony; else if (pick == Building::Transport) ++transports; else ++warships;
            // Ships go to the front of the queue, ahead of what the governor queued.
            if (k.queueSize > 0) { Command pr; pr.type = CommandType::PrioritizeLastConstruction; pr.systemId = k.id; send(pr); }
        }
    }
    // Credits rush the ship at the front of a yard's queue (a player spends the treasury on the war), keeping 300 in reserve.
    for (const auto& k : cols) {
        const auto* c = colonyAt(s, k.id);
        if (!c || c->queue.empty() || !isShip(c->queue.front().product) || !constructionBlocker(s, *c).empty()) continue;
        const int cost = rushCost(s, k.id);
        if (cost > 0 && s.credits - cost >= 300) { Command b; b.type = CommandType::BuyConstruction; b.systemId = k.id; send(b); }
    }
    // Scouts and colony ships.
    std::set<int> claimed, heading;
    for (const auto& c : s.colonies) claimed.insert(c.systemId);
    const std::vector<Fleet> fleets = s.fleets;
    for (const auto& f : fleets) {
        if (f.ownerId != me || f.hull <= 0 || !f.colonyShip || (f.x == f.targetX && f.y == f.targetY)) continue;
        if (const auto* t = gameplay::systemAtCell(s, f.targetX, f.targetY)) heading.insert(t->id);
    }
    for (const auto& f : fleets) {
        if (f.ownerId != me || f.hull <= 0) continue;
        const bool idle = f.x == f.targetX && f.y == f.targetY;
        if (f.colonyShip) {
            if (!idle || f.colonize) continue;
            const System* here = gameplay::systemAtCell(s, f.x, f.y);
            if (here && !claimed.count(here->id) && !minorAt(s, here->id)) {
                const int hereId = here->id;   // send() replaces the state; no pointer into it survives the call
                Command c; c.type = CommandType::Colonize; c.fleetId = f.id; if (send(c)) { claimed.insert(hereId); continue; }
            }
            int bx = -1, by = -1, bid = 0, bd = 1 << 20;
            for (const auto& sys : s.systems) {
                if (claimed.count(sys.id) || heading.count(sys.id) || minorAt(s, sys.id) || !explored(s, sys.x, sys.y) || systemCapacity(s, sys.id) <= 0) continue;
                const int d = dist(f.x, f.y, sys.x, sys.y);
                if (d < bd && !route(s, f, sys.x, sys.y).empty()) { bd = d; bx = sys.x; by = sys.y; bid = sys.id; }
            }
            if (bid) { Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = bx; c.targetY = by; if (send(c)) heading.insert(bid); }
        } else if (f.shipClass == Building::Scout && idle) {
            int bx = -1, by = -1, bd = 1 << 20;
            for (const auto& sys : s.systems) {
                if (explored(s, sys.x, sys.y)) continue;
                const int d = dist(f.x, f.y, sys.x, sys.y);
                if (d < bd && !route(s, f, sys.x, sys.y).empty()) { bd = d; bx = sys.x; by = sys.y; }
            }
            if (bx >= 0) { Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = bx; c.targetY = by; send(c); }
        }
    }
    // A target that has lost its last world is done.
    if (m.target && empireEliminated(s, m.target)) { m.target = 0; m.targetSystem = 0; m.takenFromTarget.clear(); m.garrison.clear(); }
    // Target: the nearest contacted rival (war needs first contact; seed 7000 launched at turn 68 and could not declare on the
    // uncontacted Cardassians until 229). With no rival met yet, the nearest rival of all, as before, so the force still gathers.
    const auto contacted = [&](int id) { const auto* r = majorRelation(s, me, id); return r && r->contact; };
    const auto pickNearest = [&](bool needContact) {
        int best = 0, bd = 1 << 20;
        for (const auto& c : s.colonies) {
            if (c.ownerId == me || c.ownerId <= 0 || (needContact && !contacted(c.ownerId))) continue;
            if (std::none_of(s.rivals.begin(), s.rivals.end(), [&](const EmpireState& r) { return r.id == c.ownerId; })) continue;
            const auto* sys = gameplay::systemById(s, c.systemId);
            if (!sys) continue;
            const int d = dist(capX, capY, sys->x, sys->y);
            if (d < bd) { bd = d; best = c.ownerId; }
        }
        return best;
    };
    if (m.target && !contacted(m.target) && !atWarWith(s, me, m.target)) {   // swap to a met rival once there is one
        if (const int met = pickNearest(true)) { m.target = met; m.targetSystem = 0; }
    }
    if (!m.target) { m.target = pickNearest(true); if (!m.target) m.target = pickNearest(false); }
    // Diplomacy: peace with all but the target.
    if (s.turn % 5 == 0)
        for (int id : internal::activeEmpires(s)) {
            if (id == me || id == m.target || empireEliminated(s, id) || !atWarWith(s, me, id)) continue;
            Command c; c.type = CommandType::OfferPeace; c.targetEmpire = id;
            if (s.campaignExpansionEnabled && s.modernEnginesActive) {   // modern diplomacy: a peace proposal with credits on the table
                c.type = CommandType::SubmitDiplomaticProposal; c.proposalTreatyType = diplomacy::TreatyType::Peace;
                c.proposalCreditOffer = std::clamp<std::int64_t>(s.credits / 4, 0, 3000);
                ++m.peaceProposals;
            }
            send(c);
        }
    if (!m.target || s.turn < 40) return;
    // Garrisons: a world taken from the target keeps one warship while the force has more than strikeSize, so the target's
    // transports cannot walk back in behind the force (seed 7131: the Romulans went from 1 world to 8 that way, turns 400-650).
    if (m.target == 0) m.garrison.clear();
    for (auto it = m.garrison.begin(); it != m.garrison.end();) {
        const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& x) { return x.id == it->first; });
        const auto* c = colonyAt(s, it->second);
        if (f == s.fleets.end() || f->hull <= 0 || !c || c->ownerId != me) it = m.garrison.erase(it); else ++it;
    }
    std::vector<Fleet> force, lift;
    for (const auto& f : s.fleets) if (f.ownerId == me && f.hull > 0) {
        if (f.shipClass == Building::Destroyer && !m.garrison.count(f.id)) force.push_back(f);
        else if (f.shipClass == Building::Transport) lift.push_back(f);
    }
    for (int sysId : m.takenFromTarget) {
        if (static_cast<int>(force.size()) <= m.strikeSize) break;
        const auto* c = colonyAt(s, sysId);
        if (!c || c->ownerId != me || std::any_of(m.garrison.begin(), m.garrison.end(), [&](const auto& g) { return g.second == sysId; })) continue;
        const auto* sys = gameplay::systemById(s, sysId);
        if (!sys) continue;
        auto best = force.end(); int bd = 1 << 20;
        for (auto f = force.begin(); f != force.end(); ++f) { const int d = dist(f->x, f->y, sys->x, sys->y); if (d < bd) { bd = d; best = f; } }
        if (best == force.end()) break;
        m.garrison[best->id] = sysId;
        if (best->x != sys->x || best->y != sys->y) { Command c2; c2.type = CommandType::MoveFleet; c2.fleetId = best->id; c2.targetX = sys->x; c2.targetY = sys->y; send(c2); }
        force.erase(best);
    }
    for (const auto& [fid, sysId] : m.garrison) {   // a garrison that was pulled off station goes back
        const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& x) { return x.id == fid; });
        const auto* sys = gameplay::systemById(s, sysId);
        if (f != s.fleets.end() && sys && (f->targetX != sys->x || f->targetY != sys->y)) {
            const int fx = sys->x, fy = sys->y; Command c2; c2.type = CommandType::MoveFleet; c2.fleetId = fid; c2.targetX = fx; c2.targetY = fy; send(c2);
        }
    }
    // One strike force: it gathers at the capital until it has strikeSize warships (whether or not a war is already on), then
    // goes; if it falls below half strength it comes home to regroup.
    if (m.launched && static_cast<int>(force.size()) * 2 < m.strikeSize) m.launched = false;
    if (!m.launched) {
        if (static_cast<int>(force.size()) < m.strikeSize) {
            for (const auto& f : force) if (f.targetX != capX || f.targetY != capY) {
                Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = capX; c.targetY = capY; send(c);
            }
            for (const auto& f : lift) if ((f.targetX != capX || f.targetY != capY) && f.x == f.targetX && f.y == f.targetY) {
                Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = capX; c.targetY = capY; send(c);
            }
            return;
        }
        m.launched = true;
        if (!m.firstLaunch) m.firstLaunch = s.turn;
    }
    if (!atWarWith(s, me, m.target)) {
        Command c; c.type = CommandType::DeclareWar; c.targetEmpire = m.target;
        if (!send(c)) return;
        if (!m.warFrom) m.warFrom = s.turn;
    }
    // Fleet first: the war target's armed stacks (at least a tenth of our force's attack, reachable) are beaten before any landing,
    // nearest first. Without this the target's fleets retook each world behind the force (seed 7000: the Klingons held 3-9
    // worlds from turn 575 to 1,200 against a 41-warship force).
    {
        const Fleet* lead0 = force.empty() ? nullptr : &force.front();
        // Only the war target's stacks: raids by others are left to the worlds' own defences (chasing every raider kept the force
        // at home for hundreds of turns in a trial).
        int ourAttack = 0; for (const auto& f : force) ourAttack += fleetAttack(f);
        std::map<std::pair<int, int>, int> stack; std::map<std::pair<int, int>, int> stackFleet;
        for (const auto& f : s.fleets) {
            if (f.ownerId != m.target || f.hull <= 0 || fleetAttack(f) <= 0) continue;
            stack[{f.x, f.y}] += fleetAttack(f);
            auto& low = stackFleet[{f.x, f.y}]; low = low ? std::min(low, f.id) : f.id;
        }
        int tx = 0, ty = 0, bd = 1 << 20, foe = 0;
        for (const auto& [cell, atk] : stack) {
            const int d = lead0 ? dist(lead0->x, lead0->y, cell.first, cell.second) : 0;
            const Fleet probe = lead0 ? *lead0 : Fleet{};
            const bool reach = !lead0 || d == 0 || !route(s, probe, cell.first, cell.second).empty();
            const auto skip = m.huntSkip.find(stackFleet[cell]);
            if (skip != m.huntSkip.end() && skip->second > s.turn) continue;
            if (atk * 10 >= ourAttack && reach && d < bd) { bd = d; tx = cell.first; ty = cell.second; foe = stackFleet[cell]; }
        }
        // A hunt that has not ended in 15 turns (the stack keeps moving, or sits where no battle starts) is dropped for 50 turns
        // and the force goes on to the worlds (seed 7393 chased one Cardassian stack for 865 turns).
        if (foe) {   // the stack is known by its lowest fleet id, so a stack that moves is still the same hunt
            if (m.huntFleet != foe) { m.huntFleet = foe; m.huntSince = s.turn; }
            else if (s.turn - m.huntSince > 15) { m.huntSkip[foe] = s.turn + 50; foe = 0; m.huntFleet = 0; }
        }
        if (foe) {
            ++m.homeDefence;
            for (const auto& f : force) {
                if (f.x == tx && f.y == ty) { if (f.attackTarget == 0) { Command c; c.type = CommandType::AttackFleet; c.fleetId = f.id; c.targetFleetId = foe; send(c); } }
                else if (f.targetX != tx || f.targetY != ty) { Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = tx; c.targetY = ty; send(c); }
            }
            return;
        }
    }
    // Objective: the target's world nearest the force (or the capital), reachable.
    const Fleet* lead = force.empty() ? nullptr : &force.front();
    const int ox = lead ? lead->x : capX, oy = lead ? lead->y : capY;
    int objId = 0, objX = 0, objY = 0;
    if (m.targetSystem) {
        const auto* c = colonyAt(s, m.targetSystem);
        const auto* sys = gameplay::systemById(s, m.targetSystem);
        if (c && sys && c->ownerId == m.target) { objId = sys->id; objX = sys->x; objY = sys->y; }
        else if (c && c->ownerId == me) m.takenFromTarget.insert(m.targetSystem);
    }
    if (!objId) {
        int bd = 1 << 20;
        for (const auto& c : s.colonies) {
            if (c.ownerId != m.target) continue;
            const auto* sys = gameplay::systemById(s, c.systemId);
            if (!sys) continue;
            const int d = dist(ox, oy, sys->x, sys->y);
            const bool reach = !lead || (lead->x == sys->x && lead->y == sys->y) || !route(s, *lead, sys->x, sys->y).empty();
            if (d < bd && reach) { bd = d; objId = sys->id; objX = sys->x; objY = sys->y; }
        }
        m.targetSystem = objId;
    }
    if (!objId) return;
    std::vector<int> defenders;
    for (const auto& f : s.fleets)
        if (f.ownerId != me && f.hull > 0 && f.x == objX && f.y == objY && fleetAttack(f) > 0 && atWarWith(s, me, f.ownerId)) defenders.push_back(f.id);
    int forceThere = 0;
    for (const auto& f : force) {
        if (f.x == objX && f.y == objY) {
            ++forceThere;
            if (!defenders.empty() && f.attackTarget == 0) { Command c; c.type = CommandType::AttackFleet; c.fleetId = f.id; c.targetFleetId = defenders.front(); send(c); }
        } else if (f.targetX != objX || f.targetY != objY) {
            Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = objX; c.targetY = objY; send(c);
        }
    }
    m.dbgStation = gameplay::stationDefends(s, m.target, objX, objY);
    m.dbgThere = forceThere; m.dbgDef = static_cast<int>(defenders.size()); m.dbgLift = static_cast<int>(lift.size()); m.dbgLiftThere = 0;
    for (const auto& f : lift) m.dbgLiftThere += f.x == objX && f.y == objY;
    // C1: transports landing together add their troops, so they wait at the objective until three are there (or every
    // transport we have), then all land in the same turn.
    const int landWith = std::min(3, static_cast<int>(lift.size()));
    for (const auto& f : lift) {
        if (f.x == objX && f.y == objY) {
            if (defenders.empty() && forceThere > 0 && !f.invadeSystemId && m.dbgLiftThere >= landWith) { Command c; c.type = CommandType::Invade; c.fleetId = f.id; c.systemId = objId; send(c); }
        } else if (forceThere > 0) {
            if (f.targetX != objX || f.targetY != objY) { Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = objX; c.targetY = objY; send(c); }
        } else if ((f.targetX != capX || f.targetY != capY) && f.x == f.targetX && f.y == f.targetY) {
            Command c; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = capX; c.targetY = capY; send(c);
        }
    }
}
}

int main(int argc, char** argv) {
    const int turns = argc > 1 ? std::atoi(argv[1]) : 300;
    const int seeds = argc > 2 ? std::atoi(argv[2]) : 6;
    std::vector<std::string> only;
    for (int i = 3; i < argc; ++i) only.emplace_back(argv[i]);
    for (const auto* p : presets::all()) {
        if (!only.empty() && std::find(only.begin(), only.end(), p->id) == only.end()) continue;
        if (!p->lineupPlayable && !p->lineup.empty()) {
            std::cout << "SKIP " << p->id << ": lineup not playable (" << p->lineupNote << ")" << std::endl;
            continue;
        }
        std::map<std::string, int> endings, winsBy;
        std::map<std::string, std::vector<int>> firstReach;   // "<empire> <condition>" -> first turns
        std::map<std::string, int> appearances;
        int games = 0, totalTurns = 0;
        [[maybe_unused]] int capped = 0, playerOut = 0;   // Counted for the endings breakdown; not all builds print them.
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < seeds; ++k) {
            CampaignSetup setup;
            setup.seed = 7000 + 131 * static_cast<std::uint64_t>(k);
            if (const char* pick = std::getenv("BOTF_SOAK_SEED"); pick && std::to_string(setup.seed) != pick) continue;
            setup.presetId = p->id;
            if (p->rulesBase == "classic") setup.preset = RulesetPreset::Classic;
            // The preset's new-game defaults, as the New campaign screen fills them (newCampaign itself applies only the
            // overlays and rule flags). Sizes are NewCampaignVM.SIZES.
            for (const auto& race : p->lineup) {
                std::string key = race;
                std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                setup.lineup.push_back(key);
            }
            if (setup.lineup.empty() && p->empires > 0) setup.empires = p->empires;
            if (p->era > 0) setup.era = static_cast<StartingEra>(p->era);
            const std::map<std::string, std::array<int, 3>> sizes{{"small", {16, 13, 30}}, {"medium", {22, 16, 50}}, {"large", {28, 20, 80}}, {"huge", {34, 24, 120}}};
            // Round 4 (22 Nov 2026): BOTF_SOAK_SIZE=<small|medium|large|huge> overrides the preset's galaxy size.
            const char* sizeEnv = std::getenv("BOTF_SOAK_SIZE");
            const auto sz = sizes.find(sizeEnv ? std::string(sizeEnv) : (p->size.empty() ? std::string("medium") : p->size));
            if (sz != sizes.end()) { setup.width = sz->second[0]; setup.height = sz->second[1]; setup.systems = sz->second[2]; }
            // Round 4: BOTF_SOAK_PLAYER=<empire id> seats the observer AI on that empire (2 = Klingon, the aggressive profile).
            if (const char* seat = std::getenv("BOTF_SOAK_PLAYER")) { setup.playerEmpire = std::atoi(seat); setup.playerSet = true; }
            if (!p->shape.empty()) parseGalaxyShape(p->shape, setup.galaxy.shape);
            if (!p->minorDensity.empty()) parseMinorDensity(p->minorDensity, setup.galaxy.density);
            if (!p->difficulty.empty()) parseDifficulty(p->difficulty, setup.galaxy.difficulty);
            if (const char* dEnv = std::getenv("BOTF_SOAK_DIFF")) parseDifficulty(dEnv, setup.galaxy.difficulty);   // round 4: e.g. easy, simple
            if (p->startSystems >= 0) setup.startSystems = p->startSystems;
            setup.planetsMin = p->planetsMin; setup.planetsMax = p->planetsMax;
            if (p->starTypes >= 0) setup.starTypes = p->starTypes;
            if (p->threats) setup.threats = p->threats;
            GameState state;
            try {
                state = newCampaign(setup);
            } catch (const std::exception& e) {
                std::cout << "FAIL " << p->id << " seed " << setup.seed << ": newCampaign: " << e.what() << std::endl;
                continue;
            }
            const bool scripted = std::getenv("BOTF_SOAK_SCRIPT") != nullptr;
            state.observerMode = !std::getenv("BOTF_SOAK_PASSIVE") && !scripted;
            ScriptMem mem; if (const char* strike = std::getenv("BOTF_SOAK_STRIKE")) mem.strikeSize = std::max(1, std::atoi(strike));
            if (const char* cap = std::getenv("BOTF_SOAK_FLEETCAP")) mem.fleetCap = std::max(1, std::atoi(cap));
            // C4 (20 Nov 2026): BOTF_SOAK_PLAYON=1 plays on past domination (as a winner who chose "Continue playing"), so a
            // campaign ends only by conquest or another victory type.
            if (std::getenv("BOTF_SOAK_PLAYON")) state.playingOnFrom = 1;
            // C7 (21 Nov 2026): BOTF_SOAK_NODOM=1 plays with the New Campaign domination setting off (conquest is the military win).
            if (std::getenv("BOTF_SOAK_NODOM")) state.dominationEnabled = false;
            std::map<int, EmpireTrack> track;
            for (int id : internal::activeEmpires(state)) { track[id].id = id; track[id].name = empireName(id); }
            std::string ending;
            double slowestTurn = 0;
            const bool gains = std::getenv("BOTF_SOAK_GAINS") != nullptr;
            const int snapTurn = std::getenv("BOTF_SOAK_SNAP") ? std::atoi(std::getenv("BOTF_SOAK_SNAP")) : 100;
            std::map<int, int> ownerAt;   // system id -> colony owner last turn
            std::set<int> shipsSeen;
            std::map<int, int> ownerPrev, ownerNow; std::map<std::pair<int, int>, int> captured, pairWarTurns; long long parkedTransportTurns = 0;
            for (const auto& c : state.colonies) ownerPrev[c.systemId] = c.ownerId;
            // Round 4 (22 Nov 2026): always on. Worlds that change hands between empires, and the longest run of turns with no
            // war between any two surviving majors (AI only in observer mode).
            std::map<int, int> churnPrev; for (const auto& c : state.colonies) churnPrev[c.systemId] = c.ownerId;
            long long handsChanged = 0; int peaceRun = 0, longestPeace = 0, longestPeaceEnd = 0;
            for (const auto& c : state.colonies) ownerAt[c.systemId] = c.ownerId;
            try {
                for (int t = 0; t < turns && state.outcome == CampaignOutcome::Ongoing; ++t) {
                    if (const char* dump = std::getenv("BOTF_SOAK_DUMP_TURN"); dump && std::atoi(dump) == state.turn) {
                        std::ofstream(std::string("soak-") + p->id + "-" + std::to_string(setup.seed) + "-turn" + dump + ".sav", std::ios::binary) << serialize(state);
                    }
                    if (std::getenv("BOTF_SOAK_TRACE")) std::cout << "TURN " << state.turn << " fleets " << state.fleets.size() << " colonies " << state.colonies.size() << std::endl;
                    const auto tg = std::chrono::steady_clock::now();
                    if (scripted) scriptTurn(state, mem);
                    if (scripted && std::getenv("BOTF_SOAK_TRACE_SCRIPT") && state.turn % 25 == 0) {
                        int w = 0, tr = 0, cols = 0;
                        for (const auto& f : state.fleets) if (f.ownerId == state.playerEmpire && f.hull > 0) { w += f.shipClass == Building::Destroyer; tr += f.shipClass == Building::Transport; }
                        for (const auto& c : state.colonies) cols += c.ownerId == state.playerEmpire;
                        std::cout << "SCRIPT turn " << state.turn << " colonies " << cols << " warships " << w << " transports " << tr << " target " << mem.target
                                  << " obj " << mem.targetSystem << " war " << atWarWith(state, state.playerEmpire, mem.target) << " launched " << mem.launched << " there " << mem.dbgThere << "/" << mem.dbgDef << "/" << mem.dbgLiftThere << (mem.dbgStation ? "S" : "") << " lost"; for (const auto& [id, n] : mem.lostTo) std::cout << " " << id << ":" << n; std::cout << " sec " << mem.seceded << " home " << mem.homeDefence << " pp " << mem.peaceProposals << " inv b" << mem.invBlocked << " f" << mem.invFailed << " w" << mem.invWon << " last " << mem.lastAtk << ":" << mem.lastDef << " cr " << state.credits << " wars " << std::count_if(state.majorRelations.begin(), state.majorRelations.end(), [&](const MajorRelation& r) { return r.atWar && (r.first == state.playerEmpire || r.second == state.playerEmpire); }) << " tech";
                        for (int i = 0; i < ResearchFields; ++i) std::cout << " " << state.technologies[i].level;
                        for (const auto& r : state.rivals) { std::cout << " | " << r.id << ":"; for (int i = 0; i < ResearchFields; ++i) std::cout << r.technologies[i].level; }
                        for (const auto& r : state.majorRelations) if (r.first == state.playerEmpire || r.second == state.playerEmpire)
                            std::cout << " rel" << (r.first == state.playerEmpire ? r.second : r.first) << (r.atWar ? "W" : "p") << r.trust;
                        for (int id : internal::activeEmpires(state)) {
                            int n = 0, ind = 0, pop = 0; for (const auto& c : state.colonies) if (c.ownerId == id) { ++n; ind += economy(state, c).industry; pop += c.population; }
                            std::cout << " [" << id << " c" << n << " i" << ind << " p" << pop << "]";
                        }
                        std::cout << std::endl;
                    }
                    TurnReport report;
                    try { report = advance(state); }
                    catch (const std::exception& e) {
                        if (!scripted || state.pending.empty()) throw;
                        std::cout << "SCRIPT dropped orders at turn " << state.turn << ": " << e.what() << std::endl;
                        state.pending.clear(); report = advance(state);
                    }
                    if (scripted && std::getenv("BOTF_SOAK_TRACE_SCRIPT") && state.turn % 25 == 1) {
                        const auto& l = report.ledger;
                        std::cout << "LEDGER turn " << state.turn << " orders " << l.orders << " colony " << l.colonyIncome << " trade " << l.tradeRouteIncome
                                  << " treaty " << l.treatyDividends << " bldg " << l.buildingUpkeep << " fleet " << l.fleetUpkeep << " station " << l.stationUpkeep
                                  << " other " << l.other << " net " << l.net << std::endl;
                    }
                    if (scripted) for (const auto& inv : report.invasions) if (inv.attackerOwner == state.playerEmpire) {
                        if (inv.attackStrength == 0) ++mem.invBlocked; else if (inv.conquered) ++mem.invWon; else ++mem.invFailed;
                        if (inv.attackStrength) { mem.lastAtk = inv.attackStrength; mem.lastDef = inv.defenseStrength; }
                    }
                    // Runaway guard (dominion-war seed 7655 grew to 10 GB in the first run): a turn over 20 s, or a campaign
                    // with an absurd fleet count, is recorded with its sizes and the campaign stops there.
                    const double turnSecs = std::chrono::duration<double>(std::chrono::steady_clock::now() - tg).count();
                    slowestTurn = std::max(slowestTurn, turnSecs);
                    if (turnSecs > 20.0 || state.fleets.size() > 20000) {
                        std::cout << "RUNAWAY " << p->id << " seed " << setup.seed << " turn " << state.turn << ": " << turnSecs << " s, fleets "
                                  << state.fleets.size() << ", colonies " << state.colonies.size() << ", stations "
                                  << state.stations.size() << ", events this turn " << report.events.size() << std::endl;
                        throw std::runtime_error("runaway");
                    }
                    {
                        std::map<int, int> nowOwners; for (const auto& c : state.colonies) nowOwners[c.systemId] = c.ownerId;
                        for (const auto& [sys, owner] : nowOwners) { auto w = churnPrev.find(sys); if (w != churnPrev.end() && w->second != owner && w->second > 0 && owner > 0) {
                            ++handsChanged;
                            if (scripted && w->second == state.playerEmpire) ++mem.lostTo[owner];   // worlds the scripted player lost, by taker
                            if (scripted && owner == state.playerEmpire) ++mem.tookFrom[w->second];
                        } }
                        if (scripted) for (const auto& [sys, owner] : churnPrev)   // a world of ours that is no colony now: a minor seceded
                            if (owner == state.playerEmpire && !nowOwners.count(sys)) ++mem.seceded;
                        std::set<int> alive; for (const auto& [sys, owner] : nowOwners) alive.insert(owner);
                        churnPrev = std::move(nowOwners);
                        const bool anyWar = std::any_of(state.majorRelations.begin(), state.majorRelations.end(), [&](const MajorRelation& r) {
                            return r.atWar && alive.count(r.first) && alive.count(r.second); });
                        peaceRun = anyWar ? 0 : peaceRun + 1;
                        if (peaceRun > longestPeace) { longestPeace = peaceRun; longestPeaceEnd = state.turn; }
                    }
                    if (gains) {
                        std::map<int, int> now;
                        for (const auto& c : state.colonies) now[c.systemId] = c.ownerId;
                        for (const auto& [sys, owner] : now) {
                            const auto was = ownerAt.find(sys);
                            const bool minorWorld = std::any_of(state.minors.begin(), state.minors.end(), [&](const Minor& m) { return m.systemId == sys; });
                            if (was == ownerAt.end()) { if (track.count(owner)) ++(minorWorld ? track[owner].joined : track[owner].settled); }
                            else if (was->second != owner) { if (track.count(owner)) ++track[owner].taken; if (track.count(was->second)) ++track[was->second].lost; }
                        }
                        for (const auto& [sys, owner] : ownerAt) if (!now.count(sys) && track.count(owner)) ++track[owner].lost;
                        ownerAt = std::move(now);
                        for (const auto& f : state.fleets)
                            if (f.colonyShip && track.count(f.ownerId) && shipsSeen.insert(f.id).second) ++track[f.ownerId].colonyShipsSeen;
                        for (auto& [id, tr] : track)
                            if (std::any_of(state.majorRelations.begin(), state.majorRelations.end(), [&, id = id](const MajorRelation& r) {
                                    return r.atWar && (r.first == id || r.second == id); })) ++tr.warTurns;
                        for (const auto& c : state.colonies)
                            if (track.count(c.ownerId) && !c.queue.empty()) {
                                ++track[c.ownerId].queueFront[std::clamp(static_cast<int>(c.queue.front().product), 0, 13)];
                                if (c.queue.front().product == Building::Structure) ++track[c.ownerId].structureFront[c.queue.front().referenceId];
                            }
                        if (state.turn == snapTurn)
                            for (auto& [id, tr] : track) {
                                tr.credits100 = empire(state, id).credits;
                                for (const auto& c : state.colonies) if (c.ownerId == id) { ++tr.colonies100; tr.pop100 += c.population; }
                                for (const auto& m : state.minors) tr.members100 += m.memberOf == id;
                            }
                    }
                    // Conquest diagnosis (21 Nov 2026, BOTF_SOAK_WARS=<every n turns>): who is left, how strong, and what stands
                    // between each surviving pair (war and since when, active treaties, worlds taken from each other so far).
                    if (const char* wars = std::getenv("BOTF_SOAK_WARS")) {
                        for (const auto& c : state.colonies) ownerNow[c.systemId] = c.ownerId;
                        for (const auto& [sys, owner] : ownerNow) {
                            const auto was = ownerPrev.find(sys);
                            if (was != ownerPrev.end() && was->second != owner && was->second > 0 && owner > 0) ++captured[{owner, was->second}];
                        }
                        ownerPrev = ownerNow; ownerNow.clear();
                        for (const auto& r : state.majorRelations) if (r.atWar) ++pairWarTurns[{std::min(r.first, r.second), std::max(r.first, r.second)}];
                        // Transports idle at an enemy colony they do not assault (the stuck-transport bug), counted per turn.
                        for (const auto& f : state.fleets) {
                            if (f.shipClass != Building::Transport || f.hull <= 0 || f.invadeSystemId || f.x != f.targetX || f.y != f.targetY) continue;
                            for (const auto& c : state.colonies) {
                                const auto* sys = gameplay::systemById(state, c.systemId);
                                if (!sys || sys->x != f.x || sys->y != f.y || c.ownerId == f.ownerId) continue;
                                const bool war = std::any_of(state.majorRelations.begin(), state.majorRelations.end(), [&](const MajorRelation& r) {
                                    return r.atWar && ((r.first == f.ownerId && r.second == c.ownerId) || (r.second == f.ownerId && r.first == c.ownerId)); });
                                if (war) ++parkedTransportTurns;
                            }
                        }
                        const int every = std::max(1, std::atoi(wars));
                        if (state.turn % every == 0 || state.outcome != CampaignOutcome::Ongoing) {
                            std::map<int, int> cols, fleets;
                            for (const auto& c : state.colonies) ++cols[c.ownerId];
                            for (const auto& f : state.fleets) { ++fleets[f.ownerId]; }
                            std::cout << "WARS " << p->id << " seed " << setup.seed << " turn " << state.turn << " systems " << state.systems.size() << " colonies " << state.colonies.size()
                                      << " parked-transport-turns " << parkedTransportTurns << " |";
                            for (const auto& [id, n] : cols) std::cout << " " << empireName(id) << " c" << n << " f" << fleets[id];
                            std::cout << std::endl;
                            // Stuck sites (21 Nov 2026): every hostile world where an empire's transports sit at war without
                            // landing: how many, their troops as the AI reckons them, the world's real ground defence, and what
                            // guards it (a station or armed ships of its owner block any landing; enemy warships make the AI wait).
                            std::map<std::pair<int, int>, std::array<int, 2>> stuck;   // {owner, system} -> {transports, troops}
                            for (const auto& f : state.fleets) {
                                if (f.shipClass != Building::Transport || f.hull <= 0 || f.invadeSystemId) continue;
                                const auto* c = colonyAt(state, [&] { for (const auto& sy : state.systems) if (sy.x == f.x && sy.y == f.y) return sy.id; return -1; }());
                                if (!c || c->ownerId == f.ownerId || !combatAllowed(state, f.ownerId, c->ownerId)) continue;
                                auto& e = stuck[{f.ownerId, c->systemId}];
                                ++e[0];
                                e[1] += 8 + f.hull + 2 * empire(state, f.ownerId).technologies[5].level;
                            }
                            for (const auto& [site, v] : stuck) {
                                const auto* c = colonyAt(state, site.second);
                                const auto* sy = gameplay::systemById(state, site.second);
                                const int bat = state.advancedRulesEnabled ? economy(state, *c).poweredBatteries : 0;
                                const int aiDef = (8 + c->population / InvasionPopDefenseDivisor + c->morale / 10 + bat * 12 * content::balance().batteryDefensePercent / 100) * content::balance().shieldDefensePercent / 100;
                                int guards = 0, foes = 0;
                                for (const auto& o : state.fleets) if (o.hull > 0 && o.x == sy->x && o.y == sy->y && fleetAttack(o) > 0 && o.shipClass != Building::Transport) {
                                    if (o.ownerId == c->ownerId) ++guards;
                                    else if (combatAllowed(state, site.first, o.ownerId)) ++foes;
                                }
                                int own = 0;
                                for (const auto& o : state.fleets) if (o.hull > 0 && o.x == sy->x && o.y == sy->y && o.ownerId == site.first && fleetAttack(o) > 0 && o.shipClass != Building::Transport) ++own;
                                const auto* st = stationAt(state, sy->x, sy->y);
                                std::cout << "  STUCK " << empireName(site.first) << " at " << sy->name << " (" << empireName(c->ownerId) << " pop " << c->population
                                          << " morale " << c->morale << " bat " << bat << "): transports " << v[0] << " troops " << v[1] << " ai-def " << aiDef
                                          << " station " << (st && st->hull > 0 ? (st->ownerId == c->ownerId ? "theirs" : "other") : "none")
                                          << " their-armed " << guards << " other-armed " << foes << " our-armed " << own << std::endl;
                            }
                            // Last worlds (21 Nov 2026): an empire down to two colonies, each world's defence, and what each enemy
                            // at war with it has: transports (troops, at the world, heading there), warships, task forces.
                            for (const auto& [id, n] : cols) {
                                if (n > 2 || id <= 0) continue;
                                for (const auto& c : state.colonies) {
                                    if (c.ownerId != id) continue;
                                    const auto* sy = gameplay::systemById(state, c.systemId);
                                    const auto* st = stationAt(state, sy->x, sy->y);
                                    int guards = 0;
                                    for (const auto& o : state.fleets) if (o.hull > 0 && o.x == sy->x && o.y == sy->y && o.ownerId == id && fleetAttack(o) > 0) ++guards;
                                    std::cout << "  LAST " << empireName(id) << " " << sy->name << " pop " << c.population << " morale " << c.morale
                                              << " station " << (st && st->hull > 0 ? std::to_string(st->hull) + (st->ownerId == id ? " own" : " other") : std::string("none"))
                                              << " guards " << guards << " |";
                                    for (const auto& r : state.majorRelations) {
                                        if (!r.atWar || (r.first != id && r.second != id)) continue;
                                        const int foe = r.first == id ? r.second : r.first;
                                        int tr = 0, troops = 0, here = 0, heading = 0, war = 0, tfs = 0;
                                        for (const auto& f : state.fleets) {
                                            if (f.ownerId != foe || f.hull <= 0) continue;
                                            if (f.shipClass == Building::Transport) {
                                                ++tr; troops += 8 + f.hull + 2 * empire(state, foe).technologies[5].level;
                                                if (f.x == sy->x && f.y == sy->y) ++here;
                                                else if (f.targetX == sy->x && f.targetY == sy->y) ++heading;
                                            } else if (fleetAttack(f) > 0) ++war;
                                        }
                                        for (const auto& tf : state.taskForces) if (tf.ownerId == foe) ++tfs;
                                        std::cout << " " << empireName(foe) << " transports " << tr << " (troops " << troops << ", here " << here << ", heading " << heading
                                                  << ") warships " << war << " taskforces " << tfs << ";";
                                    }
                                    std::cout << std::endl;
                                }
                            }
                            const auto& dip = campaignDiplomacyState(state);
                            for (auto a = cols.begin(); a != cols.end(); ++a)
                                for (auto b = std::next(a); b != cols.end(); ++b) {
                                    const MajorRelation* rel = nullptr;
                                    for (const auto& r : state.majorRelations)
                                        if ((r.first == a->first && r.second == b->first) || (r.first == b->first && r.second == a->first)) rel = &r;
                                    std::cout << "  PAIR " << empireName(a->first) << " / " << empireName(b->first) << ": "
                                              << (rel ? (rel->atWar ? "WAR since " + std::to_string(rel->warStartTurn) : std::string(rel->contact ? "peace" : "no-contact")) : std::string("no-relation"))
                                              << " war-turns " << pairWarTurns[{a->first, b->first}] << " taken " << captured[{a->first, b->first}] << "/" << captured[{b->first, a->first}] << " treaties";
                                    for (const auto& tr8 : dip.activeTreaties)
                                        if (tr8.status == diplomacy::TreatyStatus::Active && !tr8.partyA.isMinor && !tr8.partyB.isMinor &&
                                            ((tr8.partyA.id == a->first && tr8.partyB.id == b->first) || (tr8.partyA.id == b->first && tr8.partyB.id == a->first)))
                                            std::cout << " " << static_cast<int>(tr8.type) << "@" << tr8.startTurn;
                                    std::cout << std::endl;
                                }
                        }
                    }
                    if (state.turn % 50 == 0) std::cout << "TICK " << p->id << " seed " << setup.seed << " turn " << state.turn << " fleets " << state.fleets.size() << std::endl;
                    for (auto& [id, tr] : track) {
                        int n = 0;
                        for (const auto& c : state.colonies) n += c.ownerId == id;
                        if (n > 0 && tr.colonies == 0 && tr.peakColonies > 0 && std::getenv("BOTF_SOAK_WARS")) {
                            // 21 Nov: an empire that had lost every world holds one again; which world, and how.
                            std::cout << "REVIVE " << p->id << " seed " << setup.seed << " turn " << state.turn << " " << tr.name << ":";
                            for (const auto& c : state.colonies) if (c.ownerId == id) {
                                const bool minorWorld = std::any_of(state.minors.begin(), state.minors.end(), [&](const Minor& m) { return m.systemId == c.systemId; });
                                std::cout << " " << gameplay::systemById(state, c.systemId)->name << (minorWorld ? " (minor world)" : "") << " pop " << c.population;
                            }
                            int fl = 0, cs = 0, tp = 0;
                            for (const auto& f : state.fleets) if (f.ownerId == id) { ++fl; cs += f.colonyShip; tp += f.shipClass == Building::Transport; }
                            std::cout << " | fleets " << fl << " colony ships " << cs << " transports " << tp << std::endl;
                        }
                        tr.colonies = n;
                        tr.peakColonies = std::max(tr.peakColonies, n);
                        if (!n) { if (!tr.eliminatedTurn) tr.eliminatedTurn = state.turn; continue; }
                        for (const auto& v : victoryProgress(state, id)) {
                            const int c = static_cast<int>(v.condition);
                            if (c < 0 || c > 4 || !v.enabled) continue;
                            tr.best[c] = std::max(tr.best[c], v.percent);
                            if (!tr.first[c] && (v.achieved || v.percent >= 100)) tr.first[c] = state.turn;
                        }
                    }
                    if (state.outcome != CampaignOutcome::Ongoing)
                        for (const auto& line : report.events)
                            if (ending.empty() && (line.find("victory") != std::string::npos || line.find("defeat") != std::string::npos)) ending = line;
                }
            } catch (const std::exception& e) {
                std::cout << "FAIL " << p->id << " seed " << setup.seed << " turn " << state.turn << ": " << e.what() << std::endl;
                continue;
            }
            ++games;
            totalTurns += state.turn;
            std::string kind = "cap";
            if (state.outcome == CampaignOutcome::Ongoing) ++capped;
            else if (ending.find("Vendetta") != std::string::npos) kind = "vendetta";
            else if (ending.find("scientific") != std::string::npos) kind = "scientific";
            else if (ending.find("diplomatic") != std::string::npos) kind = "diplomatic";
            else if (ending.find("conquest victory") != std::string::npos) kind = "conquest";
            else if (ending.find("domination victory") != std::string::npos) kind = "domination";
            else if (state.outcome == CampaignOutcome::Defeat && empireEliminated(state, state.playerEmpire)) { kind = "player-eliminated"; ++playerOut; }
            else kind = "conquest/domination";
            ++endings[kind];
            const std::string winner = state.winnerEmpire ? empireName(state.winnerEmpire) : "-";
            if (state.winnerEmpire && kind != "player-eliminated") ++winsBy[winner];
            std::cout << "CHURN " << p->id << " seed " << setup.seed << " turns " << state.turn << " hands-changed " << handsChanged << " per-100 "
                      << (state.turn > 0 ? 100.0 * handsChanged / state.turn : 0.0) << " longest-no-war " << longestPeace << " ending-turn " << longestPeaceEnd << std::endl;
            std::cout << "GAME " << p->id << " seed " << setup.seed << " turns " << state.turn << " end " << kind << " winner " << winner << " slowest-turn " << slowestTurn << "s"
                      << (ending.empty() ? "" : " | " + ending) << std::endl;
            if (scripted) {
                std::cout << "SCRIPTEND " << p->id << " seed " << setup.seed << " end " << kind << " turn " << state.turn << " first-launch " << mem.firstLaunch
                          << " first-war " << mem.warFrom << " invasions won " << mem.invWon << " failed " << mem.invFailed << " blocked " << mem.invBlocked
                          << " fleet-hunts " << mem.homeDefence << " peace-proposals " << mem.peaceProposals << " rivals-out";
                for (const auto& [id, tr] : track) if (id != state.playerEmpire) std::cout << " " << id << "@" << tr.eliminatedTurn;
                std::cout << " took-from"; for (const auto& [id, n] : mem.tookFrom) std::cout << " " << id << ":" << n;
                std::cout << " lost-to"; for (const auto& [id, n] : mem.lostTo) std::cout << " " << id << ":" << n;
                std::cout << " seceded " << mem.seceded << " min-credits " << mem.minCredits << " debt-turns " << mem.debtTurns;
                std::cout << std::endl;
            }
            for (const auto& [id, tr] : track) {
                ++appearances[tr.name];
                std::cout << "EMP " << p->id << " seed " << setup.seed << " " << tr.name << (id == state.playerEmpire ? (state.observerMode ? " (observer seat)" : scripted ? " (scripted player)" : " (passive)") : "")
                          << " colonies " << tr.colonies << " peak " << tr.peakColonies
                          << (tr.eliminatedTurn ? " out@" + std::to_string(tr.eliminatedTurn) : std::string());
                for (int c = 0; c < 5; ++c) {
                    std::cout << " " << condName(c) << " " << tr.best[c] << "%" << (tr.first[c] ? "@" + std::to_string(tr.first[c]) : std::string());
                    if (tr.first[c] && (id != state.playerEmpire || state.observerMode)) firstReach[tr.name + " " + condName(c)].push_back(tr.first[c]);
                }
                std::cout << std::endl;
                if (gains) {
                    std::cout << "GAINS " << p->id << " seed " << setup.seed << " " << tr.name << " settled " << tr.settled << " joined " << tr.joined << " taken " << tr.taken
                              << " lost " << tr.lost << " war-turns " << tr.warTurns << " colony-ships " << tr.colonyShipsSeen << " t" << snapTurn << ": colonies " << tr.colonies100 << " pop "
                              << tr.pop100 << " credits " << tr.credits100 << " member-minors " << tr.members100 << " queue-front";
                    for (int b = 0; b < 14; ++b) std::cout << ' ' << tr.queueFront[b];
                    std::cout << " structure-refs";
                    for (const auto& [ref, n] : tr.structureFront) std::cout << ' ' << ref << ':' << n;
                    std::cout << std::endl;
                }
            }
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::cout << "PRESET " << p->id << ": " << games << " games, avg " << (games ? totalTurns / games : 0) << " turns, " << secs << " s; endings";
        for (const auto& [k, n] : endings) std::cout << " " << k << "=" << n;
        std::cout << "; wins";
        for (const auto& [k, n] : winsBy) std::cout << " " << k << "=" << n;
        std::cout << "; AI first reached 100%";
        for (const auto& [k, v] : firstReach) {
            auto s = v;
            std::sort(s.begin(), s.end());
            std::cout << " [" << k << " " << s.size() << "/" << appearances[k.substr(0, k.rfind(' '))] << " median t" << s[s.size() / 2] << "]";
        }
        std::cout << std::endl;
    }
    return 0;
}
