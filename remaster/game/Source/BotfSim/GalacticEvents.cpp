// Schema-13 curated galactic events: a deterministic, seeded event deck replacing
// the original trivial random events in Remastered campaigns. Events that need a
// decision offer 2-3 options (ResolveEventChoice); undecided events take their
// default after the choice window, and rival AIs decide by situation.
#include "GameplayInternal.h"
#include "Content.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace botf::gameplay {
constexpr int CuratedEventInterval = 7;
constexpr int FirstCuratedEventTurn = 6;
constexpr int EventChoiceWindow = 2;
constexpr int ActiveEventLimit = 16;
constexpr int EventKindCount = 7;

bool eventKindValid(int kind) { return kind >= 0 && kind < EventKindCount; }
bool eventHasChoice(GalacticEventKind kind) {
    return kind == GalacticEventKind::Plague || kind == GalacticEventKind::SupernovaWarning ||
        kind == GalacticEventKind::DerelictVessel || kind == GalacticEventKind::LaborUnrest ||
        kind == GalacticEventKind::ScientificBreakthrough;
}
std::int64_t plagueCureCost(const GameState& s) { return s.ruleset == Ruleset::Reference ? 150 : 60; }
std::int64_t stellarShieldCost(const GameState& s) { return s.ruleset == Ruleset::Reference ? 400 : 150; }
std::int64_t derelictSalvageCredits(const GameState& s) { return s.ruleset == Ruleset::Reference ? 250 : 100; }
std::int64_t tradeBoomBonus(const GalacticEvent& e, int colonies) {
    return std::min<std::int64_t>(150, static_cast<std::int64_t>(e.magnitude) * colonies);
}

void shuffleDeck(std::vector<int>& deck, std::uint64_t& rng) {
    for (std::size_t i = deck.size(); i > 1; --i) {
        const auto j = static_cast<std::size_t>(internal::bounded(rng, i));
        std::swap(deck[i - 1], deck[j]);
    }
}

void initializeEventDeck(GameState& s) {
    s.eventDeck.clear();
    for (int copy = 0; copy < 2; ++copy)
        for (int kind = 0; kind < EventKindCount; ++kind) s.eventDeck.push_back(kind);
    std::uint64_t g = s.seed ^ 0x9fb21c651e98df25ULL;
    shuffleDeck(s.eventDeck, g);
    s.eventDeckPosition = 0;
    s.lastCuratedEventTurn = 0;
}

void validateEvents(const GameState& s) {
    require(s.rules.curatedEvents || (s.galacticEvents.empty() && s.eventDeck.empty()),
        "Curated events are disabled by the ruleset");
    require(s.nextEventId >= 1 && s.nextEventId <= 10000000 && s.galacticEvents.size() <= ActiveEventLimit + 1,
        "Invalid galactic event identity state");
    require(s.lastCuratedEventTurn >= 0 && s.lastCuratedEventTurn <= s.turn, "Invalid curated event turn");
    require(s.eventDeck.size() <= 64 && s.eventDeckPosition >= 0 &&
        (s.eventDeck.empty() ? s.eventDeckPosition == 0 : s.eventDeckPosition < static_cast<int>(s.eventDeck.size())),
        "Invalid event deck position");
    require(!s.rules.curatedEvents || !s.eventDeck.empty(), "Curated events need an event deck");
    for (int card : s.eventDeck) require(eventKindValid(card), "Invalid event deck card");
    const auto empires = internal::activeEmpires(s);
    int prior = 0;
    for (const auto& e : s.galacticEvents) {
        require(e.id > prior && e.id < s.nextEventId, "Galactic event IDs must be unique, positive and sorted");
        prior = e.id;
        require(eventKindValid(static_cast<int>(e.kind)), "Invalid galactic event kind");
        if (e.kind == GalacticEventKind::SubspaceRift) require(e.empireId == 0, "Subspace rifts are galaxy-wide");
        else require(std::binary_search(empires.begin(), empires.end(), e.empireId), "Event empire is not a campaign empire");
        require(e.systemId == 0 || systemById(s, e.systemId), "Event system does not exist");
        require((e.x == -1 && e.y == -1) || (e.x >= 0 && e.x < s.width && e.y >= 0 && e.y < s.height), "Event sector out of bounds");
        require(e.startTurn >= 0 && e.startTurn <= s.turn && e.endTurn >= e.startTurn && e.endTurn <= s.turn + 100,
            "Invalid event timing");
        const int options = static_cast<int>(eventChoices(e.kind).size());
        require(eventHasChoice(e.kind) ? e.choiceDeadline >= e.startTurn && e.choiceDeadline <= e.startTurn + EventChoiceWindow
                                       : e.choiceDeadline == 0, "Invalid event choice deadline");
        require(e.choice >= -1 && e.choice < options && (e.choice >= 0 || !e.choiceApplied) &&
            (eventHasChoice(e.kind) || e.choice == -1), "Invalid event choice");
        require(e.magnitude >= 0 && e.magnitude <= 100, "Invalid event magnitude");
        require(e.featureId == 0 || (e.kind == GalacticEventKind::SubspaceRift &&
            std::any_of(s.galaxyFeatures.begin(), s.galaxyFeatures.end(), [&](const GalaxyFeature& f) { return f.id == e.featureId; })),
            "Invalid event feature");
    }
}

void applyEventChoiceCommand(GameState& state, const Command& command) {
    const auto event = std::find_if(state.galacticEvents.begin(), state.galacticEvents.end(),
        [&](const GalacticEvent& e) { return e.id == command.eventId; });
    require(event != state.galacticEvents.end(), "Galactic event not found");
    require(event->empireId == state.playerEmpire, "This event does not concern our empire");
    require(event->choiceDeadline > 0 && event->choice == -1, "This event has no pending decision");
    require(command.eventOption >= 0 && command.eventOption < static_cast<int>(eventChoices(event->kind).size()),
        "Invalid event option");
    event->choice = command.eventOption;
}

// ---- helpers ----
void trimEventWorkforce(Colony& c) {
    int excess = std::max(0, c.foodWorkers + c.industryWorkers + c.researchWorkers - c.population);
    auto trim = [&excess](int& workers) { const int amount = std::min(workers, excess); workers -= amount; excess -= amount; };
    trim(c.industryWorkers); trim(c.researchWorkers); trim(c.foodWorkers);
}
Colony* eventColony(GameState& s, int systemId) {
    const auto it = std::find_if(s.colonies.begin(), s.colonies.end(), [systemId](const Colony& c) { return c.systemId == systemId; });
    return it == s.colonies.end() ? nullptr : &*it;
}
int lowestResearchField(const EmpireState& e) {
    int best = 0;
    for (int field = 1; field < ResearchFields; ++field)
        if (e.technologies[static_cast<std::size_t>(field)].level < e.technologies[static_cast<std::size_t>(best)].level) best = field;
    return best;
}
void noteEvent(const GameState& s, TurnReport& report, const GalacticEvent& e, const std::string& text) {
    ChronicleEntry entry;
    entry.category = ChronicleCategory::Events;
    entry.text = text;
    entry.eventId = e.id;
    entry.empireId = e.empireId;
    entry.systemId = e.systemId;
    entry.x = e.x; entry.y = e.y;
    if (e.systemId && entry.x < 0) if (const auto sys = systemById(s, e.systemId)) { entry.x = sys->x; entry.y = sys->y; }
    note(report, std::move(entry));
}
bool playerConcerned(const GameState& s, const GalacticEvent& e) { return e.empireId == s.playerEmpire; }

int aiEventChoice(const GameState& s, const GalacticEvent& e) {
    const auto& owner = empire(s, e.empireId);
    switch (e.kind) {
    case GalacticEventKind::Plague: return owner.credits >= 2 * plagueCureCost(s) ? 1 : 0;
    case GalacticEventKind::SupernovaWarning: return owner.credits >= 2 * stellarShieldCost(s) ? 1 : 0;
    case GalacticEventKind::DerelictVessel: {
        const int threshold = scientificVictoryThreshold(s);
        for (const auto& t : owner.technologies) if (t.level < threshold) return 0;
        return 1;
    }
    case GalacticEventKind::LaborUnrest: {
        const auto colony = colonyAt(s, e.systemId);
        const std::int64_t cost = 20 + (colony ? colony->population : 0);
        return owner.credits >= 2 * cost ? 0 : 1;
    }
    case GalacticEventKind::ScientificBreakthrough: return empireAtAnyWar(s, e.empireId) ? 2 : 0;
    default: return -1;
    }
}

bool createEvent(GameState& next, GalacticEventKind kind, int target, TurnReport& report) {
    if (kind != GalacticEventKind::SubspaceRift &&
        std::any_of(next.galacticEvents.begin(), next.galacticEvents.end(), [&](const GalacticEvent& e) {
            return e.kind == kind && e.empireId == target;
        })) return false;
    GalacticEvent e;
    e.kind = kind;
    e.empireId = target;
    e.startTurn = next.turn;
    e.endTurn = next.turn;
    std::vector<const Colony*> owned;
    for (const auto& c : next.colonies) if (c.ownerId == target) owned.push_back(&c);
    const auto pickColony = [&](const std::vector<const Colony*>& from) -> const Colony* {
        if (from.empty()) return nullptr;
        return from[static_cast<std::size_t>(internal::bounded(next.rngState, from.size()))];
    };
    const auto& owner = empire(next, target);
    switch (kind) {
    case GalacticEventKind::Plague: {
        std::vector<const Colony*> eligible;
        for (const auto* c : owned) if (c->population >= 15) eligible.push_back(c);
        const auto* colony = pickColony(eligible);
        if (!colony) return false;
        e.systemId = colony->systemId;
        e.magnitude = std::clamp(8 - owner.technologies[0].level, 2, 8);
        e.endTurn = next.turn + 4;
        break;
    }
    case GalacticEventKind::SupernovaWarning: {
        if (owned.size() < 2) return false;
        const Colony* capital = owned.front();
        for (const auto* c : owned) if (c->population > capital->population) capital = c;
        std::vector<const Colony*> eligible;
        for (const auto* c : owned) if (c != capital) eligible.push_back(c);
        const auto* colony = pickColony(eligible);
        if (!colony) return false;
        e.systemId = colony->systemId;
        e.magnitude = 75;
        e.endTurn = next.turn + 6;
        break;
    }
    case GalacticEventKind::SubspaceRift: {
        if (!next.rules.galaxyFeatures ||
            std::any_of(next.galacticEvents.begin(), next.galacticEvents.end(), [](const GalacticEvent& ev) {
                return ev.kind == GalacticEventKind::SubspaceRift;
            })) return false;
        e.empireId = 0;
        const int area = next.width * next.height;
        int chosen = -1;
        for (int attempt = 0; attempt < 200 && chosen < 0; ++attempt) {
            const int cell = static_cast<int>(internal::bounded(next.rngState, static_cast<std::uint64_t>(area)));
            const int x = cell % next.width, y = cell / next.width;
            if (x == 0 || y == 0 || x == next.width - 1 || y == next.height - 1) continue;
            bool clear = !stationAt(next, x, y) && featuresAt(next, x, y).empty();
            for (const auto& f : next.fleets) clear &= f.x != x || f.y != y;
            for (int dy = -1; dy <= 1 && clear; ++dy) for (int dx = -1; dx <= 1 && clear; ++dx)
                clear = !systemAtCell(next, x + dx, y + dy) && sectorPassable(next, x + dx, y + dy);
            if (clear) chosen = cell;
        }
        if (chosen < 0) return false;
        e.x = chosen % next.width; e.y = chosen / next.width;
        e.endTurn = next.turn + 10;
        e.featureId = addFeature(next, GalaxyFeatureType::SubspaceRift, e.x, e.y, e.endTurn);
        break;
    }
    case GalacticEventKind::TradeBoom:
        if (owned.empty()) return false;
        e.magnitude = 10;
        e.endTurn = next.turn + 5;
        break;
    case GalacticEventKind::DerelictVessel: {
        const Fleet* finder = nullptr;
        for (const auto& f : next.fleets) {
            if (f.ownerId != target) continue;
            const auto sys = systemAtCell(next, f.x, f.y);
            const auto colony = sys ? colonyAt(next, sys->id) : nullptr;
            if (!colony || colony->ownerId != target) { finder = &f; break; }
        }
        if (finder) { e.x = finder->x; e.y = finder->y; }
        else if (!owned.empty()) {
            const auto sys = systemById(next, owned.front()->systemId);
            e.x = sys->x; e.y = sys->y;
        } else return false;
        break;
    }
    case GalacticEventKind::LaborUnrest: {
        const Colony* colony = nullptr;
        for (const auto* c : owned) if (c->morale <= 60 && (!colony || c->morale < colony->morale)) colony = c;
        if (!colony) return false;
        e.systemId = colony->systemId;
        e.endTurn = next.turn + 4;
        break;
    }
    case GalacticEventKind::ScientificBreakthrough:
        break;
    }
    if (eventHasChoice(kind)) {
        e.choiceDeadline = next.turn + EventChoiceWindow;
        e.endTurn = std::max(e.endTurn, e.choiceDeadline + (kind == GalacticEventKind::DerelictVessel ||
            kind == GalacticEventKind::ScientificBreakthrough ? 1 : 0));
    }
    if (e.systemId) if (const auto sys = systemById(next, e.systemId)) { e.x = sys->x; e.y = sys->y; }
    e.id = next.nextEventId++;
    if (target != next.playerEmpire && eventHasChoice(kind)) e.choice = aiEventChoice(next, e);
    next.galacticEvents.push_back(e);
    if (playerConcerned(next, e)) {
        std::string text = std::string(galacticEventName(kind)) + ": " + describeEvent(next, e);
        if (eventHasChoice(kind)) {
            const auto options = eventChoices(kind);
            text += " Decide by turn " + std::to_string(e.choiceDeadline) + " (default: " +
                options[static_cast<std::size_t>(eventDefaultChoice(kind))].label + ").";
        }
        noteEvent(next, report, e, text);
    } else if (kind == GalacticEventKind::SubspaceRift && internal::known(next, next.playerEmpire, e.x, e.y)) {
        noteEvent(next, report, e, std::string(galacticEventName(kind)) + ": " + describeEvent(next, e));
    }
    return true;
}

// Returns false when the event is finished and must be removed.
bool applyEventChoice(GameState& next, GalacticEvent& e, TurnReport& report) {
    auto& owner = internal::mutableEmpire(next, e.empireId);
    const auto options = eventChoices(e.kind);
    const auto say = [&](const std::string& text) { if (playerConcerned(next, e)) noteEvent(next, report, e, text); };
    Colony* colony = e.systemId ? eventColony(next, e.systemId) : nullptr;
    switch (e.kind) {
    case GalacticEventKind::Plague:
        if (e.choice == 1 && owner.credits >= plagueCureCost(next)) {
            owner.credits -= plagueCureCost(next);
            if (e.empireId == next.playerEmpire) report.ledger.eventCosts += plagueCureCost(next);
            say("Plague at system " + std::to_string(e.systemId) + ": the emergency cure worked; the outbreak is over.");
            return false;
        }
        if (e.choice == 0 || e.choice == 1) {
            if (e.choice == 1) say("Plague: not enough credits for the cure; the colony is quarantined instead.");
            if (colony) colony->morale = std::max(0, colony->morale - 10);
            e.magnitude = std::max(1, e.magnitude / 2);
            e.endTurn = std::min(e.endTurn, next.turn + 1);
            say("Plague at system " + std::to_string(e.systemId) + ": quarantine imposed (-10 morale, losses halved, ends next turn).");
        }
        return true;
    case GalacticEventKind::SupernovaWarning: {
        int choice = e.choice;
        if (choice == 1) {
            if (owner.credits >= stellarShieldCost(next)) {
                owner.credits -= stellarShieldCost(next);
                if (e.empireId == next.playerEmpire) report.ledger.eventCosts += stellarShieldCost(next);
                e.magnitude = 30;
                say("Supernova warning: stellar shielding is under construction; losses will be limited to 30%.");
                return true;
            }
            say("Supernova warning: not enough credits for stellar shielding; evacuating instead.");
            choice = 0;
        }
        if (choice == 0 && colony) {
            int moving = colony->population / 2;
            const int moved0 = moving;
            const auto origin = systemById(next, colony->systemId);
            std::vector<Colony*> havens;
            for (auto& c : next.colonies) if (c.ownerId == colony->ownerId && c.systemId != colony->systemId) havens.push_back(&c);
            std::sort(havens.begin(), havens.end(), [&](const Colony* a, const Colony* b) {
                const auto sa = systemById(next, a->systemId), sb = systemById(next, b->systemId);
                const int da = chebyshev(sa->x, sa->y, origin->x, origin->y), db = chebyshev(sb->x, sb->y, origin->x, origin->y);
                return da == db ? a->systemId < b->systemId : da < db;
            });
            for (auto* haven : havens) {
                if (moving <= 0) break;
                const int room = std::max(0, std::min(systemCapacity(next, haven->systemId), 10000) - haven->population);
                const int take = std::min(room, moving);
                haven->population += take;
                moving -= take;
            }
            colony->population = std::max(1, colony->population - moved0);
            trimEventWorkforce(*colony);
            say("Supernova warning: " + std::to_string(moved0 - moving) + " colonists evacuated from system " +
                std::to_string(colony->systemId) + (moving > 0 ? "; " + std::to_string(moving) + " could not be housed." : "."));
        }
        return true;
    }
    case GalacticEventKind::DerelictVessel: {
        if (e.choice == 0) {
            const int field = lowestResearchField(owner);
            const auto amount = researchGrantFor(next, e.empireId, field, 40, 50);
            grantResearch(next, e.empireId, field, amount, &report);
            say("Derelict vessel salvaged: +" + std::to_string(amount) + " " + researchName(field) + " research.");
        } else if (e.choice == 1) {
            owner.credits = std::min(internal::BalanceLimit, owner.credits + derelictSalvageCredits(next));
            if (e.empireId == next.playerEmpire) report.ledger.eventIncome += derelictSalvageCredits(next);
            say("Derelict vessel stripped: +" + std::to_string(derelictSalvageCredits(next)) + " credits.");
        } else {
            owner.intelligence = std::min(internal::BalanceLimit, owner.intelligence + 60);
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) internal::reveal(next, e.x + dx, e.y + dy, e.empireId);
            say("Derelict vessel sensor logs decoded: +60 intelligence and the surrounding sectors charted.");
        }
        return false;
    }
    case GalacticEventKind::LaborUnrest: {
        int choice = e.choice;
        if (choice == 0) {
            const std::int64_t cost = 20 + (colony ? colony->population : 0);
            if (owner.credits >= cost) {
                owner.credits -= cost;
                if (e.empireId == next.playerEmpire) report.ledger.eventCosts += cost;
                if (colony) colony->morale = std::min(100, colony->morale + 15);
                say("Labor unrest at system " + std::to_string(e.systemId) + ": demands met for " + std::to_string(cost) + " credits (+15 morale).");
                return false;
            }
            say("Labor unrest: not enough credits to meet the demands; martial law declared instead.");
            choice = 1;
        }
        if (choice == 1) {
            if (colony) colony->morale = std::max(0, colony->morale - 10);
            say("Labor unrest at system " + std::to_string(e.systemId) + ": martial law ends the strike (-10 morale).");
            return false;
        }
        return true;
    }
    case GalacticEventKind::ScientificBreakthrough: {
        const int field = e.choice == 1 ? 2 : e.choice == 2 ? 5 : lowestResearchField(owner);
        const auto amount = researchGrantFor(next, e.empireId, field, 50, 60);
        grantResearch(next, e.empireId, field, amount, &report);
        say("Scientific breakthrough applied to " + std::string(researchName(field)) + ": +" + std::to_string(amount) + " research.");
        return false;
    }
    default:
        (void)options;
        return true;
    }
}

// Per-turn effects and endings. Returns false when the event is finished.
bool progressEvent(GameState& next, GalacticEvent& e, TurnReport& report) {
    const auto say = [&](const std::string& text) { if (playerConcerned(next, e)) noteEvent(next, report, e, text); };
    Colony* colony = e.systemId ? eventColony(next, e.systemId) : nullptr;
    switch (e.kind) {
    case GalacticEventKind::Plague: {
        if (!colony || colony->ownerId != e.empireId) return false;
        const int loss = std::max(1, colony->population * e.magnitude / 100);
        const int before = colony->population;
        colony->population = std::max(1, colony->population - loss);
        trimEventWorkforce(*colony);
        say("Plague at system " + std::to_string(e.systemId) + ": population " + std::to_string(before) + " -> " +
            std::to_string(colony->population) + ".");
        if (next.turn >= e.endTurn) { say("The plague at system " + std::to_string(e.systemId) + " has run its course."); return false; }
        return true;
    }
    case GalacticEventKind::SupernovaWarning: {
        if (next.turn < e.endTurn) return true;
        const bool witnessed = playerConcerned(next, e) || internal::known(next, next.playerEmpire, e.x, e.y) ||
            (colony && colony->ownerId == next.playerEmpire);
        std::string text = "The star of system " + std::to_string(e.systemId) + " went supernova.";
        if (colony) {
            const int before = colony->population;
            colony->population = std::max(1, colony->population - colony->population * e.magnitude / 100);
            colony->morale = std::max(0, colony->morale - 25);
            trimEventWorkforce(*colony);
            text += " Population " + std::to_string(before) + " -> " + std::to_string(colony->population) + ", -25 morale.";
        }
        for (auto& f : next.fleets) {
            if (f.x != e.x || f.y != e.y) continue;
            f.hull = std::max(1, f.hull - fleetMaxHull(f) / 2);
            f.shields = 0;
        }
        for (auto& st : next.stations) {
            if (st.x != e.x || st.y != e.y) continue;
            st.hull = std::max(1, st.hull - stationStats(next, st).maxHull / 2);
            st.shields = 0;
        }
        if (witnessed) noteEvent(next, report, e, text + " Ships in the sector were badly damaged.");
        return false;
    }
    case GalacticEventKind::SubspaceRift:
        return next.turn < e.endTurn;
    case GalacticEventKind::TradeBoom: {
        int colonies = 0;
        for (const auto& c : next.colonies) if (c.ownerId == e.empireId) ++colonies;
        const std::int64_t bonus = tradeBoomBonus(e, colonies);
        auto& owner = internal::mutableEmpire(next, e.empireId);
        owner.credits = std::min(internal::BalanceLimit, owner.credits + bonus);
        if (e.empireId == next.playerEmpire) report.ledger.eventIncome += bonus;
        say("Trade boom: +" + std::to_string(bonus) + " credits from booming interstellar commerce.");
        if (next.turn >= e.endTurn) { say("The trade boom has cooled."); return false; }
        return true;
    }
    case GalacticEventKind::LaborUnrest: {
        if (!colony || colony->ownerId != e.empireId) return false;
        colony->morale = std::max(0, colony->morale - 5);
        if (!colony->queue.empty()) colony->queue.front().work = std::max(0, colony->queue.front().work - 20);
        say("Strike at system " + std::to_string(e.systemId) + ": -5 morale and 20 construction work lost.");
        if (next.turn >= e.endTurn) { say("The strike at system " + std::to_string(e.systemId) + " has ended."); return false; }
        return true;
    }
    case GalacticEventKind::DerelictVessel:
    case GalacticEventKind::ScientificBreakthrough:
        return next.turn < e.endTurn;
    }
    return false;
}

// Credits one event moves at the next event phase (creditForecast()). Mirrors the
// money paths of applyEventChoice and progressEvent: `balance` is the owner's
// projected treasury at that phase and `colonies` its projected colony count.
EventCredit eventCredits(const GameState& s, const GalacticEvent& ev, int turn, std::int64_t balance, int colonies) {
    EventCredit out;
    int choice = ev.choice;
    if (ev.choiceDeadline > 0 && choice < 0 && turn > ev.choiceDeadline) choice = eventDefaultChoice(ev.kind);
    if (choice >= 0 && !ev.choiceApplied) {
        if (ev.kind == GalacticEventKind::Plague && choice == 1 && balance >= plagueCureCost(s)) out.cost = plagueCureCost(s);
        else if (ev.kind == GalacticEventKind::SupernovaWarning && choice == 1 && balance >= stellarShieldCost(s)) out.cost = stellarShieldCost(s);
        else if (ev.kind == GalacticEventKind::DerelictVessel && choice == 1) out.income = derelictSalvageCredits(s);
        else if (ev.kind == GalacticEventKind::LaborUnrest && choice == 0) {
            // The colony's population after this turn's growth decides the price; the
            // forecast uses today's population.
            const auto colony = colonyAt(s, ev.systemId);
            const std::int64_t cost = 20 + (colony ? colony->population : 0);
            if (balance >= cost) out.cost = cost;
        }
    }
    if (ev.kind == GalacticEventKind::TradeBoom) out.income += tradeBoomBonus(ev, colonies);
    return out;
}

void resolveGalacticEvents(GameState& next, TurnReport& report) {
    std::vector<int> ids;
    for (const auto& e : next.galacticEvents) ids.push_back(e.id);
    for (int id : ids) {
        const auto it = std::find_if(next.galacticEvents.begin(), next.galacticEvents.end(), [id](const GalacticEvent& e) { return e.id == id; });
        if (it == next.galacticEvents.end()) continue;
        GalacticEvent e = *it;
        bool active = true;
        if (e.choiceDeadline > 0 && e.choice < 0 && next.turn > e.choiceDeadline) {
            e.choice = eventDefaultChoice(e.kind);
            if (playerConcerned(next, e))
                noteEvent(next, report, e, std::string(galacticEventName(e.kind)) + ": no decision was made; default applied (" +
                    eventChoices(e.kind)[static_cast<std::size_t>(e.choice)].label + ").");
        }
        if (e.choice >= 0 && !e.choiceApplied) {
            e.choiceApplied = true;
            active = applyEventChoice(next, e, report);
        }
        if (active) active = progressEvent(next, e, report);
        const auto current = std::find_if(next.galacticEvents.begin(), next.galacticEvents.end(), [id](const GalacticEvent& ev) { return ev.id == id; });
        if (active) *current = e;
        else next.galacticEvents.erase(current);
    }
    if (!next.eventsEnabled || next.eventDeck.empty() || next.turn < FirstCuratedEventTurn ||
        next.turn - next.lastCuratedEventTurn < CuratedEventInterval ||
        static_cast<int>(next.galacticEvents.size()) >= ActiveEventLimit) return;
    next.lastCuratedEventTurn = next.turn;
    std::vector<int> candidates;
    for (int id : internal::activeEmpires(next))
        if (std::any_of(next.colonies.begin(), next.colonies.end(), [id](const Colony& c) { return c.ownerId == id; }))
            candidates.push_back(id);
    if (candidates.empty()) return;
    const int target = candidates[static_cast<std::size_t>(internal::bounded(next.rngState, candidates.size()))];
    for (std::size_t attempt = 0; attempt < next.eventDeck.size(); ++attempt) {
        const auto kind = static_cast<GalacticEventKind>(next.eventDeck[static_cast<std::size_t>(next.eventDeckPosition)]);
        if (++next.eventDeckPosition >= static_cast<int>(next.eventDeck.size())) {
            next.eventDeckPosition = 0;
            shuffleDeck(next.eventDeck, next.rngState);
        }
        if (createEvent(next, kind, target, report)) break;
    }
}
} // namespace botf::gameplay

namespace botf {
// Event names, choice text, default choices and description templates are data since M0:
// data/base/events.json (docs/DATA-FORMAT.md). The effects stay in code above.
const char* galacticEventName(GalacticEventKind kind) {
    const int k = static_cast<int>(kind);
    return k >= 0 && k < content::EventKinds ? content::eventDefinition(k).name.c_str() : "Unknown event";
}

std::vector<EventChoiceInfo> eventChoices(GalacticEventKind kind) {
    std::vector<EventChoiceInfo> options;
    const int k = static_cast<int>(kind);
    if (k < 0 || k >= content::EventKinds) return options;
    const auto& def = content::eventDefinition(k);
    for (const auto& choice : def.choices) {
        EventChoiceInfo info;
        info.index = static_cast<int>(options.size());
        info.label = choice.label;
        info.effect = choice.effect;
        info.isDefault = info.index == def.defaultChoice;
        options.push_back(std::move(info));
    }
    return options;
}

int eventDefaultChoice(GalacticEventKind kind) {
    const int k = static_cast<int>(kind);
    return k >= 0 && k < content::EventKinds ? content::eventDefinition(k).defaultChoice : -1;
}

std::string describeEvent(const GameState& state, const GalacticEvent& e) {
    (void)state;
    const int k = static_cast<int>(e.kind);
    if (k < 0 || k >= content::EventKinds) return {};
    const std::string where = e.systemId ? "system " + std::to_string(e.systemId) :
        e.x >= 0 ? "sector " + gameplay::sectorText(e.x, e.y) : std::string("our space");
    const std::string& text = content::eventDefinition(k).description;
    std::string out;
    out.reserve(text.size() + 32);
    for (std::size_t i = 0; i < text.size();) {
        const auto rest = std::string_view(text).substr(i);
        if (rest.starts_with("{where}")) { out += where; i += 7; }
        else if (rest.starts_with("{magnitude}")) { out += std::to_string(e.magnitude); i += 11; }
        else if (rest.starts_with("{endTurn}")) { out += std::to_string(e.endTurn); i += 9; }
        else out += text[i++];
    }
    return out;
}

std::vector<GalacticEvent> empireEvents(const GameState& state, int empireId) {
    std::vector<GalacticEvent> result;
    for (const auto& e : state.galacticEvents)
        if (e.empireId == empireId || (e.empireId == 0 && e.x >= 0 && internal::known(state, empireId, e.x, e.y))) result.push_back(e);
    return result;
}

Command makeResolveEventChoiceCommand(std::uint64_t id, int eventId, int option) {
    Command c; c.id = id; c.type = CommandType::ResolveEventChoice; c.eventId = eventId; c.eventOption = option; return c;
}
} // namespace botf
