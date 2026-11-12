// Galaxy generator scenarios (lane M2-GEN, docs/design/GALAXY-GENERATOR.md): shapes, minor-race density, difficulty,
// the schema-15 GALAXY_SETUP save section and its determinism. `GalaxyGenTests --print` lists the fingerprints the wasm
// fixture (godot/fixtures/determinism-galaxy.json) must reproduce.
#include "MinorRaces.h"
#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
int failed = 0;
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try { fn(); ++passed; std::cout << "PASS " << name << std::endl; }
    catch (const std::exception& error) { ++failed; std::cout << "FAIL " << name << ": " << error.what() << std::endl; }
}
struct Size { const char* name; int w, h, systems; };
constexpr Size Sizes[] = {{"small", 16, 13, 30}, {"medium", 22, 16, 50}, {"large", 28, 20, 80}, {"huge", 34, 24, 120}};
constexpr botf::GalaxyShape Shapes[] = {botf::GalaxyShape::Irregular, botf::GalaxyShape::Spiral, botf::GalaxyShape::Ring, botf::GalaxyShape::Elliptical};

botf::CampaignSetup setupFor(const Size& size, std::uint64_t seed, botf::GalaxyShape shape, botf::MinorDensity density = botf::MinorDensity::Some,
                             botf::Difficulty difficulty = botf::Difficulty::Normal) {
    botf::CampaignSetup setup;
    setup.seed = seed; setup.width = size.w; setup.height = size.h; setup.systems = size.systems;
    setup.galaxy = {shape, density, difficulty};
    return setup;
}
// Normalised distance of a cell from the grid centre (1 = the edge on that axis).
double radius(int x, int y, int w, int h) {
    const double u = (x - (w - 1) / 2.0) / ((w - 1) / 2.0), v = (y - (h - 1) / 2.0) / ((h - 1) / 2.0);
    return std::sqrt(u * u + v * v);
}
double innerShare(botf::GalaxyShape shape, const Size& size, double limit, int seeds) {
    int inner = 0, total = 0;
    for (int seed = 1; seed <= seeds; ++seed)
        for (const auto& [x, y] : botf::galaxyLayout(static_cast<std::uint64_t>(seed) * 7919, size.w, size.h, size.systems, shape)) {
            ++total; if (radius(x, y, size.w, size.h) < limit) ++inner;
        }
    return static_cast<double>(inner) / total;
}
double meanNearest(botf::GalaxyShape shape, const Size& size, int seeds) {
    double sum = 0; int n = 0;
    for (int seed = 1; seed <= seeds; ++seed) {
        const auto pts = botf::galaxyLayout(static_cast<std::uint64_t>(seed) * 104729, size.w, size.h, size.systems, shape);
        for (std::size_t i = 0; i < pts.size(); ++i) {
            double best = 1e9;
            for (std::size_t j = 0; j < pts.size(); ++j) if (i != j) {
                const double dx = pts[i].first - pts[j].first, dy = pts[i].second - pts[j].second;
                best = std::min(best, std::sqrt(dx * dx + dy * dy));
            }
            sum += best; ++n;
        }
    }
    return sum / n;
}
std::string withChecksum(std::string body) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--print") {
        for (auto shape : Shapes) {
            auto s = botf::newCampaign(setupFor(Sizes[1], 424242, shape, botf::MinorDensity::Some, botf::Difficulty::Hard));
            std::cout << botf::galaxyShapeName(shape) << ' ' << botf::fingerprint(s) << std::endl;
        }
        return 0;
    }
    int passed = 0;

    run("each shape: exact count, distinct cells inside the grid, deterministic, seed-sensitive", [] {
        for (auto shape : Shapes) for (const auto& size : Sizes) {
            const auto a = botf::galaxyLayout(99, size.w, size.h, size.systems, shape);
            check(static_cast<int>(a.size()) == size.systems, std::string(botf::galaxyShapeName(shape)) + " " + size.name + ": count");
            std::set<std::pair<int, int>> cells(a.begin(), a.end());
            check(static_cast<int>(cells.size()) == size.systems, "cells are distinct");
            for (const auto& [x, y] : a) check(x >= 0 && x < size.w && y >= 0 && y < size.h, "inside the grid");
            check(a == botf::galaxyLayout(99, size.w, size.h, size.systems, shape), "same seed, same layout");
            check(a != botf::galaxyLayout(100, size.w, size.h, size.systems, shape), "another seed, another layout");
        }
        // The generated campaign uses exactly that layout (preview == galaxy), Sol first.
        const auto s = botf::newCampaign(setupFor(Sizes[1], 5, botf::GalaxyShape::Spiral));
        const auto layout = botf::galaxyLayout(5, 22, 16, 50, botf::GalaxyShape::Spiral);
        for (std::size_t i = 0; i < layout.size(); ++i) check(s.systems[i].x == layout[i].first && s.systems[i].y == layout[i].second, "campaign systems match galaxyLayout");
        check(s.systems.front().name == "Sol", "Sol is the first star");
    }, passed);

    run("shapes are distinguishable: ring is hollow, spiral and elliptical are cored, irregular is clumped", [] {
        const Size& m = Sizes[2];
        const double scatter = innerShare(botf::GalaxyShape::Legacy, m, 0.4, 30);
        const double ring = innerShare(botf::GalaxyShape::Ring, m, 0.4, 30);
        const double spiral = innerShare(botf::GalaxyShape::Spiral, m, 0.5, 30), scatter5 = innerShare(botf::GalaxyShape::Legacy, m, 0.5, 30);
        const double ell = innerShare(botf::GalaxyShape::Elliptical, m, 0.5, 30);
        std::cout << "  inner share r<0.4: scatter " << scatter << " ring " << ring << "; r<0.5: scatter " << scatter5 << " spiral " << spiral << " elliptical " << ell << std::endl;
        check(ring < 0.04 && scatter > 0.10, "ring has an empty centre");
        check(spiral > scatter5 * 1.8, "spiral is dense at the core");
        check(ell > scatter5 * 1.8, "elliptical is dense at the core");
        const double nnScatter = meanNearest(botf::GalaxyShape::Legacy, m, 30), nnIrr = meanNearest(botf::GalaxyShape::Irregular, m, 30);
        std::cout << "  mean nearest neighbour: scatter " << nnScatter << " irregular " << nnIrr << std::endl;
        check(nnIrr < nnScatter * 0.93, "irregular stars are clumped");
    }, passed);

    run("minor-race density: the vanilla count for every size, capped at 29", [] {
        using botf::MinorDensity;
        const int vanilla[4][4] = {{0, 4, 5, 6}, {0, 7, 12, 17}, {0, 10, 16, 24}, {0, 14, 22, 29}}; // Small Some 5, Huge ours
        const MinorDensity ds[] = {MinorDensity::None, MinorDensity::Few, MinorDensity::Some, MinorDensity::Many};
        for (int si = 0; si < 4; ++si) for (int di = 0; di < 4; ++di) {
            const int want = vanilla[si][di];
            check(botf::minorRaceTarget(ds[di], Sizes[si].systems) == want, std::string("table ") + Sizes[si].name + " " + botf::minorDensityName(ds[di]));
            const auto s = botf::newCampaign(setupFor(Sizes[si], 31 + si, botf::GalaxyShape::Spiral, ds[di]));
            check(static_cast<int>(s.minors.size()) == want, std::string("campaign ") + Sizes[si].name + " " + botf::minorDensityName(ds[di]) + ": " + std::to_string(s.minors.size()) + " not " + std::to_string(want));
            check(s.minors.size() <= 29, "never above 29");
        }
        check(botf::minorRaceTarget(MinorDensity::Legacy, 28) == std::max(1, 28 / 5), "no density keeps the old count");
        // None leaves a game that plays: the starter minor is gone and its system is an ordinary star.
        auto s = botf::newCampaign(setupFor(Sizes[0], 8, botf::GalaxyShape::Ring, MinorDensity::None));
        check(s.minors.empty(), "none: no minors");
        for (int i = 0; i < 5; ++i) botf::advance(s);
    }, passed);

    run("difficulty: rivals scale, the player does not", [] {
        using botf::Difficulty;
        struct Row { std::int64_t rivalResearch = 0, rivalCredits = 0, playerCredits = 0, playerResearch = 0; };
        auto play = [](Difficulty d) {
            auto s = botf::newCampaign(setupFor(Sizes[1], 2026, botf::GalaxyShape::Spiral, botf::MinorDensity::Some, d));
            s.aiAggressionEnabled = false; s.eventsEnabled = false;
            const auto startRivalCredits = s.rivals.front().credits;
            for (int i = 0; i < 12; ++i) botf::advance(s);
            Row r;
            for (const auto& e : s.rivals) { r.rivalResearch += e.research; r.rivalCredits += e.credits; }
            r.playerCredits = s.credits; r.playerResearch = s.research;
            std::cout << "  " << botf::difficultyName(d) << ": rival start " << startRivalCredits << " research " << r.rivalResearch << " credits " << r.rivalCredits
                      << " | player credits " << r.playerCredits << " research " << r.playerResearch << std::endl;
            return r;
        };
        const auto simple = play(Difficulty::Simple), normal = play(Difficulty::Normal), hard = play(Difficulty::Hard), impossible = play(Difficulty::Impossible);
        check(simple.rivalResearch < normal.rivalResearch && normal.rivalResearch < hard.rivalResearch && hard.rivalResearch < impossible.rivalResearch, "rival research climbs with difficulty");
        check(simple.rivalCredits < normal.rivalCredits && normal.rivalCredits < impossible.rivalCredits, "rival credits climb with difficulty");
        check(simple.playerCredits == normal.playerCredits && normal.playerCredits == impossible.playerCredits &&
              simple.playerResearch == impossible.playerResearch, "the player's economy is the same at every level");
        // The profile is the one table: Normal changes nothing.
        const auto& dp = botf::difficultyProfile(Difficulty::Normal);
        check(dp.aiIncomePct == 100 && dp.aiIndustryPct == 100 && dp.aiResearchPct == 100 && dp.aiStartCreditsPct == 100 && dp.aiWarConfidencePct == 100 && dp.intelThreshold == 0, "normal is neutral");
        check(botf::difficultyProfile(Difficulty::Impossible).aiIncomePct > 100 && botf::difficultyProfile(Difficulty::Simple).aiIncomePct < 100, "impossible boosts, simple handicaps");
    }, passed);

    run("determinism: same seed and options, same fingerprint; every option changes the galaxy", [] {
        const auto base = setupFor(Sizes[1], 77, botf::GalaxyShape::Spiral, botf::MinorDensity::Some, botf::Difficulty::Hard);
        const auto a = botf::fingerprint(botf::newCampaign(base));
        check(a == botf::fingerprint(botf::newCampaign(base)), "repeatable");
        auto other = base; other.galaxy.shape = botf::GalaxyShape::Ring;
        check(a != botf::fingerprint(botf::newCampaign(other)), "shape changes it");
        other = base; other.galaxy.density = botf::MinorDensity::Many;
        check(a != botf::fingerprint(botf::newCampaign(other)), "density changes it");
        other = base; other.galaxy.difficulty = botf::Difficulty::Easy;
        check(a != botf::fingerprint(botf::newCampaign(other)), "difficulty changes it");
        auto s = botf::newCampaign(base), t = botf::newCampaign(base);
        for (int i = 0; i < 10; ++i) { botf::advance(s); botf::advance(t); }
        check(botf::fingerprint(s) == botf::fingerprint(t), "ten turns on, still identical");
    }, passed);

    run("save: GALAXY_SETUP round trips, schema 14 loads, a bad section is refused", [] {
        const auto s = botf::newCampaign(setupFor(Sizes[1], 12, botf::GalaxyShape::Elliptical, botf::MinorDensity::Few, botf::Difficulty::Impossible));
        const auto text = botf::serialize(s);
        check(text.starts_with((std::string("BOTF_SIM ") + std::to_string(botf::SaveVersion) + " " + botf::ContentVersion + "\n")) && text.find("\nGALAXY_SETUP 4 2 4\n") != std::string::npos, "section written");
        const auto back = botf::deserialize(text);
        check(back.galaxy == s.galaxy && botf::fingerprint(back) == botf::fingerprint(s), "round trip keeps the setup and the fingerprint");
        auto later = back; botf::advance(later);
        check(botf::deserialize(botf::serialize(later)).galaxy == s.galaxy, "still there after a turn");
        // A game with the default setup writes no section, and its schema-14 text loads.
        const auto p = botf::generateCampaign(5, 3); // no empire rules, so its text is a valid schema-14 layout
        auto old = botf::serialize(p);
        check(old.find("GALAXY_SETUP") == std::string::npos, "default setup writes no section");
        old.replace(0, old.find('\n') + 1, "BOTF_SIM 14 remaster-14\n");
        old.erase(old.rfind("CHECKSUM "));
        const auto loaded = botf::deserialize(withChecksum(old));
        check(loaded.galaxy == botf::GalaxySetup{} && botf::fingerprint(loaded) == botf::fingerprint(p), "schema-14 save loads with the default setup");
        // Refusals: out-of-range value, the default setup spelled out, the section in a schema-14 file.
        auto forge = [&](const std::string& line, const std::string& header) {
            auto t = text; t.erase(t.rfind("CHECKSUM "));
            const auto at = t.find("GALAXY_SETUP ");
            t.replace(at, t.find('\n', at) - at, line);
            t.replace(0, t.find('\n') + 1, header);
            bool rejected = false;
            try { botf::deserialize(withChecksum(t)); } catch (const std::exception&) { rejected = true; }
            return rejected;
        };
        check(forge("GALAXY_SETUP 9 2 4", (std::string("BOTF_SIM ") + std::to_string(botf::SaveVersion) + " " + botf::ContentVersion + "\n")), "range checked");
        check(forge("GALAXY_SETUP 0 0 2", (std::string("BOTF_SIM ") + std::to_string(botf::SaveVersion) + " " + botf::ContentVersion + "\n")), "default setup is never written");
        check(forge("GALAXY_SETUP 4 2 4", "BOTF_SIM 14 remaster-14\n"), "not in a schema-14 file");
    }, passed);

    run("web-bridge names parse both ways", [] {
        botf::GalaxyShape sh; botf::MinorDensity md; botf::Difficulty df;
        check(botf::parseGalaxyShape("Spiral", sh) && sh == botf::GalaxyShape::Spiral && !botf::parseGalaxyShape("scatter", sh), "shape");
        check(botf::parseMinorDensity("MANY", md) && md == botf::MinorDensity::Many && !botf::parseMinorDensity("lots", md), "density");
        check(botf::parseDifficulty("impossible", df) && df == botf::Difficulty::Impossible && botf::parseDifficulty("expert", df) && df == botf::Difficulty::Expert && !botf::parseDifficulty("godlike", df), "difficulty");
    }, passed);

    if (failed) { std::cout << "FAILED " << passed << " passed, " << failed << " failed" << std::endl; return 1; }
    std::cout << "ALL " << passed << " GALAXY GENERATOR SCENARIOS PASSED" << std::endl;
    return 0;
}
