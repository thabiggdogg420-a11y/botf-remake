// Schema-13 build-queue templates: named, ordered lists of buildings and ships an
// empire can apply to any colony, plus an optional default template applied to
// newly acquired colonies whose queues are empty. Deterministic: items that are
// illegal when applied are skipped with a reason, never reordered.
#include "EmpireRules.h"
#include "GameplayInternal.h"
#include "ShipRoster.h"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace botf::gameplay {
constexpr int TemplateLimitPerOwner = 16;
constexpr int TemplateItemLimit = 32;
constexpr int TemplateLimit = 80;

void checkTemplateItems(const Command& c) {
    require(!c.templateItems.empty() && c.templateItems.size() <= TemplateItemLimit, "Templates hold 1-32 projects");
    for (const auto& item : c.templateItems) {
        require(static_cast<int>(item.product) >= 0 && static_cast<int>(item.product) <= static_cast<int>(Building::DilithiumRefinery),
            "Unknown template project");
        require(item.work == 0 && !item.pinned && (item.referenceId == -1 || roster::validShipId(item.referenceId)),
            "Invalid template project fields");
    }
}

std::string templateItemProblem(const GameState& s, int owner, const Construction& item) {
    if (static_cast<int>(item.product) < 0 || static_cast<int>(item.product) > static_cast<int>(Building::DilithiumRefinery))
        return "Unknown project";
    if (item.referenceId >= 0) {
        if (s.ruleset != Ruleset::Reference) return "Named ship classes need a reference campaign";
        if (!roster::validShipId(item.referenceId)) return "Unknown ship class";
        if (roster::isExpandedId(item.referenceId) && !s.rules.expandedRoster) return "Expanded ship roster is off in this ruleset";
        const auto& ship = internal::sourceShip(item.referenceId);
        if (!internal::playableShip(ship) || !internal::shipUsable(ship, owner) ||
            internal::roleProduct(ship.role) != item.product) return "Ship class does not belong to this empire or role";
    } else if (isShip(item.product) && s.ruleset == Ruleset::Reference) {
        return "Choose a named reference ship class";
    }
    if (item.product == Building::DefenseBattery && !s.advancedRulesEnabled)
        return "Defense batteries need an advanced reference campaign";
    return {};
}

void validateTemplates(const GameState& s) {
    require(s.nextBuildTemplateId >= 1 && s.nextBuildTemplateId <= 10000000 && s.buildTemplates.size() <= TemplateLimit,
        "Invalid build template identity state");
    const auto empires = internal::activeEmpires(s);
    std::map<int, int> perOwner, defaults;
    int prior = 0;
    for (const auto& t : s.buildTemplates) {
        require(t.id > prior && t.id < s.nextBuildTemplateId, "Build template IDs must be unique, positive and sorted");
        prior = t.id;
        require(std::binary_search(empires.begin(), empires.end(), t.ownerId), "Build template owner is not a campaign empire");
        require(!t.name.empty() && t.name.size() <= 48 && std::none_of(t.name.begin(), t.name.end(), [](char ch) {
            const auto byte = static_cast<unsigned char>(ch); return byte < 0x20 || byte == 0x7f; }), "Invalid build template name");
        require(!t.items.empty() && t.items.size() <= TemplateItemLimit, "Build templates hold 1-32 projects");
        for (const auto& item : t.items) {
            require(item.work == 0 && !item.pinned, "Template projects carry no progress");
            const auto problem = templateItemProblem(s, t.ownerId, item);
            require(problem.empty(), "Invalid build template project: " + problem);
        }
        require(++perOwner[t.ownerId] <= TemplateLimitPerOwner, "Too many build templates for one empire");
        if (t.autoApply) require(++defaults[t.ownerId] <= 1, "Only one default build template per empire");
    }
}

// Appends a template to a colony queue, skipping projects that are illegal now.
int appendTemplate(const GameState& s, Colony& colony, const BuildTemplate& tmpl) {
    int queued = 0;
    std::vector<std::string> skipped;
    for (const auto& item : tmpl.items) {
        const std::string name = item.referenceId >= 0 ? internal::sourceShip(item.referenceId).name : definition(item.product).name;
        const auto skip = [&](const std::string& why) { skipped.push_back(name + ": " + why); };
        if (colony.queue.size() >= static_cast<std::size_t>(internal::QueueLimit)) { skip("queue full"); continue; }
        const Construction project(item.product, 0, item.referenceId);
        if (const auto problem = templateItemProblem(s, colony.ownerId, project); !problem.empty()) { skip(problem); continue; }
        if (const auto blocker = internal::constructionTechBlocker(s, project, colony.ownerId); !blocker.empty()) { skip(blocker); continue; }
        const auto queuedCount = [&](Building b) {
            return static_cast<int>(std::count_if(colony.queue.begin(), colony.queue.end(), [b](const Construction& c) { return c.product == b; }));
        };
        if (item.product == Building::Shipyard && colony.shipyards + queuedCount(Building::Shipyard) >= 1) { skip("shipyard already built or queued"); continue; }
        if (item.product == Building::DefenseBattery && colony.defenseBatteries + queuedCount(Building::DefenseBattery) >= 10) {
            skip("defense batteries at capacity"); continue;
        }
        if (item.product == Building::DilithiumRefinery) {
            if (const auto problem = empire_rules::refineryBlocker(s, colony); !problem.empty()) { skip(problem); continue; }
        }
        if (isShip(item.product) && colony.shipyards + queuedCount(Building::Shipyard) < 1) { skip("needs a shipyard"); continue; }
        colony.queue.push_back(project);
        ++queued;
    }
    std::string explanation = "Template '" + tmpl.name + "': queued " + std::to_string(queued) + " of " +
        std::to_string(tmpl.items.size()) + " projects";
    if (!skipped.empty()) {
        explanation += " (skipped ";
        for (std::size_t i = 0; i < skipped.size(); ++i) explanation += (i ? "; " : "") + skipped[i];
        explanation += ")";
    }
    colony.queueExplanation = explanation + ".";
    internal::validateColony(colony);
    return queued;
}

void applyTemplateCommand(GameState& state, const Command& c) {
    const int owner = state.playerEmpire;
    const auto owned = [&](int id) {
        return std::find_if(state.buildTemplates.begin(), state.buildTemplates.end(), [&](const BuildTemplate& t) {
            return t.id == id && t.ownerId == owner;
        });
    };
    if (c.type == CommandType::DefineBuildTemplate) {
        for (const auto& item : c.templateItems) {
            const auto problem = templateItemProblem(state, owner, item);
            require(problem.empty(), "Template project rejected: " + problem);
        }
        if (c.templateId == 0) {
            const auto mine = std::count_if(state.buildTemplates.begin(), state.buildTemplates.end(), [&](const BuildTemplate& t) {
                return t.ownerId == owner;
            });
            require(mine < TemplateLimitPerOwner && state.buildTemplates.size() < TemplateLimit &&
                state.nextBuildTemplateId < 10000000, "Build template capacity reached");
            BuildTemplate t;
            t.id = state.nextBuildTemplateId++;
            t.ownerId = owner;
            t.name = c.templateName;
            for (const auto& item : c.templateItems) t.items.emplace_back(item.product, 0, item.referenceId);
            state.buildTemplates.push_back(std::move(t));
        } else {
            const auto t = owned(c.templateId);
            require(t != state.buildTemplates.end(), "Build template not found");
            t->name = c.templateName;
            t->items.clear();
            for (const auto& item : c.templateItems) t->items.emplace_back(item.product, 0, item.referenceId);
        }
        return;
    }
    if (c.type == CommandType::DeleteBuildTemplate) {
        const auto t = owned(c.templateId);
        require(t != state.buildTemplates.end(), "Build template not found");
        state.buildTemplates.erase(t);
        return;
    }
    if (c.type == CommandType::SetDefaultBuildTemplate) {
        if (c.templateId != 0) require(owned(c.templateId) != state.buildTemplates.end(), "Build template not found");
        for (auto& t : state.buildTemplates) if (t.ownerId == owner) t.autoApply = t.id == c.templateId;
        return;
    }
    const auto t = owned(c.templateId);
    require(t != state.buildTemplates.end(), "Build template not found");
    const auto colony = std::find_if(state.colonies.begin(), state.colonies.end(), [&](const Colony& col) { return col.systemId == c.systemId; });
    require(colony != state.colonies.end() && colony->ownerId == owner, "Command targets a non-owned system");
    appendTemplate(state, *colony, *t);
}

void autoApplyTemplates(const GameState& before, GameState& next, TurnReport& report) {
    for (const auto& t : next.buildTemplates) {
        if (!t.autoApply) continue;
        for (auto& colony : next.colonies) {
            if (colony.ownerId != t.ownerId || !colony.queue.empty()) continue;
            const auto prior = colonyAt(before, colony.systemId);
            if (prior && prior->ownerId == colony.ownerId) continue;
            const int queued = appendTemplate(next, colony, t);
            if (t.ownerId != next.playerEmpire) continue;
            ChronicleEntry e;
            e.category = ChronicleCategory::Construction;
            e.text = "New colony at system " + std::to_string(colony.systemId) + " adopted the '" + t.name + "' build template (" +
                std::to_string(queued) + " projects queued).";
            e.systemId = colony.systemId;
            e.empireId = colony.ownerId;
            if (const auto sys = systemById(next, colony.systemId)) { e.x = sys->x; e.y = sys->y; }
            note(report, std::move(e));
        }
    }
}
} // namespace botf::gameplay

namespace botf {
std::vector<BuildTemplate> empireBuildTemplates(const GameState& state, int ownerId) {
    std::vector<BuildTemplate> result;
    for (const auto& t : state.buildTemplates) if (t.ownerId == ownerId) result.push_back(t);
    return result;
}
const BuildTemplate* buildTemplateById(const GameState& state, int templateId) {
    for (const auto& t : state.buildTemplates) if (t.id == templateId) return &t;
    return nullptr;
}
Command makeDefineBuildTemplateCommand(std::uint64_t id, int templateId, const std::string& name, const std::vector<Construction>& items) {
    Command c; c.id = id; c.type = CommandType::DefineBuildTemplate; c.templateId = templateId; c.templateName = name;
    for (const auto& item : items) c.templateItems.emplace_back(item.product, 0, item.referenceId);
    return c;
}
Command makeDeleteBuildTemplateCommand(std::uint64_t id, int templateId) {
    Command c; c.id = id; c.type = CommandType::DeleteBuildTemplate; c.templateId = templateId; return c;
}
Command makeApplyBuildTemplateCommand(std::uint64_t id, int templateId, int systemId) {
    Command c; c.id = id; c.type = CommandType::ApplyBuildTemplate; c.templateId = templateId; c.systemId = systemId; return c;
}
Command makeSetDefaultBuildTemplateCommand(std::uint64_t id, int templateId) {
    Command c; c.id = id; c.type = CommandType::SetDefaultBuildTemplate; c.templateId = templateId; return c;
}
} // namespace botf
