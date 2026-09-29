#pragma once
// Engine-independent playback schedule for one tactical rules 3 round (docs/COMBAT-RULES.md, "Event stream for client
// authors"). ABotfCombatScene turns CombatRound::events / RoundResult::events into:
//
//   * one ShipTrack per ship: eased Hermite flight from the ManeuverStart pose to the ManeuverEnd pose, shortest-arc
//     facing, banking into turns and sideways slides (never a snap: every curve starts and ends at rest),
//     cloak / decloak fades and the moment the ship disappears (destroyed or warped out);
//   * VisualJobs: beams, staggered pulse bolts, arcing torpedoes (hit, miss past the target, or point-defence
//     intercept), shield and hull impacts, rams, cloak shimmer, decloak, explosions, warp-outs;
//   * SoundJobs (weapon fire, impact per impact type, explosions, cloak) and BarkJobs (combat voice lines), timed to
//     the visual jobs, fed to UBotfAudioSubsystem whose CueScheduler still rate-limits them.
//
// Everything is plain doubles in the tactical plane (cells, degrees, seconds), deterministic, and unit-tested in
// Tools/client-logic-tests.cpp. The scene maps plane cells to world units. Rules 1 and 2 rounds have no events and
// never reach this code.

#include "BotfAssetCatalog.h"
#include "BotfAudioCues.h"
#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace botf::combatfx {

inline constexpr double kPiValue = 3.14159265358979323846;

// ---------------------------------------------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------------------------------------------

struct TimelineConfig {
    double roundSeconds = 6.0;        // The 12 ticks are spread over this many seconds.
    int moveEndTick = 4;              // Maneuvers fly from tick 0 to this tick (COMBAT-RULES "Round timeline").
    double beamTicks = 1.0;           // Beams draw from tick to tick + 1.
    double beamEmitterStaggerTicks = 0.08;
    double pulseStaggerTicks = 0.16;  // Gap between bolts of one Fire event.
    double pulseFlightTicks = 0.35;   // Muzzle to target for one bolt.
    double minTorpedoFlightTicks = 0.6;
    double torpedoStaggerTicks = 0.1; // Gap between torpedoes of one volley.
    double torpedoArcCells = 0.35;    // Peak sideways/vertical bow of a torpedo path.
    double missOvershootCells = 2.5;  // A miss flies this far past the target before fading.
    double interceptFraction = 0.78;  // Point defence kills a volley at this fraction of the flight.
    double impactTicks = 0.6;         // Shield bubble / hull sparks length.
    double explosionTicks = 2.0;
    double warpTicks = 1.0;
    double cloakTicks = 1.0;          // Cloak and decloak fade length.
    double bankPerTurnDegree = 0.3;   // Roll (deg) per degree of heading change in one maneuver.
    double bankPerSlideCell = 14.0;   // Roll (deg) per lateral cell slid.
    double maxBankDegrees = 35.0;
    int playerEmpire = 0;             // Barks only for this empire's ships (0 = none).
    // 3D flight. Presentation only: the rules stay on BOTF's 2D grid. Heights are cells above (+) or below the plane.
    double smallShipMeters = 260.0;   // Escorts and smaller fly livelier and form wings; unknown length counts as capital.
    double capitalHeightSpread = 0.45;
    double smallHeightSpread = 1.5;
    double sideHeightOffset = 0.3;    // The first side holds a little above the plane, the other below.
    double capitalClimbCells = 0.3;   // Height change per maneuver around the ship's home height.
    double smallClimbCells = 1.1;
    double maxPitchDegrees = 32.0;
    double smallBankScale = 1.6;      // Small craft roll hard into turns; capital ships lumber.
    double capitalBankScale = 0.7;
    double wingRadiusCells = 2.5;     // Small ships of one side starting this close together fly as a wing...
    int wingSize = 4;                 // ...of up to this many, in finger-four on the first (lowest id) ship.
    double wingSpacingCells = 0.55;
};

// ---------------------------------------------------------------------------------------------------------------
// Inputs and outputs
// ---------------------------------------------------------------------------------------------------------------

// What the event stream does not carry: ownership, shields at round start, cloak state and a fallback pose.
struct ShipInfo {
    int fleetId = 0;
    int ownerId = 0;
    int shields = 0;
    int maxShields = 0;
    bool startsCloaked = false;
    double x = 0.0, y = 0.0; // Pose used when the ship has no maneuver or pose-carrying event this round.
    int facing = 0;
    double lengthMeters = 0.0; // Canon length of the shown class, 0 = unknown (flight style, wings).
};

struct PlanePose {
    double x = 0.0, y = 0.0; // Tactical cells.
    double facing = 0.0;     // Degrees, 0 = +x, 90 = +y.
    double bank = 0.0;       // Roll in degrees, positive = right wing down.
    double altitude = 0.0;   // Cells above (+) or below the tactical plane (presentation only).
    double pitch = 0.0;      // Degrees, nose up positive.
    bool operator==(const PlanePose&) const = default;
};

struct FlightSegment {
    double t0 = 0.0, t1 = 0.0;
    PlanePose from, to;
    double tan0x = 0.0, tan0y = 0.0, tan1x = 0.0, tan1y = 0.0; // Hermite tangents (cells over the segment).
    double bankPeak = 0.0;
    double pitchPeak = 0.0;
    int slideCells = 0;
    int maneuver = 0;
    bool operator==(const FlightSegment&) const = default;
};

inline double wrapDegrees(double a) {
    a = std::fmod(a, 360.0);
    if (a < 0) a += 360.0;
    return a;
}
// Signed shortest turn from a to b, in (-180, 180].
inline double shortestTurn(double a, double b) {
    double d = std::fmod(b - a, 360.0);
    if (d > 180.0) d -= 360.0;
    if (d <= -180.0) d += 360.0;
    return d;
}
inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
inline double smoothstep(double u) { u = clamp01(u); return u * u * (3.0 - 2.0 * u); }

struct ShipTrack {
    int fleetId = 0;
    int ownerId = 0;
    PlanePose initial;
    std::vector<FlightSegment> segments;
    bool startsCloaked = false;
    double cloakAt = -1.0;   // Cloak fade starts (seconds), -1 = none.
    double decloakAt = -1.0; // Decloak fade starts.
    double hideAt = -1.0;    // Ship vanishes (explosion swallowed it, or it warped out), -1 = never.
    double cloakSeconds = 1.0;
    double lengthMeters = 0.0;
    bool small = false;      // Escort size or smaller (TimelineConfig::smallShipMeters).
    int wingLeader = 0;      // Fleet id of the wing leader this ship holds station on, 0 = flies its own track.
    double wingBack = 0.0, wingSide = 0.0, wingUp = 0.0; // Station in the leader's frame (cells: aft, port, up).

    // The ship's own flight (RoundTimeline::poseOf adds wing stations).
    PlanePose poseAt(double t) const {
        PlanePose p = initial;
        for (const auto& s : segments) {
            if (t <= s.t0) return p;
            if (t >= s.t1) { p = s.to; p.bank = 0.0; p.pitch = 0.0; continue; }
            const double u = (t - s.t0) / std::max(1e-9, s.t1 - s.t0);
            const double e = smoothstep(u);
            const double e2 = e * e, e3 = e2 * e;
            const double h00 = 2 * e3 - 3 * e2 + 1, h10 = e3 - 2 * e2 + e, h01 = -2 * e3 + 3 * e2, h11 = e3 - e2;
            p.x = h00 * s.from.x + h10 * s.tan0x + h01 * s.to.x + h11 * s.tan1x;
            p.y = h00 * s.from.y + h10 * s.tan0y + h01 * s.to.y + h11 * s.tan1y;
            p.facing = wrapDegrees(s.from.facing + shortestTurn(s.from.facing, s.to.facing) * e);
            const double sn = std::sin(kPiValue * clamp01(u));
            p.bank = s.bankPeak * sn * sn;
            p.altitude = s.from.altitude + (s.to.altitude - s.from.altitude) * e;
            p.pitch = s.pitchPeak * sn * sn;
            return p;
        }
        return p;
    }
    // 1 = fully visible, 0 = cloaked or gone; in between = shimmering.
    double visibilityAt(double t) const {
        if (hideAt >= 0.0 && t >= hideAt) return 0.0;
        const double fade = std::max(1e-6, cloakSeconds);
        double v = startsCloaked ? 0.0 : 1.0;
        // Apply cloak/decloak in time order (a cloak and decloak at the same time: cloak first).
        struct Change { double at; bool cloak; };
        Change changes[2] = {{cloakAt, true}, {decloakAt, false}};
        if (decloakAt >= 0.0 && (cloakAt < 0.0 || decloakAt < cloakAt)) std::swap(changes[0], changes[1]);
        for (const auto& c : changes) {
            if (c.at < 0.0 || t < c.at) continue;
            const double k = smoothstep((t - c.at) / fade);
            v = c.cloak ? v * (1.0 - k) : v + (1.0 - v) * k;
        }
        return clamp01(v);
    }
};

enum class JobKind : int {
    Beam = 0, PulseBolt, Torpedo, ShieldImpact, HullImpact, PointDefence, Ram, CloakShimmer, Decloak, Explosion,
    WarpOut, Disengage, Truce
};
inline const char* jobKindName(JobKind k) {
    switch (k) {
    case JobKind::Beam: return "beam";
    case JobKind::PulseBolt: return "pulse";
    case JobKind::Torpedo: return "torpedo";
    case JobKind::ShieldImpact: return "shield";
    case JobKind::HullImpact: return "hull";
    case JobKind::PointDefence: return "intercept";
    case JobKind::Ram: return "ram";
    case JobKind::CloakShimmer: return "cloak";
    case JobKind::Decloak: return "decloak";
    case JobKind::Explosion: return "explosion";
    case JobKind::WarpOut: return "warp";
    case JobKind::Disengage: return "disengage";
    case JobKind::Truce: return "truce";
    }
    return "unknown";
}

enum class ShotOutcome : int { Hit = 0, Miss = 1, Intercepted = 2 };

struct Rgb {
    float r = 1.f, g = 1.f, b = 1.f;
    bool operator==(const Rgb&) const = default;
};

struct VisualJob {
    JobKind kind = JobKind::Beam;
    double start = 0.0, duration = 0.0;
    int fleetId = 0;       // Shooter / actor.
    int targetFleetId = 0; // Target of a shot; for impacts the ship that was struck.
    int sourceFleetId = 0; // Impacts: where the shot came from (the bubble flashes on that side).
    int mountIndex = -1;
    int weaponType = 0;
    int weaponFamily = 0;  // 0 beam, 1 pulse, 2 torpedo.
    int index = 0, count = 1; // Emitter / bolt / torpedo number within the event.
    ShotOutcome outcome = ShotOutcome::Hit;
    Rgb color;
    double arcCells = 0.0;       // Torpedo bow (signed: alternates per torpedo).
    double overshootCells = 0.0; // Miss: distance flown past the target.
    double endFraction = 1.0;    // Torpedo: fraction of the flight actually flown (intercepts stop early).
    int shieldDamage = 0, hullDamage = 0;
    bool destroysTarget = false;
    int eventIndex = -1;
    std::string vfxKey;          // Optional Niagara system, "vfx.<WeaponType>" or "vfx.impact.shield" etc.
    double end() const { return start + duration; }
    bool operator==(const VisualJob&) const = default;
};

struct SoundJob {
    double time = 0.0;
    audio::GameCue cue = audio::GameCue::BeamFire;
    std::vector<std::string> keys; // Tried in order (faction variant, type, generic).
    int priority = 1;
    int eventIndex = -1;
    bool operator==(const SoundJob&) const = default;
};

struct BarkJob {
    double time = 0.0;
    std::string detail; // "shields.50", "torpedo", "kill"; voice key vo.combat.<detail>.<fac> (see audio::cueFor).
    int priority = 1;
    int fleetId = 0;
    bool operator==(const BarkJob&) const = default;
};

struct RoundTimeline {
    double tickSeconds = 0.5;
    double roundSeconds = 6.0;
    double endSeconds = 6.0; // Last job end (explosions may run past the round).
    std::vector<ShipTrack> ships;
    std::vector<VisualJob> jobs;   // Sorted by start, then event index, then index.
    std::vector<SoundJob> sounds;  // Sorted by time.
    std::vector<BarkJob> barks;    // Sorted by time.
    std::vector<int> consumed;     // Per input event: jobs/tracks produced from it (0 = ignored; tests require > 0).

    const ShipTrack* track(int fleetId) const {
        for (const auto& s : ships) if (s.fleetId == fleetId) return &s;
        return nullptr;
    }
    // Where ships[index] is at time t: its own flight, or its station in its leader's wing. When the leader is
    // gone (destroyed, warped out) the wingman peels off back to its own track over two ticks.
    PlanePose poseOf(std::size_t index, double t) const {
        const ShipTrack& tr = ships[index];
        const PlanePose own = tr.poseAt(t);
        const ShipTrack* lead = tr.wingLeader != 0 ? track(tr.wingLeader) : nullptr;
        if (!lead) return own;
        double w = 1.0;
        if (lead->hideAt >= 0.0 && t > lead->hideAt) w = 1.0 - smoothstep((t - lead->hideAt) / std::max(1e-6, 2.0 * tickSeconds));
        if (w <= 0.0) return own;
        const PlanePose lp = lead->poseAt(t);
        const double f = lp.facing * kPiValue / 180.0;
        PlanePose p;
        p.x = own.x + (lp.x - std::cos(f) * tr.wingBack - std::sin(f) * tr.wingSide - own.x) * w;
        p.y = own.y + (lp.y - std::sin(f) * tr.wingBack + std::cos(f) * tr.wingSide - own.y) * w;
        p.altitude = own.altitude + (lp.altitude + tr.wingUp - own.altitude) * w;
        p.facing = wrapDegrees(own.facing + shortestTurn(own.facing, lp.facing) * w);
        p.bank = own.bank + (lp.bank - own.bank) * w;
        p.pitch = own.pitch + (lp.pitch - own.pitch) * w;
        return p;
    }
    bool allConsumed() const {
        for (int c : consumed) if (c <= 0) return false;
        return true;
    }
};

// ---------------------------------------------------------------------------------------------------------------
// Colours, keys, hardpoints
// ---------------------------------------------------------------------------------------------------------------

// Beam / bolt / torpedo glow per weapon type, then per owner faction for generic types.
inline Rgb weaponColor(int weaponType, int ownerFaction) {
    using W = roster::WeaponType;
    switch (static_cast<W>(weaponType)) {
    case W::PhaserBank: case W::PhaserArray: case W::PhaseCannon: case W::PulsePhaser:
        return ownerFaction == 5 ? Rgb{1.0f, 0.75f, 0.15f} : Rgb{1.0f, 0.45f, 0.05f};
    case W::Disruptor: case W::DisruptorCannon:
        return ownerFaction == 4 ? Rgb{1.0f, 0.8f, 0.15f} : Rgb{0.15f, 1.0f, 0.25f};
    case W::PlasmaCannon: return {0.2f, 1.0f, 0.45f};
    case W::PolaronBeam: return {0.45f, 0.35f, 1.0f};
    case W::CuttingBeam: return {0.3f, 1.0f, 0.3f};
    case W::EnergyDampener: return {0.3f, 0.9f, 1.0f};
    case W::TholianWeb: return {1.0f, 0.35f, 0.1f};
    case W::BioEnergy: return {0.8f, 1.0f, 0.2f};
    case W::TemporalWeapon: return {0.8f, 0.3f, 1.0f};
    case W::PhotonTorpedo: case W::PhotonicTorpedo: case W::Torpedo: return {1.0f, 0.2f, 0.08f};
    case W::QuantumTorpedo: return {0.4f, 0.65f, 1.0f};
    case W::PlasmaTorpedo: return {0.25f, 1.0f, 0.4f};
    case W::SpatialTorpedo: return {1.0f, 0.55f, 0.2f};
    case W::TricobaltDevice: return {1.0f, 0.95f, 0.85f};
    case W::None: break;
    }
    switch (ownerFaction) {
    case 2: return {0.1f, 1.0f, 0.2f};
    case 3: return {0.05f, 0.95f, 0.35f};
    case 4: return {1.0f, 0.85f, 0.1f};
    case 5: return {0.1f, 0.75f, 1.0f};
    default: return {1.0f, 0.5f, 0.2f};
    }
}

inline std::string vfxWeaponKey(int weaponType, int family) {
    if (weaponType > 0 && weaponType <= static_cast<int>(roster::WeaponType::Torpedo))
        return std::string("vfx.") + roster::weaponTypeName(static_cast<roster::WeaponType>(weaponType));
    return family == 2 ? "vfx.generic.torpedo" : family == 1 ? "vfx.generic.pulse" : "vfx.generic.beam";
}
// Keys to try for a job's optional Niagara system (specific first, then a family/generic key).
inline std::vector<std::string> vfxKeysFor(const VisualJob& job) {
    std::vector<std::string> keys;
    if (!job.vfxKey.empty()) keys.push_back(job.vfxKey);
    if (job.kind == JobKind::Beam || job.kind == JobKind::PulseBolt || job.kind == JobKind::Torpedo) {
        const std::string generic = job.weaponFamily == 2 ? "vfx.generic.torpedo" : job.weaponFamily == 1 ? "vfx.generic.pulse" : "vfx.generic.beam";
        if (keys.empty() || keys.back() != generic) keys.push_back(generic);
    }
    return keys;
}

// Sound keys for a shot: weapon.<Type>.<fac>, weapon.<Type>, weapon.generic.<beam|torpedo> (as BotfAssetCatalog).
inline std::vector<std::string> weaponSoundKeys(int weaponType, int family, int ownerFaction) {
    std::vector<std::string> keys;
    if (weaponType > 0 && weaponType <= static_cast<int>(roster::WeaponType::Torpedo))
        keys = assets::weaponAssetKeys(static_cast<roster::WeaponType>(weaponType), assets::ownerFactionPrefix(ownerFaction));
    if (keys.empty()) keys.push_back(family == 2 ? "weapon.generic.torpedo" : "weapon.generic.beam");
    return keys;
}

inline std::string upperAscii(std::string s) {
    for (auto& c : s) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}
// Imported meshes carry sockets WPN_<Type>_<Loc>_<NN> (manifest socketConvention): WPN_Beam_01, WPN_Torp_F_02,
// WPN_Plasma_Torp_01, WPN_Pulse_03 ... Torpedo mounts take *_TORP_* sockets; beams prefer WPN_BEAM*, pulses
// WPN_PULSE*, and both fall back to any other non-torpedo weapon socket.
inline bool isTorpedoSocket(const std::string& name) {
    const std::string u = upperAscii(name);
    return u.rfind("WPN_", 0) == 0 && u.find("TORP") != std::string::npos;
}
inline bool isWeaponSocket(const std::string& name) { return upperAscii(name).rfind("WPN_", 0) == 0; }

// Index into names of the socket for (family, mount, emitter), or -1 when the mesh has none (use fallbackHardpoint).
inline int pickWeaponSocket(const std::vector<std::string>& names, int family, int mountIndex, int emitterIndex) {
    std::vector<int> preferred, other;
    const char* want = family == 1 ? "WPN_PULSE" : "WPN_BEAM";
    for (int i = 0; i < static_cast<int>(names.size()); ++i) {
        const auto& n = names[static_cast<std::size_t>(i)];
        if (!isWeaponSocket(n)) continue;
        if (family == 2) { if (isTorpedoSocket(n)) preferred.push_back(i); continue; }
        if (isTorpedoSocket(n)) continue;
        (upperAscii(n).rfind(want, 0) == 0 ? preferred : other).push_back(i);
    }
    auto& pool = preferred.empty() ? other : preferred;
    if (pool.empty()) return -1;
    std::sort(pool.begin(), pool.end(), [&](int a, int b) { return upperAscii(names[static_cast<std::size_t>(a)]) < upperAscii(names[static_cast<std::size_t>(b)]); });
    const int slot = std::max(0, mountIndex) + std::max(0, emitterIndex);
    return pool[static_cast<std::size_t>(slot % static_cast<int>(pool.size()))];
}

// Hardpoint as a fraction of the mesh's local bounds half-extents (x forward, y right, z up) when no socket exists:
// torpedo tubes at the bow, beam emitters spread port/starboard along the dorsal hull.
struct Hardpoint { double x = 0.0, y = 0.0, z = 0.0; };
inline Hardpoint fallbackHardpoint(int family, int mountIndex, int emitterIndex) {
    const int m = std::max(0, mountIndex), e = std::max(0, emitterIndex);
    const double side = ((m + e) % 2 == 0) ? 1.0 : -1.0;
    if (family == 2) return {0.9, side * 0.12, -0.08};
    const double along = 0.45 - 0.25 * static_cast<double>((m / 2 + e) % 4);
    return {along, side * 0.45, family == 1 ? 0.05 : 0.2};
}

// Position along a torpedo path (plane cells plus height). u in [0,1] of the flight; the arc bows sideways in the
// plane and up out of it. A miss continues past the target by overshoot cells.
struct PathPoint { double x = 0.0, y = 0.0, lift = 0.0; };
inline PathPoint torpedoPoint(double sx, double sy, double tx, double ty, double arcCells, double overshootCells, double u) {
    double dx = tx - sx, dy = ty - sy;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len > 1e-9 && overshootCells > 0.0) {
        const double k = (len + overshootCells) / len;
        tx = sx + dx * k; ty = sy + dy * k; dx = tx - sx; dy = ty - sy;
    }
    const double nx = len > 1e-9 ? -dy / std::sqrt(dx * dx + dy * dy) : 0.0;
    const double ny = len > 1e-9 ? dx / std::sqrt(dx * dx + dy * dy) : 0.0;
    const double bow = 4.0 * u * (1.0 - u);
    return {sx + dx * u + nx * arcCells * bow, sy + dy * u + ny * arcCells * bow, std::fabs(arcCells) * bow};
}

// ---------------------------------------------------------------------------------------------------------------
// Builder
// ---------------------------------------------------------------------------------------------------------------

// Deterministic value in [0, 1) for a ship and a salt (heights must not flicker between replays of a round).
inline double unitHash(int a, int b) {
    std::uint32_t h = static_cast<std::uint32_t>(a) * 0x9E3779B1u ^ (static_cast<std::uint32_t>(b) + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return static_cast<double>(h & 0xFFFFFFu) / static_cast<double>(0x1000000u);
}
inline bool isSmallShip(double lengthMeters, const TimelineConfig& cfg) {
    return lengthMeters > 0.0 && lengthMeters <= cfg.smallShipMeters;
}
// The height a ship holds between maneuvers: capital ships near the plane, small craft spread well above and below,
// each side offset so the two fleets read as layers.
inline double homeHeight(int fleetId, double lengthMeters, bool firstSide, const TimelineConfig& cfg = {}) {
    const double spread = isSmallShip(lengthMeters, cfg) ? cfg.smallHeightSpread : cfg.capitalHeightSpread;
    return (unitHash(fleetId, 1) * 2.0 - 1.0) * spread + (firstSide ? cfg.sideHeightOffset : -cfg.sideHeightOffset);
}

// Presentation-only spacing. Ships are drawn at true scale, so a capital ship spans several grid cells and hulls that
// share or neighbour a cell would pass through each other. spacingOffsets() returns, per body, the smallest shift
// (cells: plane x, plane y, altitude) that keeps every pair of active hulls at least (ra + rb) apart, preferring
// height over sideways moves (verticalBias) so the grid positions the rules use stay readable from above.
// Deterministic: pairs resolve in index order, coincident hulls split by index. The scene smooths the result.
struct SpacingBody { double x = 0.0, y = 0.0, alt = 0.0, radius = 0.0; bool active = true; };
struct SpacingOffset { double x = 0.0, y = 0.0, alt = 0.0; };
inline std::vector<SpacingOffset> spacingOffsets(const std::vector<SpacingBody>& bodies, int iterations = 10,
                                                 double verticalBias = 2.5, double maxShift = 3.0) {
    const std::size_t n = bodies.size();
    std::vector<SpacingOffset> off(n);
    for (int pass = 0; pass < iterations; ++pass) {
        bool moved = false;
        for (std::size_t i = 0; i < n; ++i) {
            if (!bodies[i].active || bodies[i].radius <= 0.0) continue;
            for (std::size_t j = i + 1; j < n; ++j) {
                if (!bodies[j].active || bodies[j].radius <= 0.0) continue;
                const double dx = (bodies[j].x + off[j].x) - (bodies[i].x + off[i].x);
                const double dy = (bodies[j].y + off[j].y) - (bodies[i].y + off[i].y);
                const double da = (bodies[j].alt + off[j].alt) - (bodies[i].alt + off[i].alt);
                const double dist = std::sqrt(dx * dx + dy * dy + da * da);
                const double need = bodies[i].radius + bodies[j].radius;
                if (dist >= need) continue;
                // Push direction: the current separation with height weighted up and tilted at least ~27 degrees out
                // of the plane (level neighbours stack rather than shove sideways); straight up/down when coincident.
                const double flat = std::sqrt(dx * dx + dy * dy);
                const double up = da > 1e-9 ? 1.0 : da < -1e-9 ? -1.0 : ((i + j) % 2 == 0 ? 1.0 : -1.0);
                double ux = dx, uy = dy, ua = up * std::max(std::abs(da) * verticalBias, 0.5 * flat);
                double len = std::sqrt(ux * ux + uy * uy + ua * ua);
                if (len < 1e-9) { ux = 0.0; uy = 0.0; ua = up; len = 1.0; }
                ux /= len; uy /= len; ua /= len;
                // The push along u must close the gap along the true separation; split by the other hull's size so a
                // cruiser moves less than the escort beside it.
                const double along = std::max(0.3, (ux * dx + uy * dy + ua * da) / std::max(dist, 1e-9));
                const double gap = (need - dist) / (dist < 1e-9 ? 1.0 : along);
                const double wi = bodies[j].radius / need, wj = bodies[i].radius / need;
                off[i].x -= ux * gap * wi; off[i].y -= uy * gap * wi; off[i].alt -= ua * gap * wi;
                off[j].x += ux * gap * wj; off[j].y += uy * gap * wj; off[j].alt += ua * gap * wj;
                moved = true;
            }
        }
        if (!moved) break;
    }
    for (auto& o : off) {
        const double len = std::sqrt(o.x * o.x + o.y * o.y + o.alt * o.alt);
        if (len > maxShift) { const double k = maxShift / len; o.x *= k; o.y *= k; o.alt *= k; }
    }
    return off;
}

// Station-keeping between maneuvers (presentation only). A ship holding position still lives: it bobs, rolls and
// nods a little, small craft more than capital ships, each on its own phase. The motion fades in and out over
// fadeSeconds at every maneuver edge and at the round's start and end, so poses at those instants are untouched and
// rounds join seamlessly. Added by the scene on top of RoundTimeline::poseOf.
struct IdleOffset { double altitude = 0.0, bank = 0.0, pitch = 0.0; };
inline IdleOffset stationKeeping(const ShipTrack& track, double t, double roundSeconds, double fadeSeconds = 0.6) {
    if (t <= 0.0 || t >= roundSeconds) return {};
    double edge = std::min(t, roundSeconds - t);
    for (const auto& s : track.segments) {
        if (t > s.t0 && t < s.t1) return {}; // flying a maneuver
        edge = std::min(edge, std::min(std::abs(t - s.t0), std::abs(t - s.t1)));
    }
    const double env = smoothstep(edge / std::max(1e-6, fadeSeconds));
    const double phase = unitHash(track.fleetId, 7) * 2.0 * kPiValue;
    const double amp = track.small ? 1.0 : 0.4;
    IdleOffset o;
    o.altitude = env * amp * 0.12 * std::sin(2.0 * kPiValue * t / 3.4 + phase);
    o.bank = env * amp * 6.0 * std::sin(2.0 * kPiValue * t / 4.1 + phase * 1.7);
    o.pitch = env * amp * 2.5 * std::sin(2.0 * kPiValue * t / 2.9 + phase * 0.6);
    return o;
}

inline bool eventHasPose(CombatEventKind k) {
    return k == CombatEventKind::ManeuverStart || k == CombatEventKind::ManeuverEnd || k == CombatEventKind::Fire ||
        k == CombatEventKind::Ram || k == CombatEventKind::Retreated || k == CombatEventKind::Disengage ||
        k == CombatEventKind::Cloak || k == CombatEventKind::Decloak || k == CombatEventKind::Destroyed;
}

inline RoundTimeline buildRoundTimeline(const std::vector<CombatEvent>& events, const std::vector<ShipInfo>& info,
                                        const TimelineConfig& cfg = {}) {
    RoundTimeline tl;
    tl.roundSeconds = std::max(0.5, cfg.roundSeconds);
    tl.tickSeconds = tl.roundSeconds / CombatTicksPerRound;
    const double tk = tl.tickSeconds;
    const auto at = [tk](double tick) { return tick * tk; };
    tl.consumed.assign(events.size(), 0);

    const auto infoFor = [&info](int fleetId) -> const ShipInfo* {
        for (const auto& s : info) if (s.fleetId == fleetId) return &s;
        return nullptr;
    };
    const auto ownerOf = [&](int fleetId) { const auto* s = infoFor(fleetId); return s ? s->ownerId : 0; };
    const auto trackFor = [&tl, &cfg](int fleetId) -> ShipTrack& {
        for (auto& s : tl.ships) if (s.fleetId == fleetId) return s;
        ShipTrack t;
        t.fleetId = fleetId;
        t.cloakSeconds = cfg.cloakTicks;
        tl.ships.push_back(t);
        return tl.ships.back();
    };

    // Tracks: every known ship, then any actor only the events mention. Initial pose = first pose-carrying event.
    for (const auto& s : info) {
        auto& t = trackFor(s.fleetId);
        t.ownerId = s.ownerId;
        t.startsCloaked = s.startsCloaked;
        t.initial = {s.x, s.y, wrapDegrees(s.facing), 0.0};
        t.cloakSeconds = cfg.cloakTicks * tk;
    }
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        if (e.fleetId == 0 || !eventHasPose(e.kind)) continue;
        bool known = false;
        for (const auto& s : tl.ships) known = known || s.fleetId == e.fleetId;
        auto& t = trackFor(e.fleetId);
        if (!known) { t.ownerId = ownerOf(e.fleetId); t.cloakSeconds = cfg.cloakTicks * tk; }
        bool seen = false;
        for (std::size_t j = 0; j < i; ++j) seen = seen || (events[j].fleetId == e.fleetId && eventHasPose(events[j].kind));
        if (!seen) t.initial = {static_cast<double>(e.x), static_cast<double>(e.y), wrapDegrees(e.facing), 0.0};
    }

    // Flight segments: ManeuverStart .. matching ManeuverEnd.
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        if (e.kind != CombatEventKind::ManeuverStart && e.kind != CombatEventKind::ManeuverEnd) continue;
        auto& t = trackFor(e.fleetId);
        if (e.kind == CombatEventKind::ManeuverStart) {
            t.initial = {static_cast<double>(e.x), static_cast<double>(e.y), wrapDegrees(e.facing), 0.0};
            ++tl.consumed[i];
            continue;
        }
        // ManeuverEnd: find its start (the latest ManeuverStart of this ship before it), else the current pose.
        PlanePose from = t.segments.empty() ? t.initial : t.segments.back().to;
        int startTick = 0, maneuver = e.maneuver;
        for (std::size_t j = i; j-- > 0;)
            if (events[j].fleetId == e.fleetId && events[j].kind == CombatEventKind::ManeuverStart) {
                from = {static_cast<double>(events[j].x), static_cast<double>(events[j].y), wrapDegrees(events[j].facing), 0.0};
                startTick = events[j].tick;
                break;
            }
        FlightSegment s;
        s.from = from;
        s.to = {static_cast<double>(e.x), static_cast<double>(e.y), wrapDegrees(e.facing), 0.0};
        s.t0 = at(startTick);
        s.t1 = at(std::max(e.tick, startTick + 1));
        if (!t.segments.empty()) s.t0 = std::max(s.t0, t.segments.back().t1);
        if (s.t1 <= s.t0) s.t1 = s.t0 + tk;
        s.slideCells = e.value;
        s.maneuver = maneuver;
        const double dx = s.to.x - s.from.x, dy = s.to.y - s.from.y;
        const double len = std::sqrt(dx * dx + dy * dy);
        const double fa = s.from.facing * kPiValue / 180.0, fb = s.to.facing * kPiValue / 180.0;
        const double along0 = dx * std::cos(fa) + dy * std::sin(fa);
        const double along1 = dx * std::cos(fb) + dy * std::sin(fb);
        if (len < 1e-9) {
            s.tan0x = s.tan0y = s.tan1x = s.tan1y = 0.0;
        } else if (e.value > 0 || along0 <= 0.25 * len) {
            // Sideways slide or reverse thrust: keep the bow, translate along the displacement.
            s.tan0x = s.tan1x = dx; s.tan0y = s.tan1y = dy;
        } else {
            s.tan0x = std::cos(fa) * len; s.tan0y = std::sin(fa) * len;
            if (along1 > 0.0) { s.tan1x = std::cos(fb) * len; s.tan1y = std::sin(fb) * len; }
            else { s.tan1x = dx; s.tan1y = dy; }
        }
        double bank = -shortestTurn(s.from.facing, s.to.facing) * cfg.bankPerTurnDegree;
        if (e.value > 0 && len > 1e-9) {
            // Cross of forward and displacement: > 0 means the slide went to port (left) -> roll left (negative).
            const double cross = std::cos(fa) * dy - std::sin(fa) * dx;
            bank += (cross > 0 ? -1.0 : 1.0) * cfg.bankPerSlideCell * e.value;
        }
        // Small craft roll harder (and may exceed the capital-ship limit); capital ships lumber.
        double maxBank = cfg.maxBankDegrees;
        if (const auto* si = infoFor(e.fleetId); si && si->lengthMeters > 0.0) {
            const bool small = isSmallShip(si->lengthMeters, cfg);
            bank *= small ? cfg.smallBankScale : cfg.capitalBankScale;
            if (small) maxBank *= cfg.smallBankScale;
        }
        s.bankPeak = std::clamp(bank, -maxBank, maxBank);
        t.segments.push_back(s);
        ++tl.consumed[i];
    }

    // Heights (3D flight): each ship holds a home height and climbs or dives around it with every maneuver, pitching
    // its nose with the climb. A ship sitting still (no horizontal run) rises or sinks level on thrusters.
    const int firstOwner = info.empty() ? 0 : info.front().ownerId;
    for (auto& t : tl.ships) {
        const auto* si = infoFor(t.fleetId);
        t.lengthMeters = si ? si->lengthMeters : 0.0;
        t.small = isSmallShip(t.lengthMeters, cfg);
        const bool firstSide = t.ownerId == firstOwner;
        const double home = homeHeight(t.fleetId, t.lengthMeters, firstSide, cfg);
        const double climb = t.small ? cfg.smallClimbCells : cfg.capitalClimbCells;
        double h = home;
        t.initial.altitude = h;
        for (std::size_t k = 0; k < t.segments.size(); ++k) {
            auto& s = t.segments[k];
            s.from.altitude = h;
            h = home + (unitHash(t.fleetId, 101 + static_cast<int>(k)) * 2.0 - 1.0) * climb;
            s.to.altitude = h;
            const double run = std::hypot(s.to.x - s.from.x, s.to.y - s.from.y);
            s.pitchPeak = run > 0.3
                ? std::clamp(std::atan2(s.to.altitude - s.from.altitude, run) * 180.0 / kPiValue * 1.5, -cfg.maxPitchDegrees, cfg.maxPitchDegrees)
                : 0.0;
        }
    }

    // Wings: escort-size ships of one side that start close together fly finger-four on the lowest-id ship among them
    // (wingman aft-port, element lead aft-starboard, its wingman further aft-starboard).
    {
        static constexpr double kStations[3][3] = {{1.0, 1.0, 0.25}, {1.0, -1.0, -0.25}, {2.0, -2.0, 0.0}}; // aft, port, up
        std::vector<ShipTrack*> pool;
        for (auto& t : tl.ships) if (t.small) pool.push_back(&t);
        std::sort(pool.begin(), pool.end(), [](const ShipTrack* a, const ShipTrack* b) { return a->fleetId < b->fleetId; });
        std::vector<bool> used(pool.size(), false);
        for (std::size_t i = 0; i < pool.size(); ++i) {
            if (used[i]) continue;
            used[i] = true;
            int members = 1;
            for (std::size_t j = i + 1; j < pool.size() && members < std::max(1, cfg.wingSize); ++j) {
                if (used[j] || pool[j]->ownerId != pool[i]->ownerId) continue;
                if (std::hypot(pool[j]->initial.x - pool[i]->initial.x, pool[j]->initial.y - pool[i]->initial.y) > cfg.wingRadiusCells) continue;
                used[j] = true;
                const auto& st = kStations[members - 1];
                pool[j]->wingLeader = pool[i]->fleetId;
                pool[j]->wingBack = st[0] * cfg.wingSpacingCells;
                pool[j]->wingSide = st[1] * cfg.wingSpacingCells;
                pool[j]->wingUp = st[2] * cfg.wingSpacingCells;
                ++members;
            }
        }
    }

    // Weapon, impact and state jobs.
    struct Impact { double time; int target; int source; int shieldDamage; bool destroyed; int shooterOwner; };
    std::vector<Impact> impacts;
    const auto addJob = [&tl](VisualJob j, std::size_t ev) {
        j.eventIndex = static_cast<int>(ev);
        tl.jobs.push_back(std::move(j));
        ++tl.consumed[ev];
    };
    const auto addSound = [&tl](double time, audio::GameCue cue, std::vector<std::string> keys, int priority, std::size_t ev) {
        tl.sounds.push_back({time, cue, std::move(keys), priority, static_cast<int>(ev)});
    };
    const auto cueKeys = [](audio::GameCue cue, int owner) { return audio::cueFor(cue, "", assets::ownerFactionPrefix(owner)); };
    const auto addImpacts = [&](const CombatEvent& e, double time, std::size_t ev, int family) {
        const int shooterOwner = ownerOf(e.fleetId);
        if (e.shieldDamage > 0 || (e.hit && e.hullDamage <= 0)) {
            VisualJob j;
            j.kind = JobKind::ShieldImpact; j.start = time; j.duration = cfg.impactTicks * tk;
            j.fleetId = e.targetFleetId; j.targetFleetId = e.targetFleetId; j.sourceFleetId = e.fleetId;
            j.weaponType = e.weaponType; j.weaponFamily = family; j.shieldDamage = e.shieldDamage;
            j.color = weaponColor(e.weaponType, shooterOwner); j.vfxKey = "vfx.impact.shield";
            addJob(j, ev);
            const auto r = cueKeys(audio::GameCue::ShieldHit, 0);
            addSound(time, audio::GameCue::ShieldHit, r.sfxKeys, r.priority, ev);
        }
        if (e.hullDamage > 0) {
            VisualJob j;
            j.kind = JobKind::HullImpact; j.start = time; j.duration = cfg.impactTicks * 1.5 * tk;
            j.fleetId = e.targetFleetId; j.targetFleetId = e.targetFleetId; j.sourceFleetId = e.fleetId;
            j.weaponType = e.weaponType; j.weaponFamily = family; j.hullDamage = e.hullDamage; j.shieldDamage = e.shieldDamage;
            j.destroysTarget = e.destroyed; j.color = weaponColor(e.weaponType, shooterOwner); j.vfxKey = "vfx.impact.hull";
            addJob(j, ev);
            const auto r = cueKeys(audio::GameCue::HullHit, 0);
            addSound(time, audio::GameCue::HullHit, r.sfxKeys, r.priority, ev);
        }
        impacts.push_back({time, e.targetFleetId, e.fleetId, e.shieldDamage, e.destroyed, shooterOwner});
    };

    std::vector<std::pair<int, double>> killTimes; // target -> impact time of the killing shot
    bool torpedoBarked = false;
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        const int owner = ownerOf(e.fleetId);
        const double t0 = at(e.tick);
        switch (e.kind) {
        case CombatEventKind::ManeuverStart:
        case CombatEventKind::ManeuverEnd:
            break;
        case CombatEventKind::Fire: {
            const int family = std::clamp(e.weaponFamily, 0, 2);
            const int n = std::max(1, e.projectiles);
            const Rgb color = weaponColor(e.weaponType, owner);
            const std::string vfx = vfxWeaponKey(e.weaponType, family);
            const auto keys = weaponSoundKeys(e.weaponType, family, owner);
            VisualJob base;
            base.fleetId = e.fleetId; base.targetFleetId = e.targetFleetId; base.mountIndex = e.mountIndex;
            base.weaponType = e.weaponType; base.weaponFamily = family; base.count = n; base.color = color; base.vfxKey = vfx;
            base.outcome = e.intercepted ? ShotOutcome::Intercepted : (e.hit ? ShotOutcome::Hit : ShotOutcome::Miss);
            base.shieldDamage = e.shieldDamage; base.hullDamage = e.hullDamage; base.destroysTarget = e.destroyed;
            double impactTime = t0;
            if (family == 0) {
                for (int k = 0; k < n; ++k) {
                    VisualJob j = base;
                    j.kind = JobKind::Beam; j.index = k;
                    j.start = t0 + k * cfg.beamEmitterStaggerTicks * tk;
                    j.duration = std::max(0.1, cfg.beamTicks - k * cfg.beamEmitterStaggerTicks) * tk;
                    if (!e.hit) j.overshootCells = cfg.missOvershootCells;
                    addJob(j, i);
                }
                addSound(t0, audio::GameCue::BeamFire, keys, 3, i);
                impactTime = t0 + 0.25 * tk;
            } else if (family == 1) {
                const double stagger = std::min(cfg.pulseStaggerTicks, 0.9 / n);
                for (int k = 0; k < n; ++k) {
                    VisualJob j = base;
                    j.kind = JobKind::PulseBolt; j.index = k;
                    j.start = t0 + k * stagger * tk;
                    j.duration = cfg.pulseFlightTicks * tk;
                    if (!e.hit) j.overshootCells = cfg.missOvershootCells;
                    addJob(j, i);
                    addSound(j.start, audio::GameCue::BeamFire, keys, 3, i);
                    impactTime = j.end();
                }
            } else {
                const double arrive = std::max(at(e.impactTick), t0 + cfg.minTorpedoFlightTicks * tk);
                for (int k = 0; k < n; ++k) {
                    VisualJob j = base;
                    j.kind = JobKind::Torpedo; j.index = k;
                    j.start = t0 + k * cfg.torpedoStaggerTicks * tk;
                    const double flight = std::max(0.2 * tk, arrive - t0);
                    j.arcCells = (k % 2 == 0 ? 1.0 : -1.0) * cfg.torpedoArcCells * (1.0 + 0.3 * (k / 2));
                    if (e.intercepted) { j.endFraction = cfg.interceptFraction; j.duration = flight * cfg.interceptFraction; }
                    else if (!e.hit) { j.overshootCells = cfg.missOvershootCells; j.duration = flight * 1.35; }
                    else j.duration = flight;
                    addJob(j, i);
                    impactTime = std::max(impactTime, j.start + flight);
                }
                addSound(t0, audio::GameCue::TorpedoLaunch, keys, 3, i);
                if (cfg.playerEmpire > 0 && owner == cfg.playerEmpire && !torpedoBarked) {
                    tl.barks.push_back({t0, "torpedo", 3, e.fleetId});
                    torpedoBarked = true;
                }
                if (e.intercepted) {
                    VisualJob j = base;
                    j.kind = JobKind::PointDefence;
                    const double flight = std::max(0.2 * tk, arrive - t0);
                    j.start = t0 + flight * cfg.interceptFraction; j.duration = cfg.impactTicks * tk;
                    j.vfxKey = "vfx.intercept";
                    addJob(j, i);
                    const auto r = cueKeys(audio::GameCue::HullHit, 0);
                    addSound(j.start, audio::GameCue::HullHit, r.sfxKeys, r.priority, i);
                }
            }
            if (e.hit && !e.intercepted) {
                addImpacts(e, impactTime, i, family);
                if (e.destroyed) killTimes.push_back({e.targetFleetId, impactTime});
            }
            break;
        }
        case CombatEventKind::Ram: {
            VisualJob j;
            j.kind = JobKind::Ram; j.fleetId = e.fleetId; j.targetFleetId = e.targetFleetId;
            j.start = t0 - 0.5 * tk > 0 ? t0 - 0.5 * tk : t0; j.duration = at(std::max(e.impactTick, e.tick)) + 0.5 * tk - j.start;
            j.shieldDamage = e.shieldDamage; j.hullDamage = e.hullDamage; j.destroysTarget = e.destroyed;
            j.color = {1.0f, 0.6f, 0.2f};
            addJob(j, i);
            CombatEvent hitEvent = e;
            hitEvent.hit = true;
            addImpacts(hitEvent, at(std::max(e.impactTick, e.tick)), i, 0);
            if (e.destroyed) killTimes.push_back({e.targetFleetId, at(std::max(e.impactTick, e.tick))});
            break;
        }
        case CombatEventKind::Cloak: {
            VisualJob j;
            j.kind = JobKind::CloakShimmer; j.fleetId = e.fleetId; j.start = t0; j.duration = cfg.cloakTicks * tk;
            j.color = {0.55f, 0.8f, 1.0f}; j.vfxKey = "vfx.cloak";
            addJob(j, i);
            auto& t = trackFor(e.fleetId);
            // A ship that starts cloaked (round 1 marker) is already invisible: no fade-out needed.
            if (!t.startsCloaked) t.cloakAt = t0;
            const auto r = cueKeys(audio::GameCue::Cloak, owner);
            addSound(t0, audio::GameCue::Cloak, r.sfxKeys, r.priority, i);
            break;
        }
        case CombatEventKind::Decloak: {
            VisualJob j;
            j.kind = JobKind::Decloak; j.fleetId = e.fleetId; j.start = t0; j.duration = cfg.cloakTicks * tk;
            j.color = {0.55f, 0.8f, 1.0f}; j.vfxKey = "vfx.decloak";
            addJob(j, i);
            trackFor(e.fleetId).decloakAt = t0;
            const auto r = cueKeys(audio::GameCue::Decloak, owner);
            addSound(t0, audio::GameCue::Decloak, r.sfxKeys, r.priority, i);
            break;
        }
        case CombatEventKind::Destroyed: {
            double when = t0 + 0.3 * tk;
            for (const auto& k : killTimes) if (k.first == e.fleetId) when = std::max(t0, k.second);
            VisualJob j;
            j.kind = JobKind::Explosion; j.fleetId = e.fleetId; j.targetFleetId = e.fleetId;
            j.start = when; j.duration = cfg.explosionTicks * tk; j.color = {1.0f, 0.7f, 0.3f}; j.vfxKey = "vfx.explosion";
            addJob(j, i);
            auto& t = trackFor(e.fleetId);
            t.hideAt = t.hideAt >= 0 ? std::min(t.hideAt, when + 0.35 * j.duration) : when + 0.35 * j.duration;
            const auto r = cueKeys(audio::GameCue::ShipDestroyed, 0);
            addSound(when, audio::GameCue::ShipDestroyed, r.sfxKeys, r.priority, i);
            break;
        }
        case CombatEventKind::Retreated: {
            VisualJob j;
            j.kind = JobKind::WarpOut; j.fleetId = e.fleetId; j.start = t0; j.duration = cfg.warpTicks * tk;
            j.color = {0.7f, 0.85f, 1.0f}; j.vfxKey = "vfx.warp.out";
            addJob(j, i);
            auto& t = trackFor(e.fleetId);
            t.hideAt = t.hideAt >= 0 ? std::min(t.hideAt, j.end()) : j.end();
            const auto r = cueKeys(audio::GameCue::Retreat, 0);
            addSound(t0, audio::GameCue::Retreat, r.sfxKeys, r.priority, i);
            break;
        }
        case CombatEventKind::Disengage: {
            VisualJob j;
            j.kind = JobKind::Disengage; j.fleetId = e.fleetId; j.start = t0; j.duration = std::max(1, cfg.moveEndTick) * tk;
            j.color = {0.7f, 0.85f, 1.0f};
            addJob(j, i);
            break;
        }
        case CombatEventKind::Truce: {
            VisualJob j;
            j.kind = JobKind::Truce; j.start = t0; j.duration = tk;
            addJob(j, i);
            break;
        }
        }
    }

    // Barks: shields crossing 75/50/25 percent on the player's ships (the worst crossing of the round), and kills.
    std::stable_sort(impacts.begin(), impacts.end(), [](const Impact& a, const Impact& b) { return a.time < b.time; });
    if (cfg.playerEmpire > 0) {
        std::vector<std::pair<int, int>> shields; // fleet -> current
        for (const auto& s : info) shields.push_back({s.fleetId, s.shields});
        BarkJob worst;
        int worstLevel = 100;
        bool killBarked = false;
        for (const auto& imp : impacts) {
            const auto* target = infoFor(imp.target);
            if (target && target->ownerId == cfg.playerEmpire && target->maxShields > 0 && imp.shieldDamage > 0) {
                for (auto& s : shields) if (s.first == imp.target) {
                    const int before = s.second * 100 / target->maxShields;
                    s.second = std::max(0, s.second - imp.shieldDamage);
                    const int after = s.second * 100 / target->maxShields;
                    for (int level : {25, 50, 75})
                        if (before > level && after <= level && level < worstLevel) {
                            worstLevel = level;
                            worst = {imp.time, "shields." + std::to_string(level), level == 25 ? 7 : level == 50 ? 6 : 5, imp.target};
                            break;
                        }
                }
            }
            if (imp.destroyed && !killBarked && imp.shooterOwner == cfg.playerEmpire && ownerOf(imp.target) != cfg.playerEmpire) {
                tl.barks.push_back({imp.time + 0.3 * tk, "kill", 5, imp.source});
                killBarked = true;
            }
        }
        if (worstLevel < 100) tl.barks.push_back(worst);
    }

    std::stable_sort(tl.jobs.begin(), tl.jobs.end(), [](const VisualJob& a, const VisualJob& b) {
        if (a.start != b.start) return a.start < b.start;
        if (a.eventIndex != b.eventIndex) return a.eventIndex < b.eventIndex;
        return a.index < b.index;
    });
    std::stable_sort(tl.sounds.begin(), tl.sounds.end(), [](const SoundJob& a, const SoundJob& b) { return a.time < b.time; });
    std::stable_sort(tl.barks.begin(), tl.barks.end(), [](const BarkJob& a, const BarkJob& b) { return a.time < b.time; });
    tl.endSeconds = tl.roundSeconds;
    for (const auto& j : tl.jobs) tl.endSeconds = std::max(tl.endSeconds, j.end());
    return tl;
}

// ---------------------------------------------------------------------------------------------------------------
// Cinematic camera director
// ---------------------------------------------------------------------------------------------------------------

// Picks what the cinematic camera looks at: the most interesting active job (explosion > torpedo impact > hull hit >
// ram > torpedo > decloak > beam/pulse), held for at least minShotSeconds so the camera never twitches.
struct CameraShot {
    int subjectFleetId = 0; // Frame this ship ...
    int otherFleetId = 0;   // ... with this one in the background (0 = none).
    double since = 0.0;
    bool operator==(const CameraShot&) const = default;
};

inline int jobInterest(JobKind k) {
    switch (k) {
    case JobKind::Explosion: return 10;
    case JobKind::HullImpact: return 7;
    case JobKind::Ram: return 7;
    case JobKind::Torpedo: return 6;
    case JobKind::Decloak: return 5;
    case JobKind::PointDefence: return 5;
    case JobKind::WarpOut: return 4;
    case JobKind::ShieldImpact: return 4;
    case JobKind::PulseBolt: return 3;
    case JobKind::Beam: return 3;
    case JobKind::CloakShimmer: return 2;
    case JobKind::Disengage: return 1;
    case JobKind::Truce: return 0;
    }
    return 0;
}

class CinematicDirector {
public:
    double minShotSeconds = 1.4;
    CameraShot shot;

    // Returns true when the shot changed.
    bool update(const RoundTimeline& tl, double t, int preferredFleetId = 0) {
        const VisualJob* best = nullptr;
        int bestScore = -1;
        for (const auto& j : tl.jobs) {
            if (j.start > t) break;
            if (t > j.end() || j.fleetId == 0) continue;
            int score = jobInterest(j.kind) * 4;
            if (preferredFleetId != 0 && (j.fleetId == preferredFleetId || j.targetFleetId == preferredFleetId)) score += 2;
            if (score > bestScore) { bestScore = score; best = &j; }
        }
        if (!best) return false;
        CameraShot next;
        const bool shooterShot = best->kind == JobKind::Beam || best->kind == JobKind::PulseBolt || best->kind == JobKind::Torpedo || best->kind == JobKind::Ram;
        next.subjectFleetId = shooterShot ? best->fleetId : best->targetFleetId != 0 ? best->targetFleetId : best->fleetId;
        next.otherFleetId = shooterShot ? best->targetFleetId : best->sourceFleetId;
        next.since = t;
        if (next.subjectFleetId == shot.subjectFleetId && next.otherFleetId == shot.otherFleetId) return false;
        const bool urgent = best->kind == JobKind::Explosion && shot.subjectFleetId != next.subjectFleetId;
        if (shot.subjectFleetId != 0 && t - shot.since < minShotSeconds && !urgent) return false;
        shot = next;
        return true;
    }
    void reset() { shot = {}; }
};

} // namespace botf::combatfx
