// Schema-13 turn chronicle ("Turn Briefing"): every line of TurnReport::events
// gets a categorized ChronicleEntry with structured references. Gameplay-layer
// systems record entries directly through note(); lines written by the older
// systems are classified here. Only player-visible lines ever reach events.
#include "GameplayInternal.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace botf::gameplay {
void note(TurnReport& report, ChronicleEntry entry) {
    entry.eventIndex = static_cast<int>(report.events.size());
    report.events.push_back(entry.text);
    report.chronicle.push_back(std::move(entry));
}

bool chronicleStarts(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }
bool chronicleHas(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// Leading integer after a prefix ("System 12: ..." -> 12).
int chronicleNumberAfter(const std::string& text, std::size_t start) {
    int value = 0;
    bool any = false;
    while (start < text.size() && std::isdigit(static_cast<unsigned char>(text[start]))) {
        value = value * 10 + (text[start] - '0');
        any = true;
        if (value > 100000000) break;
        ++start;
    }
    return any ? value : 0;
}

void chronicleSystemByName(const GameState& s, const std::string& text, const char* marker, ChronicleEntry& e) {
    const auto at = text.find(marker);
    if (at == std::string::npos) return;
    std::string name = text.substr(at + std::char_traits<char>::length(marker));
    // Longest system name that prefixes the remainder wins (names may contain spaces).
    const System* best = nullptr;
    for (const auto& sys : s.systems)
        if (name.rfind(sys.name, 0) == 0 && (!best || sys.name.size() > best->name.size())) best = &sys;
    if (best) { e.systemId = best->id; e.x = best->x; e.y = best->y; }
}

void chronicleEmpireByName(const std::string& text, ChronicleEntry& e) {
    for (int id = 1; id <= 5; ++id)
        if (text.find(empireName(id)) != std::string::npos) { e.otherEmpireId = id; return; }
}

ChronicleEntry classifyChronicleLine(const GameState& s, const std::string& text) {
    ChronicleEntry e;
    e.text = text;
    e.empireId = s.playerEmpire;
    using C = ChronicleCategory;
    if (chronicleStarts(text, "Battle at sector ")) {
        e.category = C::Combat;
        std::istringstream in(text.substr(17));
        char comma = 0;
        int x = -1, y = -1;
        if (in >> x >> comma >> y && comma == ',') { e.x = x; e.y = y; }
    } else if (chronicleHas(text, " captured by ") || chronicleStarts(text, "Invasion of ")) {
        e.category = C::Combat;
        chronicleSystemByName(s, text, chronicleStarts(text, "Invasion of ") ? "Invasion of " : "", e);
        chronicleEmpireByName(text, e);
    } else if (chronicleHas(text, " repaired at ")) {
        e.category = C::Combat;
        chronicleSystemByName(s, text, " repaired at ", e);
    } else if (chronicleStarts(text, "Discovered ")) {
        e.category = C::Exploration;
        chronicleSystemByName(s, text, "Discovered ", e);
    } else if (chronicleStarts(text, "Fleet ")) {
        e.category = C::Exploration;
        e.fleetId = chronicleNumberAfter(text, 6);
    } else if (chronicleStarts(text, "Colony founded at ")) {
        e.category = C::Colonization;
        chronicleSystemByName(s, text, "Colony founded at ", e);
    } else if (chronicleStarts(text, "Terraforming of ")) {
        e.category = C::Colonization;
    } else if (chronicleHas(text, " launched at ")) {
        e.category = C::Construction;
        chronicleSystemByName(s, text, " launched at ", e);
    } else if (chronicleStarts(text, "System ")) {
        e.systemId = chronicleNumberAfter(text, 7);
        if (const auto sys = systemById(s, e.systemId)) { e.x = sys->x; e.y = sys->y; }
        else e.systemId = 0;
        if (chronicleHas(text, "trade windfall") || chronicleHas(text, "civic celebration") || chronicleHas(text, "civil unrest"))
            e.category = C::Events;
        else if (chronicleHas(text, "population")) e.category = C::Colonization;
        else if (chronicleHas(text, "Sabotage")) e.category = C::Intelligence;
        else e.category = C::Construction;
    } else if (chronicleHas(text, "advanced to level")) {
        e.category = C::Research;
    } else if (chronicleStarts(text, "Intelligence") || chronicleStarts(text, "Security Alert") ||
        chronicleStarts(text, "Sabotage disrupted")) {
        e.category = C::Intelligence;
        chronicleEmpireByName(text, e);
    } else if (chronicleHas(text, "campaign victory") || chronicleHas(text, "campaign defeat") || chronicleHas(text, " victory.")) {
        e.category = C::Victory;
    } else if (chronicleStarts(text, "Diplomacy:") || chronicleStarts(text, "First contact") || chronicleHas(text, " accepted ") ||
        chronicleHas(text, " declined ") || chronicleHas(text, " at war") || chronicleHas(text, "peace") ||
        chronicleHas(text, " joined ") || chronicleHas(text, "membership") || chronicleHas(text, "friendship") ||
        chronicleHas(text, "gift") || chronicleHas(text, "offer withdrawn") || chronicleHas(text, "treaty")) {
        e.category = C::Diplomacy;
        chronicleEmpireByName(text, e);
        if (e.otherEmpireId == s.playerEmpire) e.otherEmpireId = 0;
    } else {
        e.category = C::Events;
    }
    return e;
}

void buildChronicle(const GameState& next, TurnReport& report) {
    std::vector<char> covered(report.events.size(), 0);
    for (const auto& entry : report.chronicle)
        if (entry.eventIndex >= 0 && static_cast<std::size_t>(entry.eventIndex) < covered.size() &&
            report.events[static_cast<std::size_t>(entry.eventIndex)] == entry.text)
            covered[static_cast<std::size_t>(entry.eventIndex)] = 1;
    // Drop anything that no longer lines up (defensive: events are append-only).
    std::erase_if(report.chronicle, [&](const ChronicleEntry& entry) {
        return entry.eventIndex < 0 || static_cast<std::size_t>(entry.eventIndex) >= report.events.size() ||
            report.events[static_cast<std::size_t>(entry.eventIndex)] != entry.text;
    });
    for (std::size_t i = 0; i < report.events.size(); ++i) {
        if (covered[i]) continue;
        auto entry = classifyChronicleLine(next, report.events[i]);
        entry.eventIndex = static_cast<int>(i);
        report.chronicle.push_back(std::move(entry));
    }
    std::stable_sort(report.chronicle.begin(), report.chronicle.end(), [](const ChronicleEntry& a, const ChronicleEntry& b) {
        return a.eventIndex < b.eventIndex;
    });
}
} // namespace botf::gameplay

namespace botf {
const char* chronicleCategoryName(ChronicleCategory category) {
    switch (category) {
    case ChronicleCategory::Construction: return "Construction";
    case ChronicleCategory::Research: return "Research";
    case ChronicleCategory::Exploration: return "Exploration";
    case ChronicleCategory::Colonization: return "Colonization";
    case ChronicleCategory::Combat: return "Combat";
    case ChronicleCategory::Diplomacy: return "Diplomacy";
    case ChronicleCategory::Intelligence: return "Intelligence";
    case ChronicleCategory::Events: return "Events";
    case ChronicleCategory::Stations: return "Stations";
    case ChronicleCategory::Victory: return "Victory";
    default: return "Unknown";
    }
}

std::string describeChronicle(const TurnReport& report) {
    std::ostringstream out;
    out << "Turn " << report.turn << " briefing\n";
    for (const auto& e : report.chronicle) {
        out << "  [" << chronicleCategoryName(e.category) << "] " << e.text;
        if (e.x >= 0) out << " @(" << e.x << ',' << e.y << ')';
        out << '\n';
    }
    return out.str();
}
} // namespace botf
