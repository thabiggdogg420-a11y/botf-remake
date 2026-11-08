// BOTF Remastered expanded ship roster: types (docs/SHIP-ROSTER.md).
// The data itself is data/base/roster.json (docs/DATA-FORMAT.md), loaded by Content.h; accessors are in ShipRoster.h.
#pragma once
#include <array>

namespace botf::roster {

enum class Faction : int {
    Federation, Klingon, Romulan, Cardassian, Ferengi, Dominion, Borg, Breen, Tholian, Gorn, Orion, Hirogen, Kazon,
    Vidiian, Suliban, Xindi, Species8472, Krenim, Malon, SonA, Maquis, Reman, Devore, Andorian, Vulcan, Bajoran,
    Nausicaan, Talarian
};
// Descriptive hull role; engineRole carries the ShipRecord role the campaign core understands
// (0 scout, 1 destroyer/escort, 2 cruiser, 3 strike/carrier, 4 capital, 5 colony, 7 station, 8 special, 9 transport).
enum class HullRole : int {
    Scout, Science, Escort, Frigate, Destroyer, Raider, LightCruiser, Cruiser, HeavyCruiser, StrikeCruiser, Carrier,
    Battlecruiser, Explorer, Battleship, Dreadnought, Colony, Transport, Station, Special
};
enum class Era : int { Enterprise, Original, Movie, NextGeneration, DominionWar, Nemesis, Kelvin, Future };
enum class SourceKind : int { Canon, ModDerived, Kelvin };
enum class WeaponType : int {
    None, PhaseCannon, PhaserBank, PhaserArray, PulsePhaser, Disruptor, DisruptorCannon, PlasmaCannon, PolaronBeam,
    CuttingBeam, EnergyDampener, TholianWeb, BioEnergy, TemporalWeapon, SpatialTorpedo, PhotonicTorpedo, PhotonTorpedo,
    QuantumTorpedo, PlasmaTorpedo, TricobaltDevice, Torpedo
};
enum class FiringArc : int { Forward, ForwardWide, Broadside, Aft, All };
// Tactical flight model (COMBAT-RULES.md, tactical rules 3). Size affects how easily a hull is hit by
// pulses and torpedoes; Station is an immobile platform.
enum class SizeClass : int { Small, Medium, Large, Huge, Station };
// Weapon behaviour family (tactical rules 3): continuous beams, burst pulses, travelling torpedoes.
enum class WeaponFamily : int { Beam, Pulse, Torpedo };

struct FlightProfile {
    int turnRate;      // Degrees per round, a multiple of 45 (0 for stations, 180 = can reverse heading in one round).
    int acceleration;  // Cells of speed change per round.
    int lateral;       // Cells of sideways (strafe) thrust per round.
    SizeClass size;
};

struct WeaponMount {
    WeaponType type;
    int count;
    FiringArc arc;
};

struct RosterClass {
    int id;                          // Stable id >= ExpandedIdBase; saved as Fleet::referenceShipId / Construction::referenceId.
    const char* assetKey;            // Lookup key into the local HD asset manifest (model, VFX, audio).
    Faction faction;
    int raceId;                      // referenceData::RaceCatalogue id, 36 when the power is outside that catalogue.
    int minorRaceId;                 // minors::Roster id when the faction is a managed minor race, else 0.
    const char* roleName;            // ShipRecord::name ("Light Cruiser").
    const char* className;           // ShipRecord::shipClass ("Intrepid").
    HullRole hullRole;
    int engineRole;                  // ShipRecord::role.
    Era era;
    int tier;                        // 0..10 technology tier the stats were derived from.
    std::array<int, 6> technologyRequirements; // Biotech, Computers, Construction, Energy, Propulsion, Weapons.
    int industryCost;
    int buildTurns;                  // Nominal turns at 600 industry per turn (informational; the core uses industry).
    int maintenance;
    int hull;
    int shields;
    int attack;                      // Derived from the weapon mounts (see SHIP-ROSTER.md).
    int torpedoSharePercent;         // Share of attack delivered by torpedo-type weapons (tactical beam/torpedo split).
    std::array<WeaponMount, 4> weapons;
    int weaponCount;
    int speed;                       // 1..3 (0 for stations).
    int range;                       // 0..2.
    int sensors;                     // 1..5 (informational).
    bool cloak;
    SourceKind sourceKind;
    const char* source;
    bool buildable;                  // Buildable by its major empire when the ruleset enables the expanded roster.
    const char* description;         // Original remaster text.
    FlightProfile flight;            // Tactical flight model (rules 3).
};

} // namespace botf::roster
