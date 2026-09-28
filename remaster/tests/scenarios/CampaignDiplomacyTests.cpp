// Campaign-level diplomacy & intelligence scenarios (Lane B).
// Exercises the modern engines through the real turn pipeline: bug regressions
// that only show up in a campaign, plus the modern diplomacy rules documented in
// remaster/docs/DIPLOMACY.md.
#include "Simulation.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& fn, const std::string& message) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
void run(const char* name, const std::function<void()>& fn, int& passed) {
    fn(); ++passed; std::cout << "PASS " << name << '\n';
}
using botf::diplomacy::ParticipantId;
using botf::diplomacy::TreatyType;
using botf::diplomacy::TreatyDuration;

botf::MajorRelation& relation(botf::GameState& s, int a, int b) {
    if (a > b) std::swap(a, b);
    for (auto& r : s.majorRelations) if (r.first == a && r.second == b) return r;
    throw std::runtime_error("missing relation");
}
botf::EmpireState& empireOf(botf::GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    for (auto& r : s.rivals) if (r.id == id) return r;
    throw std::runtime_error("missing empire");
}
void meetEveryone(botf::GameState& s) {
    for (auto& r : s.majorRelations) r.contact = true;
}
void establish(botf::GameState& s, int a, int b, TreatyType type, TreatyDuration duration = TreatyDuration::Indefinite) {
    botf::diplomacy::TurnResolutionDelta delta;
    s.diplomacyEngineState.currentTurn = s.turn;
    check(botf::diplomacy::DiplomacyEngine::establishTreaty(s.diplomacyEngineState, ParticipantId::major(a),
        ParticipantId::major(b), type, duration, delta), "setup treaty");
}
void jumpToTurn(botf::GameState& s, int turn) {
    s.turn = turn;
    s.diplomacyEngineState.currentTurn = turn;
}
bool hasEvent(const botf::TurnReport& r, const std::string& needle) {
    return std::any_of(r.events.begin(), r.events.end(), [&](const std::string& e) { return e.find(needle) != std::string::npos; });
}
void claimSystem(botf::GameState& s, botf::Colony& colony, int newOwner) {
    colony.ownerId = newOwner;
    const auto sys = std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& x) { return x.id == colony.systemId; });
    auto& known = empireOf(s, newOwner).explored;
    known.push_back(sys->y * s.width + sys->x);
    std::sort(known.begin(), known.end());
    known.erase(std::unique(known.begin(), known.end()), known.end());
}
}

int main() {
    int passed = 0;
    try {
        run("bug 7: research theft keeps both research ledgers reconciled (Supreme era, Science focus)", [] {
            auto s = botf::generateReferenceCampaignWithEra(7, botf::StartingEra::Supreme, 2);
            s.eventsEnabled = false;
            meetEveryone(s);
            for (int i = 0; i < 3; ++i) botf::advance(s); // Let the rival accumulate unspent research
            std::int64_t stolenTotal = 0;
            for (int attempt = 0; attempt < 12 && stolenTotal == 0; ++attempt) {
                auto& intel = s.empireIntelStates[1];
                intel.budget = botf::intelligence::BudgetAllocations{};
                intel.budget.internalSecurity.percentage = 0;
                botf::intelligence::RivalAllocation alloc;
                alloc.rivalId = 2;
                alloc.espionage.percentage = 100;
                intel.budget.rivalAllocations.push_back(alloc);
                auto& rival = intel.getOrCreateRivalState(2);
                rival.focusDepartment = botf::intelligence::IntelDepartment::Science;
                rival.departmentWeights = {{1, 1, 30, 1}};
                rival.espionageHistory.assign(20, 100000);
                rival.espionageStockpile = 100000;
                rival.lastEspionageAttackTurn = -100;
                const auto attackerBefore = s.research;
                const auto opsBefore = s.intelOperations.size();
                botf::advance(s); // validate() inside advance enforces the ledger
                for (auto i = opsBefore; i < s.intelOperations.size(); ++i) {
                    const auto& op = s.intelOperations[i];
                    if (op.attackerEmpireId == 1 && op.researchStolen > 0) stolenTotal += op.researchStolen;
                }
                check(s.research >= attackerBefore + stolenTotal, "Stolen research must enter the attacker's ledger");
            }
            check(stolenTotal > 0, "A Science operation should have stolen research");
            botf::validate(s);
            check(botf::serialize(botf::deserialize(botf::serialize(s))) == botf::serialize(s), "Theft state roundtrips");
        }, passed);

        run("bug 7 soak: Supreme-era reference campaigns advance 150 turns without throwing", [] {
            for (std::uint64_t seed : {1ULL, 2ULL}) {
                for (int player : {1, 3}) {
                    auto s = botf::generateReferenceCampaignWithEra(seed, botf::StartingEra::Supreme, 5, 16, 12, 28, player);
                    for (int t = 0; t < 150 && s.outcome == botf::CampaignOutcome::Ongoing; ++t) botf::advance(s);
                    botf::validate(s);
                }
            }
        }, passed);

        run("bug 5: eliminated empires leave diplomacy and intelligence", [] {
            auto s = botf::generateReferenceCampaign(31, 3);
            s.eventsEnabled = false;
            meetEveryone(s);
            establish(s, 1, 3, TreatyType::NonAggression, TreatyDuration::Turns50);
            botf::submit(s, botf::makeSetIntelBudgetCommand(1, 60, {{2, {10, 10}}, {3, {10, 10}}}));
            botf::advance(s);
            check(botf::empireIntelState(s, 1)->budget.rivalAllocations.size() == 2, "Setup budget covers empire 3");
            for (auto& c : s.colonies) if (c.ownerId == 3) claimSystem(s, c, 1);
            std::erase_if(s.fleets, [](const botf::Fleet& f) { return f.ownerId == 3; }); // No colony ship to refound it
            std::erase_if(s.taskForces, [](const botf::TaskForce& t) { return t.ownerId == 3; });
            botf::validate(s);
            check(botf::empireEliminated(s, 3) && !botf::empireEliminated(s, 2), "Elimination detected");
            std::string why;
            check(!botf::canProposeTreaty(s, 1, 3, false, TreatyType::Friendship, &why), "Proposal to the eliminated refused");
            rejects([&] { botf::submit(s, botf::makeProposeTreatyCommand(1, 3, false, TreatyType::Trade)); }, "Proposal command accepted");
            rejects([&] { botf::submit(s, botf::makeSetIntelBudgetCommand(2, 80, {{3, {10, 10}}})); }, "Intel budget against the eliminated accepted");
            rejects([&] { botf::submit(s, botf::makeSetIntelDepartmentFocusCommand(3, 3, botf::intelligence::IntelDepartment::Science)); },
                "Intel focus on the eliminated accepted");
            botf::Command war; war.id = 4; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 3;
            rejects([&] { botf::submit(s, war); }, "War on the eliminated accepted");
            const auto turn = s.turn;
            const auto report = botf::advance(s);
            for (const auto& t : botf::activeTreaties(s, 1)) check(!t.involves(ParticipantId::major(3)), "Treaty with the eliminated survived");
            bool dissolved = false;
            for (const auto& t : s.diplomacyEngineState.treatyHistory)
                dissolved |= t.involves(ParticipantId::major(3)) && t.status == botf::diplomacy::TreatyStatus::Dissolved;
            check(dissolved, "Treaty archived as Dissolved");
            const auto* intel = botf::empireIntelState(s, 1);
            check(intel->budget.rivalAllocations.size() == 1 && intel->budget.internalSecurity.percentage == 80,
                "Budget against the eliminated returns to internal security");
            check(hasEvent(report, "agents recalled"), "Player told about recalled agents");
            for (const auto& op : s.intelOperations)
                check(op.turn <= turn || (op.attackerEmpireId != 3 && op.defenderEmpireId != 3), "Intel involving the eliminated");
        }, passed);

        run("bug 8: AI war decisions respect peace cool-downs and treaties, and are reported", [] {
            auto s = botf::generateReferenceCampaign(41);
            s.eventsEnabled = false;
            meetEveryone(s);
            jumpToTurn(s, 30);
            relation(s, 1, 2).trust = 10;
            auto report = botf::advance(s);
            check(relation(s, 1, 2).atWar, "A distrustful Klingon neighbour declares war");
            check(hasEvent(report, "Klingon Empire is now at war with us."), "War reported to the player");
            check(relation(s, 1, 2).trust == 0, "War costs 25 trust once");
            for (int i = 0; i < 4; ++i) botf::advance(s);
            check(relation(s, 1, 2).trust == 0, "No repeated war penalty while at war");

            relation(s, 1, 2).trust = 30;
            botf::Command peace; peace.id = 1; peace.type = botf::CommandType::OfferPeace; peace.targetEmpire = 2;
            botf::submit(s, peace);
            botf::advance(s);
            report = botf::advance(s);
            check(!relation(s, 1, 2).atWar && hasEvent(report, "accepted peace terms"), "Peace accepted");
            const int peaceTurn = s.turn;
            for (int i = 0; i < botf::diplomacy::WarCooldownTurns - 1; ++i) {
                relation(s, 1, 2).trust = 0; // Even at rock bottom the truce holds through the cool-down
                botf::advance(s);
                check(!relation(s, 1, 2).atWar, "Klingons re-declared war during the cool-down (turn " + std::to_string(s.turn) + ")");
            }
            check(s.turn - peaceTurn == botf::diplomacy::WarCooldownTurns - 1, "Cool-down window elapsed");

            auto bound = botf::generateReferenceCampaign(41);
            bound.eventsEnabled = false;
            meetEveryone(bound);
            jumpToTurn(bound, 60);
            establish(bound, 1, 2, TreatyType::NonAggression, TreatyDuration::Turns100);
            for (int i = 0; i < 10; ++i) {
                relation(bound, 1, 2).trust = 0;
                botf::advance(bound);
                check(!relation(bound, 1, 2).atWar, "Klingons broke a non-aggression pact");
            }
        }, passed);

        run("bug 9: modern diplomacy and intelligence orders are rejected in legacy campaigns", [] {
            auto s = botf::generateCampaign(51);
            meetEveryone(s);
            std::string why;
            check(!botf::canProposeTreaty(s, 1, 2, false, TreatyType::NonAggression, &why) && !why.empty(), "Legacy canProposeTreaty");
            rejects([&] { botf::submit(s, botf::makeProposeTreatyCommand(1, 2, false, TreatyType::NonAggression)); }, "Proposal accepted");
            rejects([&] { botf::submit(s, botf::makeRespondProposalCommand(1, 1, true)); }, "Response accepted");
            rejects([&] { botf::submit(s, botf::makeSetIntelBudgetCommand(1, 60, {{2, {20, 20}}})); }, "Intel budget accepted");
            rejects([&] { botf::submit(s, botf::makeSetIntelDepartmentFocusCommand(1, 2, botf::intelligence::IntelDepartment::Economic)); },
                "Intel focus accepted");
            check(s.pending.empty(), "Nothing queued");
        }, passed);

        run("AI-initiated proposals reach the player and are ratified through RespondDiplomaticProposal", [] {
            auto s = botf::generateReferenceCampaign(61);
            s.eventsEnabled = false;
            meetEveryone(s);
            jumpToTurn(s, 30);
            relation(s, 1, 2).trust = 90;
            botf::TurnReport report;
            for (int i = 0; i < 4 && botf::incomingProposals(s, 1).empty(); ++i) report = botf::advance(s);
            const auto incoming = botf::incomingProposals(s, 1);
            check(!incoming.empty(), "The Klingons should court a trusted neighbour");
            check(hasEvent(report, "Klingon Empire proposes"), "Proposal announced to the player");
            const auto proposal = incoming.front();
            check(proposal.dueTurn == s.turn + 2, "Human targets get two turns to answer");
            botf::submit(s, botf::makeRespondProposalCommand(s.lastAcceptedCommand + 1, proposal.id, true));
            const int trustBefore = relation(s, 1, 2).trust;
            report = botf::advance(s);
            const auto treaties = botf::activeTreaties(s, 1);
            check(std::any_of(treaties.begin(), treaties.end(), [&](const auto& t) { return t.type == proposal.treatyType; }),
                "Accepted AI proposal ratified");
            check(relation(s, 1, 2).trust >= std::min(100, trustBefore + 5) - 1, "Signing builds trust");
            check(hasEvent(report, "ratified"), "Ratification reported");
        }, passed);

        run("treaty consequences: friendship dividend, trust growth and shared exploration", [] {
            auto base = botf::generateReferenceCampaign(71);
            base.eventsEnabled = false;
            base.aiAggressionEnabled = false;
            meetEveryone(base);
            auto friends = base;
            establish(friends, 1, 2, TreatyType::Friendship);
            const auto a = botf::advance(base);
            const auto b = botf::advance(friends);
            check(b.tradeIncome - a.tradeIncome == 3, "Friendship pays 3 credits a turn");
            check(friends.credits - base.credits == 3, "Dividend credited");
            for (int i = 0; i < 9; ++i) { botf::advance(base); botf::advance(friends); }
            check(relation(friends, 1, 2).trust > relation(base, 1, 2).trust, "Friendship grows trust");

            auto affiliated = botf::generateReferenceCampaign(71);
            affiliated.aiAggressionEnabled = false;
            meetEveryone(affiliated);
            establish(affiliated, 1, 2, TreatyType::Affiliation);
            botf::advance(affiliated);
            const auto& mine = affiliated.explored;
            const auto& theirs = empireOf(affiliated, 2).explored;
            check(std::includes(mine.begin(), mine.end(), theirs.begin(), theirs.end()) &&
                  std::includes(theirs.begin(), theirs.end(), mine.begin(), mine.end()), "Affiliates share star charts");
            check(botf::sensorSharingEmpires(affiliated, 1) == std::vector<int>({2}), "Sensor-sharing helper");
            check(botf::alliedEmpires(affiliated, 1).empty() && botf::treatyForbidsAttack(affiliated, 1, 2), "Alliance/NAP helpers");
        }, passed);

        run("alliance obligations: AI allies answer calls to arms; refusing one costs the player", [] {
            auto s = botf::generateReferenceCampaign(81, 3);
            s.eventsEnabled = false;
            meetEveryone(s);
            jumpToTurn(s, 30);
            for (auto& r : s.majorRelations) r.trust = 50;
            establish(s, 2, 3, TreatyType::Alliance);
            check(botf::alliedEmpires(s, 3) == std::vector<int>({2}), "Allied helper");
            botf::Command war; war.id = 1; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 3;
            botf::submit(s, war);
            const auto report = botf::advance(s);
            check(relation(s, 1, 3).atWar, "Player at war with empire 3");
            check(relation(s, 1, 2).atWar, "Klingon ally honored the alliance");
            check(hasEvent(report, "honored its alliance"), "Player told the ally joined");

            auto p = botf::generateReferenceCampaign(82, 3);
            p.eventsEnabled = false;
            p.aiAggressionEnabled = false;
            meetEveryone(p);
            jumpToTurn(p, 30);
            for (auto& r : p.majorRelations) r.trust = 50;
            establish(p, 1, 2, TreatyType::Alliance);
            relation(p, 2, 3).atWar = true;
            relation(p, 2, 3).warStartTurn = 30;
            p.diplomacyEngineState.defenseCalls.push_back({1, 2, 3, 30, 31});
            check(botf::openCallsToArms(p, 1).size() == 1, "Open call visible to the player");
            const auto due = botf::advance(p); // Turn 31: the call falls due unanswered
            check(hasEvent(due, "refused"), "Refusal reported");
            check(relation(p, 1, 2).trust <= 50 - 20 + 2, "Refusing the call cost trust");
            check(botf::openCallsToArms(p, 1).empty(), "Call resolved");
            check(botf::alliedEmpires(p, 1).empty(), "Klingons cancelled an alliance with an unreliable ally");
        }, passed);

        run("minor races drift and neglected members secede", [] {
            auto s = botf::generateReferenceCampaign(91, 3);
            s.eventsEnabled = false;
            s.aiAggressionEnabled = false;
            check(!s.minors.empty(), "Campaign has a minor");
            meetEveryone(s);
            auto& m = s.minors.front();
            const int minorSystem = m.systemId;
            m.contact[1] = m.contact[2] = true;
            m.memberOf = 1;
            m.attitude[1] = 31;
            m.attitude[2] = 60;
            botf::Colony colony;
            colony.systemId = minorSystem;
            colony.population = 30; colony.foodWorkers = 15; colony.industryWorkers = 10; colony.researchWorkers = 5;
            s.colonies.push_back(colony);
            std::sort(s.colonies.begin(), s.colonies.end(), [](const auto& x, const auto& y) { return x.systemId < y.systemId; });
            claimSystem(s, *std::find_if(s.colonies.begin(), s.colonies.end(), [&](const auto& c) { return c.systemId == minorSystem; }), 1);
            relation(s, 1, 2).atWar = relation(s, 1, 3).atWar = true; // A two-front war neglects members
            jumpToTurn(s, 4);
            botf::syncDiplomacyState(s);
            botf::validate(s);
            const auto report = botf::advance(s); // Turn 5: council review
            check(s.minors.front().memberOf == 0 && !botf::colonyAt(s, minorSystem), "Member seceded");
            check(hasEvent(report, "seceded from our empire"), "Secession reported");
            check(s.minors.front().attitude[2] < 60, "Unattached attitudes drift toward neutral");
            bool logged = false;
            for (const auto& e : s.diplomacyEngineState.eventHistory) logged |= e.type == botf::diplomacy::DiplomaticEventType::MinorSeceded;
            check(logged, "Secession in the diplomatic log");
            check(botf::minorDiplomacy(s, minorSystem)->membershipOwner == 0, "Engine state synchronized");
        }, passed);

        run("diplomatic victory predicate: alliance blocs holding a population majority", [] {
            auto s = botf::generateReferenceCampaign(101, 3);
            s.aiAggressionEnabled = false;
            // Keep only the starter minor so the three-equal-empires arithmetic holds; roster minors
            // (MinorRaces.h) each add nominal population to the galaxy total.
            std::erase_if(s.minors, [](const botf::Minor& m) { return m.name != "Bynars"; });
            std::erase_if(s.diplomacyEngineState.minorStates, [&](const auto& ms) { return !botf::minorAt(s, ms.minorId); });
            botf::validate(s);
            meetEveryone(s);
            const auto alone = botf::diplomaticVictoryStatus(s, 1);
            check(!alone.achieved && alone.allies.empty(), "No bloc, no diplomatic victory");
            establish(s, 1, 2, TreatyType::Alliance);
            const auto bloc = botf::diplomaticVictoryStatus(s, 1);
            std::int64_t mine = 0, total = 0;
            for (const auto& c : s.colonies) { total += c.population; if (c.ownerId == 1 || c.ownerId == 2) mine += c.population; }
            for (const auto& m : s.minors) if (m.memberOf == 0) total += botf::diplomacy::IndependentMinorNominalPopulation;
            check(bloc.blocPopulation == mine && bloc.galaxyPopulation == total, "Population accounting");
            check(bloc.achieved == (mine * 2 > total) && botf::diplomaticVictoryAchieved(s, 1) == bloc.achieved, "Majority rule");
            check(bloc.achieved, "Two of three equal empires form a majority bloc");
            check(!botf::diplomaticVictoryAchieved(botf::generateCampaign(101, 3), 1), "Legacy campaigns never win diplomatically");
        }, passed);

        run("modern diplomacy campaigns resume identically across save/load", [] {
            auto s = botf::generateReferenceCampaignWithEra(111, botf::StartingEra::Developed, 5);
            auto resumed = botf::deserialize(botf::serialize(s));
            for (int t = 0; t < 90; ++t) {
                botf::advance(s);
                botf::advance(resumed);
                if (t % 30 == 29) resumed = botf::deserialize(botf::serialize(resumed));
                check(botf::serialize(s) == botf::serialize(resumed), "Diverged at turn " + std::to_string(s.turn));
                if (s.outcome != botf::CampaignOutcome::Ongoing) break;
            }
        }, passed);
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << passed << " scenarios: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ALL " << passed << " CAMPAIGN DIPLOMACY SCENARIOS PASSED\n";
    return 0;
}
