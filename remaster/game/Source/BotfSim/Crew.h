// BOTF Remastered: crew experience ratings (lane MOD-6; docs/design/ECONOMY-MECHANICS.md section 6). Header-only, integer maths,
// no dependencies, so the tactical engine and the economy rules share one table.
#pragma once

namespace botf::crew {
inline constexpr int StartExperience = 25, MaxExperience = 100;
// Experience points to a rating, Green 0 to Legendary 4 (the tactical engine's CrewRating).
inline constexpr int rating(int experience) { return experience < 15 ? 0 : experience < 40 ? 1 : experience < 65 ? 2 : experience < 90 ? 3 : 4; }
inline constexpr const char* ratingName(int r) {
    constexpr const char* Names[5] = {"Green", "Regular", "Veteran", "Elite", "Legendary"};
    return r >= 0 && r <= 4 ? Names[r] : "Regular";
}
// Percent added to a ship's attack by its crew in tactical rules that have no crew accuracy (rules 2 and 3).
inline constexpr int attackPercent(int experience) {
    constexpr int P[5] = {-10, 0, 5, 10, 15};
    return P[rating(experience)];
}
} // namespace botf::crew
