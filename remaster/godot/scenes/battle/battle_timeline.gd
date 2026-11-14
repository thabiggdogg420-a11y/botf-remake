class_name BattleTimeline
extends RefCounted
## Engine-independent playback schedule for one tactical rules 3 round.
##
## GDScript port of remaster/game/Source/BOTFRemake/Private/BotfCombatTimeline.h (keep the two in step).
## Turns a round's `events` (docs/COMBAT-RULES.md, "Tactical rules 3") into:
##   * one ShipTrack per ship: eased Hermite flight from the ManeuverStart pose to the ManeuverEnd pose, shortest-arc
##     facing, banking into turns and sideways slides (every curve starts and ends at rest, nothing snaps),
##     cloak / decloak fades and the moment the ship disappears (destroyed or warped out);
##   * visual jobs: beams, staggered pulse bolts, arcing torpedoes (hit, miss past the target, point-defence
##     intercept), shield and hull impacts, rams, cloak shimmer, decloak, explosions, warp-outs;
##   * sound jobs and bark jobs timed to the visual jobs.
## Everything is in the tactical plane (cells, degrees, seconds) and deterministic.
##
## Cinematic playback (Config.cinematic, M1 battleplay; Simon, 9 Nov 2026: "all ships seem to fire their weapons at the
## same time and only seem to move between turns, I want the combat to play out naturally similar to how ships fight in
## the shows"). Godot only: the frozen Unreal port keeps the tick clock. The core resolves movement in ticks 0-4 and
## fires mount i of every ship at tick 5 + i, so a tick clock draws every ship's first mount at one instant after the
## ships have stopped. The cinematic clock keeps the core's ticks as the order of play but:
##   * ships fly their plotted moves over the whole round on a cruise profile (speed up, cruise, slow down);
##   * each fire tick owns a window of the round sized by how many shots it holds; a shot launches inside its window
##     when the mount's arc bears on the target along the flown paths, with deterministic jitter;
##   * torpedoes launch in their fire tick's window and fly 1.2-2.6 s by the core's range (real travel time);
##   * small craft hold their wing formation mid-round and ease back onto their own cells at both ends;
##   * impacts on one ship keep the core's resolution order (event resolveOrder when the bridge sends it, else
##     impact tick, launch-before-arrival, event order), and a ship destroyed in a tick launches its shots before the
##     blow that kills it. Hits, damage, kills and their order are the core's; only when they show is chosen here.

const TICKS_PER_ROUND := 12

enum Job { BEAM, PULSE, TORPEDO, SHIELD, HULL, INTERCEPT, RAM, CLOAK, DECLOAK, EXPLOSION, WARP, DISENGAGE, TRUCE }
const JOB_NAMES := ["beam", "pulse", "torpedo", "shield", "hull", "intercept", "ram", "cloak", "decloak",
	"explosion", "warp", "disengage", "truce"]
enum Outcome { HIT, MISS, INTERCEPTED }

## Owner faction ids used by the tactical scene: 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi.
const FACTION_PREFIX := {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer", 6: "dom", 7: "borg", 8: "breen"} # M4: Dominion, Borg and Breen hulls (ship.dom.* / ship.borg.* / ship.breen.*) resolve by class name too
const TORPEDO_TYPES := ["SpatialTorpedo", "PhotonicTorpedo", "PhotonTorpedo", "QuantumTorpedo", "PlasmaTorpedo",
	"TricobaltDevice", "Torpedo"]
const WEAPON_TYPES := ["PhaseCannon", "PhaserBank", "PhaserArray", "PulsePhaser", "Disruptor", "DisruptorCannon",
	"PlasmaCannon", "PolaronBeam", "CuttingBeam", "EnergyDampener", "TholianWeb", "BioEnergy", "TemporalWeapon",
	"SpatialTorpedo", "PhotonicTorpedo", "PhotonTorpedo", "QuantumTorpedo", "PlasmaTorpedo", "TricobaltDevice", "Torpedo"]
const POSE_KINDS := ["ManeuverStart", "ManeuverEnd", "Fire", "Ram", "Retreated", "Disengage", "Cloak", "Decloak",
	"Destroyed"]


## Timeline configuration (TimelineConfig).
class Config:
	var round_seconds := 6.0
	var move_end_tick := 4
	var beam_ticks := 1.0
	var beam_emitter_stagger_ticks := 0.08
	var pulse_stagger_ticks := 0.16
	var pulse_flight_ticks := 0.35
	var min_torpedo_flight_ticks := 1.6 ## R3.5: 0.6 crossed the arena in 0.3 s, too fast to follow
	var torpedo_stagger_ticks := 0.1
	var torpedo_arc_cells := 0.3 ## battlechoreo: a spread fans out (0.12 drew a string of beads)
	var miss_overshoot_cells := 2.5
	var intercept_fraction := 0.78
	var impact_ticks := 0.6
	var explosion_ticks := 3.5 # long enough to read the kill (coordinator, 8 Nov)
	var warp_ticks := 1.0
	var cloak_ticks := 1.0
	var bank_per_turn_degree := 0.3
	var bank_per_slide_cell := 14.0
	var max_bank_degrees := 35.0
	var player_empire := 0 ## Barks only for this empire's ships (0 = none).
	# 3D flight. Presentation only: the rules stay on BOTF's 2D grid. Heights are cells above (+) or below the plane.
	var small_ship_meters := 260.0   ## Escorts and smaller fly livelier and form wings; unknown length counts as capital.
	var capital_height_spread := 0.45
	var small_height_spread := 1.5
	var side_height_offset := 0.3    ## The first side holds a little above the plane, the other below.
	var deck_layer_cells := 1.4      ## Tactical rules 4: height of one deck layer (Ship.elevation -1..1), in cells.
	var capital_climb_cells := 0.0   ## Height change per maneuver around the home height: capital ships hold altitude (R3.5).
	var small_climb_cells := 0.35   ## Escorts change level a little, nose pitched with the climb.
	var max_pitch_degrees := 32.0
	var small_bank_scale := 1.6      ## Small craft roll hard into turns; capital ships lumber.
	var capital_bank_scale := 0.7
	var wing_radius_cells := 2.5     ## Small ships of one side starting this close together fly as a wing...
	var wing_size := 4               ## ...of up to this many, in finger-four on the first (lowest id) ship.
	var wing_spacing_cells := 0.55
	# Cinematic clock (see the header). Shares are of round_seconds.
	var cinematic := false
	var fire_from := 0.01          ## The first fire window opens here...
	var fire_to := 0.95            ## ...and the last closes here (torpedoes may still be in flight after it).
	var flight_to := 0.94          ## The plotted moves are flown from 0 to here.
	var cruise_accel := 0.22       ## Share of a flight spent speeding up, and again slowing down.
	var launch_span := 1.0         ## Launches (and beam / bolt impacts) fall in this share of their window...
	var window_lead := 0.9         ## ...which opens this many window widths early (clamped to the first window), so one
	                               ## fire tick's shots overlap the last one's: in the show a fleet's fire ripples, 2-5
	                               ## ships at once, never a whole line on one beat (M1 weaponfx, BATTLE-REFERENCE.md).
	var busy_weight := 0.5         ## Extra window width per shot in a tick: busy ticks get more of the round.
	var idle_weight := 0.3         ## Width of a fire tick with no launches (a busy tick has 1 + busy_weight·n).
	var beam_reach_seconds := 0.12 ## A beam reaches its target this long after it lights.
	var beam_hold_seconds := 0.9   ## Phasers hold (and sweep) this long (footage: 0.4-1.5 s, BATTLE-REFERENCE.md).
	var bolt_gap_seconds := 0.12   ## Disruptor / pulse bolts in one burst.
	var bolt_flight_seconds := 0.34
	var torpedo_stagger_seconds := 0.12 ## Tubes in one spread (footage WotW d_4891: 0.1-0.15 s).
	var min_torpedo_flight_seconds := 1.0  ## A torpedo at point blank flies this long (battlechoreo: footage WotW d_4891, a third of the frame in 0.75 s)...
	var torpedo_seconds_per_tick := 0.15    ## ...plus this per core tick of flight (impactTick - tick - 1, range)...
	var max_torpedo_flight_seconds := 1.6  ## ...up to this. Later only if the core's order on the target needs it.
	var effect_gap_seconds := 0.05 ## Least gap between two impacts on one ship, and between a dying ship's last launch and its death.
	# M1 battlechoreo (BATTLE-REFERENCE.md "Choreography, measured"; Engine proposals 1-5). Presentation only.
	var ship_gap_min := 0.4        ## One ship's shots come this far apart at least (footage: 0.4-1.2 s)...
	var ship_gap_max := 1.2        ## ...and at most; each ship's gap and phase are seeded per ship.
	var escort_gap_min := 0.3      ## Escorts fire as they strafe: their shots bunch into the pass.
	var escort_gap_max := 0.55
	var max_beams_at_once := 4     ## Beams held at once across the battle (footage: 1-4 in a fleet shot).
	var beam_first_seconds := 0.7  ## A beam that cuts out and re-fires: first burst...
	var beam_refire_gap := 0.15    ## ...dark for this long...
	var beam_refire_seconds := 0.6  ## ...second burst (FC d_682). Damage lands once, with the first. Held beams are 0.6-1.5 s (Simon, 13 Nov).
	var beam_refire_share := 0.25  ## Share of beams drawn as two bursts (seeded per shot).
	var burst_pair_gap := 0.125    ## Pulse cannons: pairs this far apart (Tears d_2053)...
	var burst_pairs_min := 3       ## ...3-4 pairs drawn for one core shot.
	var burst_pairs_max := 4
	var choreo := true             ## Ships fly attack runs, turn to bear and drift inside the round.
	var death_seconds := 1.25      ## A dying ship drifts and vents this long before it breaks up.
	# M4 flight: the continuous flight-and-doctrine sim (BattleFlight) that flies every ship and picks when each shot
	# launches. null = the M4 combat planner (_choreograph) on the core's round poses.
	var flight = null
	var encounter := {}            ## The encounter the flight sim seeds from (arena size, immobile ships).


## Pose in the tactical plane: x, y in cells, facing in degrees (0 = +x, 90 = +y), bank = roll (+ = right wing down),
## altitude = cells above (+) or below the plane and pitch = nose up (+) in degrees (both presentation only).
class Pose:
	var x := 0.0
	var y := 0.0
	var facing := 0.0
	var bank := 0.0
	var altitude := 0.0
	var pitch := 0.0
	func _init(px := 0.0, py := 0.0, pf := 0.0, pb := 0.0, pa := 0.0, pp := 0.0) -> void:
		x = px; y = py; facing = pf; bank = pb; altitude = pa; pitch = pp
	func copy() -> Pose:
		return Pose.new(x, y, facing, bank, altitude, pitch)
	func equals(o: Pose) -> bool:
		return x == o.x and y == o.y and facing == o.facing and bank == o.bank and altitude == o.altitude \
			and pitch == o.pitch
	## The plane part only (height and pitch are the 3D flight layer).
	func plane_equals(o: Pose) -> bool:
		return x == o.x and y == o.y and facing == o.facing and bank == o.bank


class Segment:
	var t0 := 0.0
	var t1 := 0.0
	var from: Pose
	var to: Pose
	var tan0 := Vector2.ZERO
	var tan1 := Vector2.ZERO
	var bank_peak := 0.0
	var pitch_peak := 0.0
	var slide_cells := 0
	var maneuver := ""


class ShipTrack:
	var fleet_id := 0
	var owner_id := 0
	var initial := Pose.new()
	var segments: Array[Segment] = []
	var starts_cloaked := false
	var cloak_at := -1.0
	var decloak_at := -1.0
	var hide_at := -1.0
	var cloak_seconds := 1.0
	var length_meters := 0.0
	var small_craft := false  ## Escort size or smaller (Config.small_ship_meters).
	var wing_leader := 0      ## Fleet id of the wing leader this ship holds station on, 0 = flies its own track.
	var wing_back := 0.0      ## Station in the leader's frame (cells: aft, port, up).
	var wing_side := 0.0
	var wing_up := 0.0
	var cruise := 0.0         ## Cinematic: share of each flight spent accelerating (0 = the tick clock's smoothstep).
	# M1 battlechoreo: in-round choreography (presentation only; zero at the round's start and from choreo_end on, so
	# every round starts and ends on the core's poses).
	var style := -1           ## -1 none, 0 capital (holds a slow line), 1 cruiser (turns to bear, drifts), 2 escort (attack runs).
	var choreo_end := 0.0     ## Round time the choreography has fully handed back to the core's flight.
	var run_times := PackedFloat64Array()   ## Escort: keyframe times...
	var run_points: Array[Vector3] = []     ## ...and positions (plane x, plane y, altitude), cells.
	var drift := Vector3.ZERO ## Cruiser / capital: peak drift (plane x, plane y, altitude), cells.
	var aim_turn := 0.0       ## Cruiser / capital: degrees turned to bring the arrays to bear.
	var sway_phase := 0.0
	var kicks: Array[Vector3] = [] ## Hard hits: (time, side +-1, strength 0..1): the ship rolls and turns away.
	var kill_at := -1.0       ## Killing blow: the ship drifts, vents and tumbles until hide_at.
	var hull_max := 0.0
	# M4 combat: a flown path. The flight planner steers the ship (class speed, turn rate and acceleration, an attack run
	# on its target, a break away when hurt, the order's own manoeuvre) and records it every PATH_DT seconds; the pose is
	# read back by cubic interpolation. Six floats a sample: x, y, altitude, facing, bank, pitch. Empty = no flown path.
	var path := PackedFloat32Array()
	var maneuver_last := ""   ## The round's last plotted manoeuvre name (Engage, Charge, Circle/Evade, Retreat ...).
	var hull_frac := 1.0      ## Hull at the round's start over the ship's maximum.
	var intent := ""          ## What the planner made of the ship this round (tools and tests read it).

	## The ship's own flight (BattleTimeline.pose_of adds wing stations).
	func pose_at(t: float) -> Pose:
		var p := initial.copy()
		for s in segments:
			if t <= s.t0:
				return p
			if t >= s.t1:
				p = s.to.copy()
				p.bank = 0.0
				p.pitch = 0.0
				continue
			var u := (t - s.t0) / maxf(1e-9, s.t1 - s.t0)
			var e := BattleTimeline.flight_ease(u, cruise)
			var e2 := e * e
			var e3 := e2 * e
			var h00 := 2 * e3 - 3 * e2 + 1
			var h10 := e3 - 2 * e2 + e
			var h01 := -2 * e3 + 3 * e2
			var h11 := e3 - e2
			p.x = h00 * s.from.x + h10 * s.tan0.x + h01 * s.to.x + h11 * s.tan1.x
			p.y = h00 * s.from.y + h10 * s.tan0.y + h01 * s.to.y + h11 * s.tan1.y
			p.facing = BattleTimeline.wrap_degrees(s.from.facing + BattleTimeline.shortest_turn(s.from.facing, s.to.facing) * e)
			var sn := sin(PI * clampf(u, 0.0, 1.0))
			p.bank = s.bank_peak * sn * sn
			p.altitude = s.from.altitude + (s.to.altitude - s.from.altitude) * e
			p.pitch = s.pitch_peak * sn * sn
			return p
		return p

	## 1 = fully visible, 0 = cloaked or gone; in between = shimmering.
	func visibility_at(t: float) -> float:
		if hide_at >= 0.0 and t >= hide_at:
			return 0.0
		var fade := maxf(1e-6, cloak_seconds)
		var v := 0.0 if starts_cloaked else 1.0
		var changes := [[cloak_at, true], [decloak_at, false]]
		if decloak_at >= 0.0 and (cloak_at < 0.0 or decloak_at < cloak_at):
			changes.reverse()
		for c in changes:
			if c[0] < 0.0 or t < c[0]:
				continue
			var k := BattleTimeline.smoothstep01((t - c[0]) / fade)
			v = v * (1.0 - k) if c[1] else v + (1.0 - v) * k
		return clampf(v, 0.0, 1.0)


# ------------------------------------------------------------------------------------------------------------------
# Result
# ------------------------------------------------------------------------------------------------------------------

var tick_seconds := 0.5
var round_seconds := 6.0
var end_seconds := 6.0
var ships: Array[ShipTrack] = []
## Visual jobs, Dictionaries sorted by start, then event index, then index. Keys: kind (Job), start, duration,
## fleet_id, target_fleet_id, source_fleet_id, mount_index, weapon (type name), family (0 beam, 1 pulse, 2 torpedo),
## index, count, outcome, color, arc_cells, overshoot_cells, end_fraction, shield_damage, hull_damage,
## destroys_target, event_index, vfx_key.
var jobs: Array[Dictionary] = []
## Sound jobs {time, cue, keys: PackedStringArray, priority, event_index}, sorted by time.
var sounds: Array[Dictionary] = []
## Bark jobs {time, detail, priority, fleet_id}, sorted by time. Voice key vo.combat.<detail>.<fac>.
var barks: Array[Dictionary] = []
## Per input event: jobs / tracks produced from it (tests require every entry > 0).
var consumed: PackedInt32Array = PackedInt32Array()
## Round time at which each core tick's window opens (TICKS_PER_ROUND + 1 entries; the last closes the fire phase).
var window_starts := PackedFloat64Array()
var cinematic := false
## Cinematic: small craft fly in their wing's formation only inside (formation_from, formation_until) and ease back onto
## their own core cells at both ends, so every round starts and ends where the core put each ship.
var formation_until := -1.0
## Damage as it lands, in time order: {time, target, source, shield_damage, hull_damage, event_index, order}. `order` is
## the core's resolution rank among impacts (playback keeps it per target).
var impact_log: Array[Dictionary] = []


## Hull and shield points lost by `fleet_id` to impacts shown at or before t (Vector2i(hull, shield)).
func damage_until(fleet_id: int, t: float) -> Vector2i:
	var out := Vector2i.ZERO
	for imp in impact_log:
		if imp.time > t:
			break
		if imp.target == fleet_id:
			out += Vector2i(int(imp.hull_damage), int(imp.shield_damage))
	return out


func track(fleet_id: int) -> ShipTrack:
	for s in ships:
		if s.fleet_id == fleet_id:
			return s
	return null


## Where `s` is at time t: its own flight, or its station in its leader's wing. When the leader is gone (destroyed,
## warped out) the wingman peels off back to its own track over two ticks.
func pose_of(s: ShipTrack, t: float) -> Pose:
	var own := flown(s, t)
	var lead := track(s.wing_leader) if s.wing_leader != 0 else null
	if lead == null or s.path.size() > 0:
		return own
	var w := 1.0
	# A leader that takes its killing blow stops leading then: its wreck tumbles and is hidden later, and a wingman that copied
	# that tumble swung round it (and snapped when the wreck was hidden). The wingman holds the leader's last pose at the blow
	# and peels back to its own track from there.
	var t_dead := lead.kill_at if lead.kill_at >= 0.0 else lead.hide_at
	if t_dead >= 0.0 and t > t_dead:
		w = 1.0 - smoothstep01((t - t_dead) / maxf(1e-6, 2.0 * tick_seconds))
	if formation_until > 0.0:
		var edge := 0.18 * formation_until
		w *= smoothstep01(t / edge) * smoothstep01((formation_until - t) / edge)
	if w <= 0.0:
		return own
	var lt := t if t_dead < 0.0 else minf(t, t_dead - 0.001)
	var lp := flown(lead, lt)
	var f := deg_to_rad(lp.facing)
	var p := Pose.new()
	p.x = own.x + (lp.x - cos(f) * s.wing_back - sin(f) * s.wing_side - own.x) * w
	p.y = own.y + (lp.y - sin(f) * s.wing_back + cos(f) * s.wing_side - own.y) * w
	p.altitude = own.altitude + (lp.altitude + s.wing_up - own.altitude) * w
	if t_dead >= 0.0 and t > t_dead:
		# Peeling off a wreck: the turn back to its own heading is measured once, at the blow, and followed from there (a fresh
		# shortest turn each frame flips the wingman half a circle when its own heading passes opposite the held wreck's).
		var t_h := t_dead - 0.001
		var own_h := flown(s, t_h)
		var back := shortest_turn(lp.facing, own_h.facing) + shortest_turn(own_h.facing, own.facing)
		p.facing = wrap_degrees(lp.facing + back * (1.0 - w))
	else:
		p.facing = wrap_degrees(own.facing + shortest_turn(own.facing, lp.facing) * w)
	p.bank = own.bank + (lp.bank - own.bank) * w
	p.pitch = own.pitch + (lp.pitch - own.pitch) * w
	return p


# ------------------------------------------------------------------------------------------------------------------
# In-round choreography (M1 battlechoreo; presentation only)
# ------------------------------------------------------------------------------------------------------------------

## Half-width (seconds) of the window an escort's bow turn is averaged over (see _run_turn).
const RUN_HEADING_WINDOW := 0.5

const METERS_PER_CELL := 893.0 ## BattleScene CELL x SPREAD x 17.71 m: a Galaxy (640 m) spans 0.72 plane cells as drawn (stage 3: SPREAD 1.8, was 592 at 1.2).


## Where `s` is drawn at t: the core's flight plus its in-round choreography (attack run, drift, reactions, death).
## Equal to pose_at at t = 0, from choreo_end on, and once the ship is hidden.
var _flown_t := NAN
var _flown_cache := {}

func flown(s: ShipTrack, t: float) -> Pose:
	if s.style < 0 and s.kill_at < 0.0 and s.kicks.is_empty():
		return s.pose_at(t)
	# One evaluation per ship per frame (wingmen read their leader's, effects read their shooter's).
	if t != _flown_t:
		_flown_t = t
		_flown_cache.clear()
	var hit = _flown_cache.get(s.fleet_id)
	if hit != null:
		return (hit as Pose).copy()
	var p := _flown_uncached(s, t)
	_flown_cache[s.fleet_id] = p.copy()
	return p


func _flown_uncached(s: ShipTrack, t: float) -> Pose:
	if s.hide_at >= 0.0 and t >= s.hide_at:
		return s.pose_at(t)
	if s.kill_at >= 0.0 and t > s.kill_at:
		return _dying(s, t)
	return _alive(s, t)


static func _choreo_env(s: ShipTrack, t: float) -> float:
	if s.choreo_end <= 0.0:
		return 0.0
	# An escort blends onto a run that begins at its own cell and then flies 4-5 cells/s; blending in over 0.8 s made the
	# hand-over itself peak near 7 cells/s (a visible lurch), so its blend-in takes 1.6 s.
	return smoothstep01(t / (1.6 if s.style == 2 else 0.8)) * smoothstep01((s.choreo_end - t) / 1.0)


## Plane x, plane y, altitude (cells) of the choreographed path without reactions.
static func _alive_pos(s: ShipTrack, t: float) -> Vector3:
	if s.path.size() > 0:
		var q := path_pose(s, t)
		return Vector3(q[0], q[1], q[2])
	var b := s.pose_at(t)
	var bp := Vector3(b.x, b.y, b.altitude)
	var e := _choreo_env(s, t)
	if e <= 0.0 or s.style < 0:
		return bp
	if s.style == 2 and s.run_times.size() >= 2:
		return bp.lerp(_run_point(s, t), e)
	# M4: the drift swells and fades with zero speed at both ends (sin squared), so a capital never lurches at a round's
	# start or end (the first cut ramped 2-3 cells in 0.8 s: 4 cells a second).
	var u := clampf(t / s.choreo_end, 0.0, 1.0)
	var bump := pow(sin(PI * u), 1.5) # M4: gets moving sooner than sin squared, still zero offset at both ends
	# A capital keeps cruising through a round boundary (a battlecruiser sat still for 1.5 s where one round ended and the
	# next began, both drifts at zero speed): a Hermite offset that is 0 at both ends but leaves at, and arrives with, a slow
	# cruise along the bow. The core's poses still land exactly (zero offset at 0 and at choreo_end).
	var T := maxf(s.choreo_end, 0.5)
	var f0 := deg_to_rad(s.pose_at(0.0).facing)
	var f1 := deg_to_rad(s.pose_at(s.choreo_end).facing)
	var cruise := CRUISE_CELLS_PER_SECOND if s.style == 0 else 0.5 * CRUISE_CELLS_PER_SECOND
	var h10 := u * (1.0 - u) * (1.0 - u)
	var h11 := u * u * (u - 1.0)
	var cr := Vector3(cos(f0) * h10 + cos(f1) * h11, sin(f0) * h10 + sin(f1) * h11, 0.0) * (cruise * T)
	return bp + s.drift * bump + cr


## Hermite through the escort's run keyframes.
static func _run_point(s: ShipTrack, t: float) -> Vector3:
	var T := s.run_times
	var P := s.run_points
	var n := T.size()
	if t <= T[0]:
		return P[0]
	if t >= T[n - 1]:
		return P[n - 1]
	var k := 0
	while k < n - 2 and t >= T[k + 1]:
		k += 1
	var dt := T[k + 1] - T[k]
	var u := (t - T[k]) / maxf(dt, 1e-6)
	var m0 := _run_tangent(T, P, k) * dt
	var m1 := _run_tangent(T, P, k + 1) * dt
	var u2 := u * u
	var u3 := u2 * u
	return P[k] * (2 * u3 - 3 * u2 + 1) + m0 * (u3 - 2 * u2 + u) + P[k + 1] * (-2 * u3 + 3 * u2) + m1 * (u3 - u2)


static func _run_tangent(T: PackedFloat64Array, P: Array[Vector3], i: int) -> Vector3:
	var n := T.size()
	var a := maxi(0, i - 1)
	var b := mini(n - 1, i + 1)
	return (P[b] - P[a]) / maxf(T[b] - T[a], 1e-6)


## Reaction shove: a hard hit pushes the ship sideways a little (cells, plane + altitude).
## How far an escort's bow is turned off the core's facing by its run at t: toward the direction of travel, let go of where
## the run is nearly still, and averaged over +-RUN_HEADING_WINDOW so the bow swings round at a visible rate (at the
## turnaround the direction of travel reverses inside a frame; a raw heading would snap the hull and its wingmen with it).
static func _run_turn(s: ShipTrack, t: float) -> float:
	var sum := 0.0
	const N := 8
	for i in range(-N, N + 1):
		var ti := t + RUN_HEADING_WINDOW * float(i) / float(N)
		var e := _choreo_env(s, ti)
		if e <= 0.0:
			continue
		var qa := _alive_pos(s, ti - 0.05)
		var qb := _alive_pos(s, ti + 0.05)
		var flat := Vector2(qb.x - qa.x, qb.y - qa.y).length() / 0.1
		var wv := e * clampf(flat / 0.6, 0.0, 1.0)
		var heading := rad_to_deg(atan2(qb.y - qa.y, qb.x - qa.x))
		sum += shortest_turn(s.pose_at(ti).facing, heading) * wv
	return sum / float(2 * N + 1)


static func _kick_pos(s: ShipTrack, t: float, facing: float) -> Vector3:
	var out := Vector3.ZERO
	var e := _choreo_env(s, t) * kick_scale(s)
	for k in s.kicks:
		var d := t - k.x
		if d <= 0.0:
			continue
		var f := deg_to_rad(facing)
		var push := k.y * k.z * 0.3 * (1.0 - exp(-d / 0.4)) * e
		out += Vector3(-sin(f) * push, cos(f) * push, -0.1 * k.z * (1.0 - exp(-d / 0.4)) * e)
	return out


func _alive(s: ShipTrack, t: float) -> Pose:
	if s.path.size() > 0:
		return _alive_flown(s, t)
	var p := s.pose_at(t)
	var e := _choreo_env(s, t)
	var q := _alive_pos(s, t) + _kick_pos(s, t, p.facing)
	var out := p.copy()
	out.x = q.x
	out.y = q.y
	out.altitude = q.z
	var face := p.facing
	if s.style == 2 and s.run_times.size() >= 2 and e > 0.0:
		var h := 0.05
		var qa := _alive_pos(s, t - h)
		var qb := _alive_pos(s, t + h)
		var v := (qb - qa) / (2.0 * h)
		var flat := Vector2(v.x, v.y).length()
		var wv := e * clampf(flat / 0.6, 0.0, 1.0)
		face = p.facing + _run_turn(s, t)
		var qm := _alive_pos(s, t)
		var h0 := rad_to_deg(atan2(qm.y - qa.y, qm.x - qa.x))
		var h1 := rad_to_deg(atan2(qb.y - qm.y, qb.x - qm.x))
		var rate := shortest_turn(h0, h1) / h
		out.bank = p.bank + clampf(-rate * 0.4, -70.0, 70.0) * wv
		out.pitch = p.pitch + clampf(rad_to_deg(atan2(v.z, maxf(flat, 1e-3))), -35.0, 35.0) * wv
	elif s.style >= 0:
		var ae := _aim_env(s, t)
		var calm := smoothstep01(minf(t, s.choreo_end - t) / 1.5)
		face = p.facing + s.aim_turn * ae + sin(t * 0.7 + s.sway_phase) * 3.0 * calm
		out.bank = p.bank + sin(t * 0.5 + s.sway_phase) * 3.0 * calm
	for k in s.kicks:
		var d := t - k.x
		if d <= 0.0:
			continue
		var a := exp(-d / 0.7) * smoothstep01(d / 0.15)
		out.bank += k.y * k.z * 32.0 * a
		face += k.y * k.z * 24.0 * (1.0 - exp(-d / 0.5)) * e
	out.facing = wrap_degrees(face)
	return out


## How far the bow has slewed toward the aim by time t (0..1): it takes as long as the class turn rate needs, in and out.
static func _aim_env(s: ShipTrack, t: float) -> float:
	var rate: float = CLASS_TURN[clampi(s.style, 0, 3)]
	var tt := clampf(2.0 * absf(s.aim_turn) / rate, 1.2, 0.45 * maxf(s.choreo_end, 1.0))
	return smoothstep01(t / tt) * smoothstep01((s.choreo_end - t) / tt)


## M4 combat: the planner's flown path at time t as [x, y, altitude, facing, bank, pitch]. Catmull-Rom through the
## samples (twenty a second), so position and heading are smooth at any frame rate. Outside the path: the core's pose.
const PATH_DT := 0.1

static func path_pose(s: ShipTrack, t: float) -> Array:
	var n := s.path.size() / 6
	var f := t / PATH_DT
	if n < 2 or f <= 0.0 or f >= float(n - 1):
		var b := s.pose_at(t)
		if n >= 2 and f <= 0.0:
			return [s.path[0], s.path[1], s.path[2], s.path[3], 0.0, 0.0]
		return [b.x, b.y, b.altitude, b.facing, 0.0, 0.0]
	var k := int(f)
	var u := f - float(k)
	var i0 := maxi(k - 1, 0) * 6
	var i1 := k * 6
	var i2 := (k + 1) * 6
	var i3 := mini(k + 2, n - 1) * 6
	var out := []
	for c in 3:
		var p0 := s.path[i0 + c]
		var p1 := s.path[i1 + c]
		var p2 := s.path[i2 + c]
		var p3 := s.path[i3 + c]
		out.append(0.5 * ((2.0 * p1) + (-p0 + p2) * u + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * u * u + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * u * u * u))
	out.append(wrap_degrees(s.path[i1 + 3] + shortest_turn(s.path[i1 + 3], s.path[i2 + 3]) * u))
	out.append(lerpf(s.path[i1 + 4], s.path[i2 + 4], u))
	out.append(lerpf(s.path[i1 + 5], s.path[i2 + 5], u))
	return out


## M4 flight: a flown hull's reaction to a hard hit scales with its size (a capital is not shoved like an escort: the
## full kick moved a Galaxy at 460 m/s).
static func kick_scale(s: ShipTrack) -> float:
	if s.path.is_empty():
		return 1.0
	return clampf(220.0 / maxf(s.length_meters, 1.0), 0.2, 1.0)


## An alive ship on its flown path, with the reactions to hard hits on top.
func _alive_flown(s: ShipTrack, t: float) -> Pose:
	var q := path_pose(s, t)
	var out := s.pose_at(t).copy()
	var kp := _kick_pos(s, t, float(q[3]))
	out.x = float(q[0]) + kp.x
	out.y = float(q[1]) + kp.y
	out.altitude = float(q[2]) + kp.z
	out.bank = float(q[4])
	out.pitch = float(q[5])
	var face := float(q[3])
	var e := _choreo_env(s, t) * kick_scale(s)
	for k in s.kicks:
		var d := t - k.x
		if d <= 0.0:
			continue
		var a := exp(-d / 0.7) * smoothstep01(d / 0.15)
		out.bank += k.y * k.z * 32.0 * a
		face += k.y * k.z * 24.0 * (1.0 - exp(-d / 0.5)) * e
	out.facing = wrap_degrees(face)
	return out


## Proposal 5: crippled by the killing blow, the ship keeps its way on, sinks a little, rolls and yaws until it breaks up.
func _dying(s: ShipTrack, t: float) -> Pose:
	var k := s.kill_at
	var p0 := _alive(s, k)
	var a := _alive_pos(s, k - 0.05)
	var b := _alive_pos(s, k + 0.05)
	var v := (b - a) / 0.1
	var d := t - k
	var side := 1.0 if unit_hash(s.fleet_id, 51) < 0.5 else -1.0
	var travel := 1.1 * (1.0 - exp(-d / 1.1))
	var out := p0.copy()
	out.x += v.x * travel
	out.y += v.y * travel
	out.altitude += v.z * travel - 0.12 * d * d
	out.bank += side * 45.0 * d
	out.pitch += 9.0 * d
	out.facing = wrap_degrees(out.facing + side * 14.0 * d)
	return out


## The ship `s` mostly fires at, else the nearest enemy at mid-round.
static func _choreo_target(tl: BattleTimeline, s: ShipTrack, counts: Dictionary, T_end: float) -> ShipTrack:
	var best := 0
	var best_n := 0
	for tgt in counts.get(s.fleet_id, {}):
		if counts[s.fleet_id][tgt] > best_n:
			best_n = counts[s.fleet_id][tgt]
			best = tgt
	if best != 0 and tl.track(best) != null:
		return tl.track(best)
	var me := s.pose_at(0.5 * T_end)
	var near: ShipTrack = null
	var nd := INF
	for o in tl.ships:
		if o.owner_id == s.owner_id or o.fleet_id == s.fleet_id:
			continue
		var op := o.pose_at(0.5 * T_end)
		var dd := Vector2(op.x - me.x, op.y - me.y).length()
		if dd < nd:
			nd = dd
			near = o
	return near


## Class speeds (cells a second), turn rates (degrees a second) and accelerations (cells a second squared), by class: 0
## capital, 1 cruiser, 2 escort, 3 strike craft. A cell is 592 m as drawn, so a capital ship cruises at about 180 m/s and
## turns 10 degrees a second, a cruiser at 300 m/s and 24, an escort at 590 m/s and 60, a fighter at 740 m/s and 90 (the
## LCARS screensaver's fleet table, Battle.cs MakeShip: capital 125-190 m/s, cruiser 210-290, escort 470-540, fighter
## 700-820). M4 combat: the first cut flew escorts at 4-5 cells a second, 2.5 km/s, 17 hull lengths a second.
const CLASS_SPEED := [0.30, 0.50, 1.00, 1.25]
const CRUISE_CELLS_PER_SECOND := 0.12   ## M4: a capital's slow cruise through a round boundary (cells a second).
const CLASS_TURN := [10.0, 24.0, 60.0, 90.0]
const CLASS_ACCEL := [0.10, 0.22, 0.80, 1.10]
const STRIKE_CRAFT_METERS := 120.0
const BREAK_OFF_HULL := 0.3      ## A ship under this share of its hull breaks off the fight.


static func flight_class(s: ShipTrack) -> int:
	if s.style == 2 and s.length_meters > 0.0 and s.length_meters <= STRIKE_CRAFT_METERS:
		return 3
	return maxi(0, s.style)


## Sets each ship's flight from the round's orders, targets and shots (sched: event -> [launch, impact]), M4 combat.
## Capitals and cruisers keep the core's path and add a purposeful drift: closing on the target, circling it, backing
## off, holding or breaking away, with the bow slewing to a broadside at the class turn rate. Escorts and strike craft are
## flown by _fly_escort, a steering simulation. Every ship starts and ends on the core's pose.
static func _choreograph(tl: BattleTimeline, events: Array, sched: Dictionary, info_by_id: Dictionary, cfg: Config) -> void:
	var T_end := cfg.flight_to * tl.round_seconds
	var counts := {}
	var shots := {}
	for i in sched:
		var e: Dictionary = events[i]
		var fid := int(_num(e, "fleetId"))
		var tgt := int(_num(e, "targetFleetId"))
		if not counts.has(fid):
			counts[fid] = {}
			shots[fid] = []
		counts[fid][tgt] = counts[fid].get(tgt, 0) + 1
		shots[fid].append(float(sched[i][0]))
	for s in tl.ships:
		s.style = ship_style(s.length_meters, cfg)
		s.choreo_end = T_end
		s.sway_phase = unit_hash(s.fleet_id, 41) * TAU
		var inf: Dictionary = info_by_id.get(s.fleet_id, {})
		s.hull_max = float(inf.get("hull", 0))
		var mx := float(inf.get("maxHull", 0))
		s.hull_frac = clampf(float(inf.get("hull", 0)) / mx, 0.0, 1.0) if mx > 0.0 and inf.has("hull") else 1.0
	# Cruisers and capitals first (the escorts' runs are laid against where their targets are drawn).
	for s in tl.ships:
		if s.style != 0 and s.style != 1:
			continue
		var tg := _choreo_target(tl, s, counts, T_end)
		var me := s.pose_at(0.5 * T_end)
		var bearing := me.facing
		var range_cells := 6.0
		if tg != null:
			var tp := tg.pose_at(0.5 * T_end)
			bearing = rad_to_deg(atan2(tp.y - me.y, tp.x - me.x))
			range_cells = Vector2(tp.x - me.x, tp.y - me.y).length()
		var h1 := unit_hash(s.fleet_id, 42)
		var h2 := unit_hash(s.fleet_id, 43)
		var m := s.maneuver_last
		var hurt := s.hull_frac < BREAK_OFF_HULL
		var turn_cap := 75.0 if s.style == 1 else 40.0
		var side := 1.0 if h2 < 0.5 else -1.0
		var d := (0.6 + 0.4 * h1) if s.style == 1 else (0.35 + 0.2 * h1)
		var drift_dir := me.facing
		var want_face := me.facing
		s.intent = "duel"
		if hurt or m == "Retreat":
			# Turn away and make for the edge: the stern to the enemy.
			want_face = bearing + 180.0 - side * 25.0
			drift_dir = bearing + 180.0
			d *= 0.9
			s.intent = "break off"
		elif m == "Hold":
			want_face = bearing + side * 70.0 if s.style == 1 else bearing
			d *= 0.25
			s.intent = "hold"
		elif m == "Charge" or m == "Assault" or m == "Ram":
			# Bow on, closing the range.
			want_face = bearing
			drift_dir = bearing
			d *= 1.2 if range_cells > 3.0 else 0.5
			s.intent = "close"
		elif m.begins_with("Circle") or m == "Harry" or m == "Strafe" or m == "Flyby" or m == "Evade":
			# Broadside and slide round the target.
			want_face = bearing + side * 80.0
			drift_dir = bearing + side * 90.0
			s.intent = "circle"
		else:
			# Engage: swing to a broadside on the target and slide along it, a capital easing its bow round more slowly.
			if s.style == 1:
				var f1 := wrap_degrees(bearing - 70.0)
				var f2 := wrap_degrees(bearing + 70.0)
				want_face = f1 if absf(shortest_turn(me.facing, f1)) < absf(shortest_turn(me.facing, f2)) else f2
				drift_dir = want_face + (h2 - 0.5) * 60.0
			else:
				want_face = me.facing + shortest_turn(me.facing, bearing) * 0.5 + (h2 - 0.5) * 50.0
				drift_dir = want_face + (h1 - 0.5) * 120.0
		s.aim_turn = clampf(shortest_turn(me.facing, want_face), -turn_cap, turn_cap)
		# The bow can only slew at the class turn rate: what the round's time allows.
		var lim: float = CLASS_TURN[s.style] * 2.2
		s.aim_turn = clampf(s.aim_turn, -lim, lim)
		var hr := deg_to_rad(drift_dir)
		s.drift = Vector3(cos(hr) * d, sin(hr) * d, (h2 * 2.0 - 1.0) * (0.5 if s.style == 1 else 0.4))
	# Escorts and strike craft: a flown attack run (or the order's own manoeuvre) on their target.
	for s in tl.ships:
		if s.style != 2 or s.wing_leader != 0:
			continue
		var tg := _choreo_target(tl, s, counts, T_end)
		if tg == null:
			continue
		var my: Array = shots.get(s.fleet_id, [])
		my.sort()
		_fly_escort(tl, s, tg, my, T_end)
	# Wingmen hold their station on the leader's flown run, then break to their own cells (formations hold and break).
	for s in tl.ships:
		if s.style != 2 or s.wing_leader == 0:
			continue
		var lead := tl.track(s.wing_leader)
		if lead == null:
			continue
		_fly_escort(tl, s, lead, [], T_end, lead)


## Steering simulation of one escort or strike craft (M4 combat): from the core's start pose to the core's end pose it
## flies what its order says, at its class's speed, turn rate and acceleration, with no teleporting and no corner it cannot
## turn: approach the target, a strafing pass inside about a hull length of it timed on its own shots, a break away, and
## coming about for the next pass; Circle and Harry orbit the target, Evade jinks, Charge and Ram close straight in,
## Retreat and a crippled hull run for the edge, Hold stays put. The last 2.4 s brake onto the core's cell and facing.
static func _fly_escort(tl: BattleTimeline, s: ShipTrack, tg: ShipTrack, my_shots: Array, T_end: float, lead: ShipTrack = null) -> void:
	var ci := flight_class(s)
	var vmax: float = CLASS_SPEED[ci] * (0.9 + 0.2 * unit_hash(s.fleet_id, 46))
	var turn_rate: float = CLASS_TURN[ci]
	var accel: float = CLASS_ACCEL[ci]
	var b0 := s.pose_at(0.0)
	var b1 := s.pose_at(T_end)
	var pos := Vector3(b0.x, b0.y, b0.altitude)
	var end_pos := Vector3(b1.x, b1.y, b1.altitude)
	var heading := b0.facing
	var speed := 0.0
	var vz := 0.0
	var side := 1.0 if unit_hash(s.fleet_id, 44) < 0.5 else -1.0
	var up := 1.0 if unit_hash(s.fleet_id, 45) < 0.5 else -1.0
	var tlen := tg.length_meters if tg.length_meters > 0.0 else 300.0
	var pass_d := 1.25 * 0.6 * (tlen + s.length_meters) / METERS_PER_CELL + 0.15
	var m := s.maneuver_last
	var hurt := s.hull_frac < BREAK_OFF_HULL
	var mode := "run"
	if lead != null:
		mode = "follow"
	elif hurt or m == "Retreat":
		mode = "flee"
	elif m == "Hold" or (my_shots.is_empty() and m != "Charge" and m != "Ram"):
		mode = "hold" # nothing to shoot this round (or told to hold): it advances with the line instead of running at nothing
	elif m.begins_with("Circle") or m == "Harry":
		mode = "orbit"
	elif m == "Evade":
		mode = "evade"
	elif m == "Charge" or m == "Ram":
		mode = "charge"
	s.intent = {"run": "attack run", "flee": "break off", "hold": "advance", "orbit": "circle", "evade": "evade", "charge": "charge", "follow": "hold formation"}[mode]
	var t_p := 0.45 * T_end
	if not my_shots.is_empty():
		t_p = 0.5 * (float(my_shots[0]) + float(my_shots[-1])) + 0.2
	t_p = clampf(t_p, 1.8, T_end - 3.0)
	var t_arrive := T_end - 2.4
	var n := int(ceil(T_end / PATH_DT)) + 1
	var path := PackedFloat32Array()
	path.resize(n * 6)
	var orbit_r := 2.0 + 0.8 * unit_hash(s.fleet_id, 47) + 0.5 * pass_d
	var tp0 := _point_of(tl, tg, 0.0)
	var orbit_a := atan2(pos.y - tp0.y, pos.x - tp0.x)
	var orbit_w := side * vmax * 0.75 / orbit_r
	var phase := "approach"
	var phase_t := 0.0
	var break_dir := 0.0
	var yaw_f := 0.0
	var pitch_f := 0.0
	var follow_face := heading
	var dock_vel := Vector3.ZERO
	var was_docking := false
	var prev_heading := heading
	var dt := PATH_DT
	for k in n:
		var t := float(k) * dt
		var tp := _point_of(tl, tg, t)
		var to_t := Vector3(tp.x - pos.x, tp.y - pos.y, 0.0)
		var dist := to_t.length()
		var dir_t := to_t / maxf(dist, 1e-4)
		var nrm := Vector3(-dir_t.y, dir_t.x, 0.0) * side
		var goal := end_pos
		var want_speed := vmax
		# Head for the core's cell in time: the last 2.4 s at the latest, earlier when it is far (the ship runs home).
		var rem0 := (end_pos - pos).length()
		var arriving := t >= t_arrive or (T_end - t) <= rem0 / (0.7 * vmax) + vmax / (2.0 * accel) + 0.6 + absf(shortest_turn(heading, rad_to_deg(atan2(end_pos.y - pos.y, end_pos.x - pos.x)))) / turn_rate
		if arriving:
			goal = end_pos
			var rem := rem0
			want_speed = minf(vmax * 1.4, sqrt(2.0 * accel * maxf(rem - 0.04, 0.0)) + 0.05)
		else:
			match mode:
				"follow":
					var lt := minf(t + 0.25, T_end)
					var lp := _point_of(tl, lead, lt)
					var lf := deg_to_rad(tl.flown(lead, lt).facing)
					goal = Vector3(lp.x - cos(lf) * s.wing_back - sin(lf) * s.wing_side, lp.y - sin(lf) * s.wing_back + cos(lf) * s.wing_side, lp.z + s.wing_up)
					var la := _point_of(tl, lead, maxf(t - 0.1, 0.0))
					var lb := _point_of(tl, lead, minf(t + 0.1, T_end))
					var lspeed := Vector2(lb.x - la.x, lb.y - la.y).length() / 0.2
					want_speed = minf(vmax * 1.35, lspeed + (goal - pos).length() * 1.6)
					follow_face = tl.flown(lead, t).facing
				"hold":
					goal = Vector3(b0.x, b0.y, b0.altitude).lerp(end_pos, clampf(t / t_arrive, 0.0, 1.0))
					want_speed = vmax * 0.35
				"flee":
					goal = pos + Vector3(-dir_t.x, -dir_t.y, up * 0.2) * 6.0
					want_speed = vmax
				"orbit":
					orbit_a += orbit_w * dt
					goal = tp + Vector3(cos(orbit_a), sin(orbit_a), up * 0.25 * sin(t * 0.8)) * orbit_r
					want_speed = vmax
				"evade":
					goal = tp - dir_t * (orbit_r + 1.5) + nrm * sin(t * 2.1 + s.sway_phase) * 1.4 + Vector3(0, 0, sin(t * 1.6) * 0.4)
					want_speed = vmax * 0.9
				"charge":
					goal = tp
					want_speed = vmax
					if dist < 0.7:
						goal = tp + dir_t * 3.0
				_:
					phase_t += dt
					if phase == "approach":
						goal = tp + nrm * pass_d * 1.0 + Vector3(0, 0, up * 0.3)
						want_speed = vmax * 0.85
						if (t >= t_p - 0.9 and dist < 6.0) or dist < 1.6 * pass_d + 0.8:
							phase = "strafe"
							phase_t = 0.0
					elif phase == "strafe":
						goal = tp + nrm * pass_d + dir_t * 0.6
						want_speed = vmax
						if dist < pass_d * 1.15 + 0.1 or phase_t > 2.2:
							phase = "break"
							phase_t = 0.0
							break_dir = side
					elif phase == "break":
						var h := deg_to_rad(heading + break_dir * 70.0)
						goal = pos + Vector3(cos(h), sin(h), up * 0.35) * 3.0
						want_speed = vmax
						if phase_t > 1.3 or dist > 4.5:
							phase = "regroup"
							phase_t = 0.0
					else:
						# Round again: back onto the core's own track (the next pass, if any, is the next round's run).
						var bp2 := s.pose_at(minf(t + 1.0, T_end))
						goal = Vector3(bp2.x, bp2.y, bp2.altitude)
						want_speed = vmax * 0.7
		# The run may stray from the core's own path only as far as there is time to fly out and back.
		if not arriving and mode != "hold":
			var bp := s.pose_at(t)
			var base := Vector3(bp.x, bp.y, bp.altitude)
			var dev := goal - base
			var maxd := 0.35 * vmax * minf(t, T_end - t) + 0.8
			if dev.length() > maxd:
				goal = base + dev.normalized() * maxd
		# Docking: inside three cells of home the last stretch is a controlled slide onto the core's cell (the core's ships have
		# lateral thrusters too), the bow swinging round to the core's facing at the class turn rate.
		var docking := arriving and rem0 < 3.0
		if docking:
			if not was_docking:
				dock_vel = Vector3(cos(deg_to_rad(heading)) * speed, sin(deg_to_rad(heading)) * speed, vz)
				was_docking = true
			var dv_want := end_pos - pos
			var rem_d := dv_want.length()
			var vmag := minf(vmax * 1.2, sqrt(2.0 * accel * maxf(rem_d - 0.02, 0.0)) * 0.9)
			dv_want = dv_want / maxf(rem_d, 1e-4) * vmag
			var dv := dv_want - dock_vel
			var lim := accel * 1.5 * dt
			if dv.length() > lim:
				dv = dv / dv.length() * lim
			dock_vel += dv
			if k > 0:
				pos += dock_vel * dt
			speed = Vector2(dock_vel.x, dock_vel.y).length()
			vz = dock_vel.z
			var want_dock := b1.facing
			if speed > 0.4 * vmax and rem_d > 0.8 and (T_end - t) > 2.0:
				want_dock = rad_to_deg(atan2(dock_vel.y, dock_vel.x))
			heading = wrap_degrees(heading + clampf(shortest_turn(heading, want_dock), -turn_rate * dt, turn_rate * dt))
		else:
			# Steer: the heading turns at the class rate; speed follows accel; sharp turns cost speed.
			var to_g := goal - pos
			var flat := Vector2(to_g.x, to_g.y)
			var want_h := rad_to_deg(atan2(to_g.y, to_g.x)) if flat.length() > 0.04 else heading
			if mode == "follow" and not arriving and (goal - pos).length() < 0.5:
				want_h = follow_face
			var dh := shortest_turn(heading, want_h)
			heading = wrap_degrees(heading + clampf(dh, -turn_rate * dt, turn_rate * dt))
			var left := shortest_turn(heading, want_h)
			want_speed *= 0.55 + 0.45 * maxf(0.0, cos(deg_to_rad(minf(absf(left), 120.0))))
			speed += clampf(want_speed - speed, -accel * 1.5 * dt, accel * dt)
			var hv := deg_to_rad(heading)
			var vz_t := clampf((goal.z - pos.z) * 0.9, -0.5, 0.5) * (0.3 + 0.7 * clampf(speed / maxf(vmax, 1e-3), 0.0, 1.0))
			vz += clampf(vz_t - vz, -1.2 * dt, 1.2 * dt)
			if k > 0:
				pos += Vector3(cos(hv) * speed, sin(hv) * speed, vz) * dt
		var yaw_rate := shortest_turn(prev_heading, heading) / dt if k > 0 else 0.0
		prev_heading = heading
		yaw_f += (yaw_rate - yaw_f) * minf(1.0, dt / 0.2)
		var pitch_t := clampf(rad_to_deg(atan2(vz, maxf(speed, 0.05))), -35.0, 35.0)
		pitch_f += (pitch_t - pitch_f) * minf(1.0, dt / 0.25)
		var o := k * 6
		path[o] = pos.x
		path[o + 1] = pos.y
		path[o + 2] = pos.z
		path[o + 3] = heading
		path[o + 4] = clampf(-yaw_f * 0.45, -65.0, 65.0)
		path[o + 5] = pitch_f
	# Hand back exactly to the core's end pose: what the braking left over is blended out over the last 1.4 s.
	var e_pos := end_pos - Vector3(path[(n - 1) * 6], path[(n - 1) * 6 + 1], path[(n - 1) * 6 + 2])
	var e_head := shortest_turn(path[(n - 1) * 6 + 3], b1.facing)
	var blend := 1.4
	for k in n:
		var t := float(k) * dt
		var w := smoothstep01((t - (T_end - blend)) / blend)
		if w <= 0.0:
			continue
		var o := k * 6
		path[o] += e_pos.x * w
		path[o + 1] += e_pos.y * w
		path[o + 2] += e_pos.z * w
		path[o + 3] = wrap_degrees(path[o + 3] + e_head * w)
		var calm := 1.0 - smoothstep01((t - (T_end - 0.7)) / 0.7)
		path[o + 4] *= calm
		path[o + 5] *= calm
	path[0] = b0.x
	path[1] = b0.y
	path[2] = b0.altitude
	path[3] = b0.facing
	s.path = path


static func _point_of(tl: BattleTimeline, s: ShipTrack, t: float) -> Vector3:
	var p := tl.flown(s, t)
	return Vector3(p.x, p.y, p.altitude)


func all_consumed() -> bool:
	for c in consumed:
		if c <= 0:
			return false
	return true


static func job_end(j: Dictionary) -> float:
	return j.start + j.duration


# ------------------------------------------------------------------------------------------------------------------
# Math helpers
# ------------------------------------------------------------------------------------------------------------------

static func wrap_degrees(a: float) -> float:
	a = fmod(a, 360.0)
	if a < 0.0:
		a += 360.0
	return a


## Signed shortest turn from a to b, in (-180, 180].
static func shortest_turn(a: float, b: float) -> float:
	var d := fmod(b - a, 360.0)
	if d > 180.0:
		d -= 360.0
	if d <= -180.0:
		d += 360.0
	return d


static func smoothstep01(u: float) -> float:
	u = clampf(u, 0.0, 1.0)
	return u * u * (3.0 - 2.0 * u)


## Progress along a flight for u in [0, 1]. accel 0: smoothstep. accel > 0: a cruise profile, speeding up evenly over the
## first `accel` share, cruising, and slowing over the last (starts and ends at rest, like smoothstep).
static func flight_ease(u: float, accel: float) -> float:
	if accel <= 0.0:
		return smoothstep01(u)
	u = clampf(u, 0.0, 1.0)
	var a := clampf(accel, 1e-3, 0.5)
	var v := 1.0 / (1.0 - a)
	if u < a:
		return 0.5 * v * u * u / a
	if u > 1.0 - a:
		var r := 1.0 - u
		return 1.0 - 0.5 * v * r * r / a
	return v * (u - 0.5 * a)


# ------------------------------------------------------------------------------------------------------------------
# 3D flight (presentation only)
# ------------------------------------------------------------------------------------------------------------------

const _U32 := 0xFFFFFFFF


## Deterministic value in [0, 1) for a ship and a salt (heights must not flicker between replays of a round). Same
## bits as BotfCombatTimeline.h unitHash (32-bit unsigned arithmetic).
static func unit_hash(a: int, b: int) -> float:
	var h := _mul32(a & _U32, 0x9E3779B1) ^ _mul32((b + 0x7F4A7C15) & _U32, 0x85EBCA77)
	h ^= h >> 15
	h = _mul32(h, 0x2C1B3C6D)
	h ^= h >> 12
	h = _mul32(h, 0x297A2D39)
	h ^= h >> 15
	return float(h & 0xFFFFFF) / float(0x1000000)


## x * k modulo 2^32 for 32-bit unsigned x and k, without overflowing a 64-bit int.
static func _mul32(x: int, k: int) -> int:
	return (x * (k & 0xFFFF) + (((x * (k >> 16)) & 0xFFFF) << 16)) & _U32


static func is_small_ship(length_meters: float, cfg: Config = null) -> bool:
	var limit := cfg.small_ship_meters if cfg != null else 260.0
	return length_meters > 0.0 and length_meters <= limit


## The height a ship holds between maneuvers: capital ships near the plane, small craft spread well above and below,
## each side offset so the two fleets read as layers.
static func home_height(fleet_id: int, length_meters: float, first_side: bool, cfg: Config = null) -> float:
	if cfg == null:
		cfg = Config.new()
	var spread := cfg.small_height_spread if is_small_ship(length_meters, cfg) else cfg.capital_height_spread
	return (unit_hash(fleet_id, 1) * 2.0 - 1.0) * spread + (cfg.side_height_offset if first_side else -cfg.side_height_offset)


## Presentation-only spacing. Ships are drawn at true scale, so a capital ship spans more than a grid cell and hulls
## that share or neighbour a cell would pass through each other. bodies: Array of {x, y, alt, radius, active} (cells).
## Returns, per body, the smallest shift Vector3(plane x, plane y, altitude) in cells that keeps every pair of active
## hulls at least (ra + rb) apart, preferring height over sideways moves (vertical_bias) so the grid positions the
## rules use stay readable from above. Deterministic: pairs resolve in index order, coincident hulls split by index.
## The scene eases toward the result; rules positions are untouched.
static func spacing_offsets(bodies: Array, iterations := 10, vertical_bias := 2.5, max_shift := 3.0) -> Array[Vector3]:
	var n := bodies.size()
	var off: Array[Vector3] = []
	off.resize(n)
	off.fill(Vector3.ZERO)
	# Unpack the live bodies once (flat arrays, original indices kept): the pair loop below runs iterations x n^2 / 2
	# times, and 24 v 24 ships spent ~10 ms a frame on dictionary lookups when this read the dictionaries per pair.
	var ids := PackedInt32Array()
	var px := PackedFloat64Array()
	var py := PackedFloat64Array()
	var pa := PackedFloat64Array()
	var pr := PackedFloat64Array()
	for i in n:
		var bi: Dictionary = bodies[i]
		if not bi.get("active", true) or float(bi.radius) <= 0.0:
			continue
		ids.append(i)
		px.append(float(bi.x))
		py.append(float(bi.y))
		pa.append(float(bi.alt))
		pr.append(float(bi.radius))
	var m := ids.size()
	var ox := PackedFloat64Array()
	var oy := PackedFloat64Array()
	var oa := PackedFloat64Array()
	ox.resize(m)
	oy.resize(m)
	oa.resize(m)
	for pass_i in iterations:
		var moved := false
		for a in m:
			for b in range(a + 1, m):
				var dx: float = (px[b] + ox[b]) - (px[a] + ox[a])
				var dy: float = (py[b] + oy[b]) - (py[a] + oy[a])
				var da: float = (pa[b] + oa[b]) - (pa[a] + oa[a])
				var need: float = pr[a] + pr[b]
				var d2 := dx * dx + dy * dy + da * da
				if d2 >= need * need:
					continue
				var dist := sqrt(d2)
				if dist >= need:
					continue
				# Push direction: the current separation with height weighted up and tilted at least ~27 degrees out of
				# the plane (level neighbours stack rather than shove sideways); straight up/down when coincident.
				var flat := sqrt(dx * dx + dy * dy)
				var up := 1.0 if da > 1e-9 else (-1.0 if da < -1e-9 else (1.0 if (ids[a] + ids[b]) % 2 == 0 else -1.0))
				var u := Vector3(dx, dy, up * maxf(absf(da) * vertical_bias, 0.5 * flat))
				if u.length() < 1e-9:
					u = Vector3(0.0, 0.0, up)
				u = u.normalized()
				# The push along u must close the gap along the true separation; split by the other hull's size so a
				# cruiser moves less than the escort beside it.
				var along := maxf(0.3, (u.x * dx + u.y * dy + u.z * da) / maxf(dist, 1e-9))
				var gap := (need - dist) / (1.0 if dist < 1e-9 else along)
				var ka := gap * (pr[b] / need)
				var kb := gap * (pr[a] / need)
				ox[a] -= u.x * ka
				oy[a] -= u.y * ka
				oa[a] -= u.z * ka
				ox[b] += u.x * kb
				oy[b] += u.y * kb
				oa[b] += u.z * kb
				moved = true
		if not moved:
			break
	for k in m:
		var v := Vector3(ox[k], oy[k], oa[k])
		if v.length() > max_shift:
			v *= max_shift / v.length()
		off[ids[k]] = v
	return off


## Station-keeping between maneuvers (presentation only). Simon, 8 Nov 2026: "the ships bob up and down like boats".
## A ship holding position keeps a steady heading and altitude: no roll, no nodding, only a slow, barely visible
## altitude drift (at most IDLE_DRIFT_CELLS, about a third of a world unit, on a 9-11 s period), each on its own phase.
## Banking happens only in turns (Segment.bank_peak). The drift fades in and out over fade_seconds at every maneuver
## edge and at the round's start and end, so poses at those instants are untouched and rounds join seamlessly.
## Returns Vector3(altitude cells, bank degrees, pitch degrees), added on top of pose_of; bank and pitch are always 0.
const IDLE_DRIFT_CELLS := 0.012
static func station_keeping(s: ShipTrack, t: float, round_secs: float, fade_seconds := 0.6) -> Vector3:
	if t <= 0.0 or t >= round_secs:
		return Vector3.ZERO
	var edge := minf(t, round_secs - t)
	for seg in s.segments:
		if t > seg.t0 and t < seg.t1:
			return Vector3.ZERO # flying a maneuver
		edge = minf(edge, minf(absf(t - seg.t0), absf(t - seg.t1)))
	var env := smoothstep01(edge / maxf(1e-6, fade_seconds))
	var phase := unit_hash(s.fleet_id, 7) * TAU
	var period := 9.0 + 2.0 * unit_hash(s.fleet_id, 8)
	return Vector3(env * IDLE_DRIFT_CELLS * sin(TAU * t / period + phase), 0.0, 0.0)


# ------------------------------------------------------------------------------------------------------------------
# Colours, keys, hardpoints
# ------------------------------------------------------------------------------------------------------------------

## Beam / bolt / torpedo glow per weapon type, then per owner faction for generic types.
static func weapon_color(weapon: String, owner_faction: int) -> Color:
	match weapon:
		"PhaserBank", "PhaserArray", "PhaseCannon", "PulsePhaser":
			return Color(1.0, 0.75, 0.15) if owner_faction == 5 else Color(1.0, 0.45, 0.05)
		"Disruptor", "DisruptorCannon":
			return Color(1.0, 0.8, 0.15) if owner_faction == 4 else Color(0.15, 1.0, 0.25)
		"PlasmaCannon": return Color(0.2, 1.0, 0.45)
		"PolaronBeam": return Color(0.45, 0.35, 1.0)
		"CuttingBeam": return Color(0.3, 1.0, 0.3)
		"EnergyDampener": return Color(0.3, 0.9, 1.0)
		"TholianWeb": return Color(1.0, 0.35, 0.1)
		"BioEnergy": return Color(0.8, 1.0, 0.2)
		"TemporalWeapon": return Color(0.8, 0.3, 1.0)
		"PhotonTorpedo", "PhotonicTorpedo", "Torpedo":
			# Each empire its own torpedo colour (coordinator, 8 Nov): Federation orange-red, Klingon red (M1 weaponfx: the
			# show's Klingon torpedoes are red, and lime read the same as their green disruptor bolts).
			match owner_faction:
				2: return Color(1.0, 0.3, 0.12)
				3: return Color(0.2, 1.0, 0.5)
				4: return Color(1.0, 0.8, 0.1)
				5: return Color(0.6, 0.4, 1.0)
			return Color(1.0, 0.62, 0.26) # battlechoreo compare 02: orange, not red; footage WotW 4891 s: an orange star, near-white core; (1.0, 0.25, 0.08) read red-pink
		"QuantumTorpedo": return Color(0.4, 0.65, 1.0)
		"PlasmaTorpedo": return Color(0.25, 1.0, 0.4)
		"SpatialTorpedo": return Color(1.0, 0.55, 0.2)
		"TricobaltDevice": return Color(1.0, 0.95, 0.85)
	match owner_faction:
		2: return Color(0.1, 1.0, 0.2)
		3: return Color(0.05, 0.95, 0.35)
		4: return Color(1.0, 0.85, 0.1)
		5: return Color(0.1, 0.75, 1.0)
	return Color(1.0, 0.5, 0.2)


static func faction_prefix(owner_faction: int) -> String:
	return FACTION_PREFIX.get(owner_faction, "")


static func family_index(family) -> int:
	if family is String:
		return 2 if family == "torpedo" else (1 if family == "pulse" else 0)
	return clampi(int(family), 0, 2)


static func vfx_weapon_key(weapon: String, family: int) -> String:
	if WEAPON_TYPES.has(weapon):
		return "vfx." + weapon
	return "vfx.generic.torpedo" if family == 2 else ("vfx.generic.pulse" if family == 1 else "vfx.generic.beam")


## Sound keys for a shot: weapon.<Type>.<fac>, weapon.<Type>, weapon.generic.<beam|torpedo>.
static func weapon_sound_keys(weapon: String, family: int, owner_faction: int) -> PackedStringArray:
	var keys := PackedStringArray()
	if WEAPON_TYPES.has(weapon):
		var base := "weapon." + weapon
		var fac := faction_prefix(owner_faction)
		if fac != "":
			keys.append(base + "." + fac)
		keys.append(base)
		keys.append("weapon.generic.torpedo" if TORPEDO_TYPES.has(weapon) else "weapon.generic.beam")
	if keys.is_empty():
		keys.append("weapon.generic.torpedo" if family == 2 else "weapon.generic.beam")
	return keys


## Sound keys for a non-weapon cue (BotfAudioCues.h cueFor).
static func cue_keys(cue: String, owner_faction: int) -> PackedStringArray:
	var fac := faction_prefix(owner_faction)
	match cue:
		"ShieldHit": return PackedStringArray(["sfx.shield.hit"])
		"HullHit": return PackedStringArray(["sfx.explosion.small"])
		"ShipDestroyed": return PackedStringArray(["sfx.explosion.large"])
		"Cloak": return PackedStringArray(["sfx.cloak." + (fac if fac != "" else "rom"), "sfx.cloak.rom"])
		"Decloak": return PackedStringArray(["sfx.decloak." + (fac if fac != "" else "rom"), "sfx.decloak.rom"])
		"Retreat": return PackedStringArray(["sfx.warp.out"])
	return PackedStringArray(["sfx.ui.beep"])


const CUE_PRIORITY := {"ShieldHit": 2, "HullHit": 2, "ShipDestroyed": 5, "Cloak": 4, "Decloak": 4, "Retreat": 3}


## Voice keys for a bark: vo.combat.<detail>.<fac>, then vo.combat.<detail head>.<fac>.
static func bark_keys(detail: String, owner_faction: int) -> PackedStringArray:
	var fac := faction_prefix(owner_faction)
	var keys := PackedStringArray()
	if fac == "":
		return keys
	keys.append("vo.combat." + detail + "." + fac)
	var dot := detail.find(".")
	if dot >= 0:
		keys.append("vo.combat." + detail.substr(0, dot) + "." + fac)
	return keys


## Hardpoint as a fraction of the mesh's half-extents (x forward, y right, z up) when the mesh has no socket:
## torpedo tubes at the bow, beam emitters spread port/starboard along the dorsal hull.
static func fallback_hardpoint(family: int, mount_index: int, emitter_index: int) -> Vector3:
	var m := maxi(0, mount_index)
	var e := maxi(0, emitter_index)
	var side := 1.0 if (m + e) % 2 == 0 else -1.0
	if family == 2:
		return Vector3(0.9, side * 0.12, -0.08)
	var along := 0.45 - 0.25 * float((m / 2 + e) % 4)
	return Vector3(along, side * 0.45, 0.05 if family == 1 else 0.2)


## Position along a torpedo path (plane cells + lift). u in [0,1]; a miss continues past the target by overshoot.
static func torpedo_point(s: Vector2, t: Vector2, arc_cells: float, overshoot_cells: float, u: float) -> Vector3:
	var d := t - s
	var length := d.length()
	if length > 1e-9 and overshoot_cells > 0.0:
		t = s + d * ((length + overshoot_cells) / length)
		d = t - s
	var n := Vector2(-d.y, d.x) / d.length() if length > 1e-9 else Vector2.ZERO
	var bow := 4.0 * u * (1.0 - u)
	var p := s + d * u + n * arc_cells * bow
	return Vector3(p.x, p.y, absf(arc_cells) * bow)


# ------------------------------------------------------------------------------------------------------------------
# Builder
# ------------------------------------------------------------------------------------------------------------------

static func _num(e: Dictionary, key: String, fallback = 0):
	var v = e.get(key, fallback)
	return fallback if v == null else v


## events: the round's `events` array. info: Array of {fleetId, ownerId, shields, maxShields, startsCloaked, x, y,
## facing, lengthMeters} for the ships that are in the round (what the stream does not carry). lengthMeters is the
## canon length of the shown class (0 = unknown): it picks the flight style and wings.
static func build(events: Array, info: Array, cfg: Config = null) -> BattleTimeline:
	if cfg == null:
		cfg = Config.new()
	var tl := BattleTimeline.new()
	tl.round_seconds = maxf(0.5, cfg.round_seconds)
	tl.tick_seconds = tl.round_seconds / TICKS_PER_ROUND
	var tk := tl.tick_seconds
	tl.consumed.resize(events.size())
	tl.consumed.fill(0)
	tl.cinematic = cfg.cinematic
	tl.window_starts = _windows(events, cfg, tl.round_seconds)
	tl.formation_until = cfg.flight_to * tl.round_seconds if cfg.cinematic else -1.0
	var W := tl.window_starts
	## Round time of a core tick: its window's opening.
	var tick_time := func(tick: int) -> float:
		return W[clampi(tick, 0, TICKS_PER_ROUND)]
	## Round time of a movement tick: the tick clock, or the cinematic flight spread over the round.
	var move_time := func(tick: int) -> float:
		if not cfg.cinematic:
			return tick * tk
		return clampf(float(tick) / float(maxi(1, cfg.move_end_tick)), 0.0, 1.0) * cfg.flight_to * tl.round_seconds

	var info_by_id := {}
	for s in info:
		info_by_id[int(s.fleetId)] = s
	var owner_of := func(fid: int) -> int:
		return int(info_by_id[fid].get("ownerId", 0)) if info_by_id.has(fid) else 0
	var track_for := func(fid: int) -> ShipTrack:
		var existing := tl.track(fid)
		if existing != null:
			return existing
		var t := ShipTrack.new()
		t.fleet_id = fid
		t.owner_id = owner_of.call(fid)
		t.cloak_seconds = cfg.cloak_ticks * tk
		tl.ships.append(t)
		return t

	# Tracks: every known ship, then any actor only the events mention. Initial pose = first pose-carrying event.
	for s in info:
		var t: ShipTrack = track_for.call(int(s.fleetId))
		t.owner_id = int(s.get("ownerId", 0))
		t.starts_cloaked = bool(s.get("startsCloaked", false))
		t.initial = Pose.new(float(s.get("x", 0)), float(s.get("y", 0)), wrap_degrees(float(s.get("facing", 0))))
	var posed := {}
	for i in events.size():
		var e: Dictionary = events[i]
		var fid := int(_num(e, "fleetId"))
		if fid == 0 or not POSE_KINDS.has(e.kind):
			continue
		var t: ShipTrack = track_for.call(fid)
		if not posed.has(fid):
			posed[fid] = true
			t.initial = Pose.new(float(e.x), float(e.y), wrap_degrees(float(e.facing)))

	# Flight segments: ManeuverStart .. matching ManeuverEnd.
	for i in events.size():
		var e: Dictionary = events[i]
		if e.kind != "ManeuverStart" and e.kind != "ManeuverEnd":
			continue
		var fid := int(e.fleetId)
		var t: ShipTrack = track_for.call(fid)
		if e.kind == "ManeuverStart":
			t.initial = Pose.new(float(e.x), float(e.y), wrap_degrees(float(e.facing)))
			tl.consumed[i] += 1
			continue
		var from: Pose = t.initial if t.segments.is_empty() else t.segments.back().to
		var start_tick := 0
		var j := i - 1
		while j >= 0:
			var p: Dictionary = events[j]
			if int(_num(p, "fleetId")) == fid and p.kind == "ManeuverStart":
				from = Pose.new(float(p.x), float(p.y), wrap_degrees(float(p.facing)))
				start_tick = int(p.tick)
				break
			j -= 1
		var s := Segment.new()
		s.from = from.copy()
		s.to = Pose.new(float(e.x), float(e.y), wrap_degrees(float(e.facing)))
		s.t0 = move_time.call(start_tick)
		s.t1 = move_time.call(maxi(int(e.tick), start_tick + 1))
		if not t.segments.is_empty():
			s.t0 = maxf(s.t0, t.segments.back().t1)
		if s.t1 <= s.t0:
			s.t1 = s.t0 + tk
		t.cruise = cfg.cruise_accel if cfg.cinematic else 0.0
		var slide := int(_num(e, "value"))
		s.slide_cells = slide
		s.maneuver = str(_num(e, "maneuver", ""))
		t.maneuver_last = s.maneuver
		var d := Vector2(s.to.x - s.from.x, s.to.y - s.from.y)
		var length := d.length()
		var fa := deg_to_rad(s.from.facing)
		var fb := deg_to_rad(s.to.facing)
		var along0 := d.x * cos(fa) + d.y * sin(fa)
		var along1 := d.x * cos(fb) + d.y * sin(fb)
		if length < 1e-9:
			s.tan0 = Vector2.ZERO; s.tan1 = Vector2.ZERO
		elif slide > 0 or along0 <= 0.25 * length:
			# Sideways slide or reverse thrust: keep the bow, translate along the displacement.
			s.tan0 = d; s.tan1 = d
		else:
			s.tan0 = Vector2(cos(fa), sin(fa)) * length
			s.tan1 = Vector2(cos(fb), sin(fb)) * length if along1 > 0.0 else d
		var bank := -shortest_turn(s.from.facing, s.to.facing) * cfg.bank_per_turn_degree
		if slide > 0 and length > 1e-9:
			var cross := cos(fa) * d.y - sin(fa) * d.x
			bank += (-1.0 if cross > 0 else 1.0) * cfg.bank_per_slide_cell * slide
		# Small craft roll harder (and may exceed the capital-ship limit); capital ships lumber.
		var max_bank := cfg.max_bank_degrees
		var meters := float(info_by_id[fid].get("lengthMeters", 0.0)) if info_by_id.has(fid) else 0.0
		if meters > 0.0:
			var small := is_small_ship(meters, cfg)
			bank *= cfg.small_bank_scale if small else cfg.capital_bank_scale
			if small:
				max_bank *= cfg.small_bank_scale
		s.bank_peak = clampf(bank, -max_bank, max_bank)
		t.segments.append(s)
		tl.consumed[i] += 1

	# Heights (3D flight): each ship holds a home height and climbs or dives around it with every maneuver, pitching
	# its nose with the climb. A ship sitting still (no horizontal run) rises or sinks level on thrusters.
	var first_owner := int(info[0].get("ownerId", 0)) if not info.is_empty() else 0
	for t in tl.ships:
		t.length_meters = float(info_by_id[t.fleet_id].get("lengthMeters", 0.0)) if info_by_id.has(t.fleet_id) else 0.0
		t.small_craft = is_small_ship(t.length_meters, cfg)
		var home := home_height(t.fleet_id, t.length_meters, t.owner_id == first_owner, cfg)
		# Rules 4: the core's deck layer decides who fires down on whose dorsal shield, so the ship flies on it.
		if info_by_id.has(t.fleet_id) and info_by_id[t.fleet_id].get("elevation") != null:
			home = float(info_by_id[t.fleet_id].elevation) * cfg.deck_layer_cells + (unit_hash(t.fleet_id, 1) - 0.5) * 0.3
		var climb := cfg.small_climb_cells if t.small_craft else cfg.capital_climb_cells
		var h := home
		t.initial.altitude = h
		for k in t.segments.size():
			var seg: Segment = t.segments[k]
			seg.from.altitude = h
			h = home + (unit_hash(t.fleet_id, 101 + k) * 2.0 - 1.0) * climb
			seg.to.altitude = h
			var run := Vector2(seg.to.x - seg.from.x, seg.to.y - seg.from.y).length()
			seg.pitch_peak = clampf(rad_to_deg(atan2(seg.to.altitude - seg.from.altitude, run)) * 1.5,
				-cfg.max_pitch_degrees, cfg.max_pitch_degrees) if run > 0.3 else 0.0

	# Wings: escort-size ships of one side that start close together fly finger-four on the lowest-id ship among them
	# (wingman aft-port, element lead aft-starboard, its wingman further aft-starboard).
	var stations := [[1.0, 1.0, 0.25], [1.0, -1.0, -0.25], [2.0, -2.0, 0.0]] # aft, port, up
	var pool: Array[ShipTrack] = []
	for t in tl.ships:
		if t.small_craft:
			pool.append(t)
	pool.sort_custom(func(a, b): return a.fleet_id < b.fleet_id)
	var used := []
	used.resize(pool.size())
	used.fill(false)
	for i in pool.size():
		if used[i]:
			continue
		used[i] = true
		var members := 1
		var j := i + 1
		while j < pool.size() and members < maxi(1, cfg.wing_size):
			var mate := pool[j]
			if not used[j] and mate.owner_id == pool[i].owner_id \
					and Vector2(mate.initial.x - pool[i].initial.x, mate.initial.y - pool[i].initial.y).length() <= cfg.wing_radius_cells:
				used[j] = true
				var st: Array = stations[members - 1]
				mate.wing_leader = pool[i].fleet_id
				mate.wing_back = st[0] * cfg.wing_spacing_cells
				mate.wing_side = st[1] * cfg.wing_spacing_cells
				mate.wing_up = st[2] * cfg.wing_spacing_cells
				members += 1
			j += 1

	# Weapon, impact and state jobs.
	var impacts: Array = []
	var kill_times := {}
	var ram_times := {}
	var ctx := {"torpedo_barked": false}
	# Cinematic: each Fire / Ram event's launch and impact time (the tick clock derives them from the ticks below).
	var sched: Dictionary = {}
	if cfg.cinematic and cfg.flight != null:
		# M4 flight: the sim flies the round and launches each shot when its mount bears; the order pass keeps the core's
		# order on every target from there.
		var launches: Dictionary = cfg.flight.plan_round(tl, events, info_by_id, cfg, cfg.encounter)
		tl._flown_cache.clear()
		sched = _schedule(tl, events, info_by_id, cfg, launches)
	else:
		sched = _schedule(tl, events, info_by_id, cfg) if cfg.cinematic else {}
		if cfg.cinematic and cfg.choreo:
			_choreograph(tl, events, sched, info_by_id, cfg)
			tl._flown_cache.clear()
	var salvo_tubes := _torpedo_salvos(events, sched, info_by_id, cfg) if cfg.cinematic else {}
	var add_job := func(j: Dictionary, ev: int) -> void:
		j.event_index = ev
		tl.jobs.append(j)
		tl.consumed[ev] += 1
	var add_sound := func(time: float, cue: String, keys: PackedStringArray, priority: int, ev: int) -> void:
		tl.sounds.append({"time": time, "cue": cue, "keys": keys, "priority": priority, "event_index": ev})
	var add_impacts := func(e: Dictionary, time: float, ev: int, family: int) -> void:
		var shooter := int(_num(e, "fleetId"))
		var target := int(_num(e, "targetFleetId"))
		var shooter_owner: int = owner_of.call(shooter)
		var shield_dmg := int(_num(e, "shieldDamage"))
		var hull_dmg := int(_num(e, "hullDamage"))
		var weapon := str(_num(e, "weapon", ""))
		# Tactical rules 4: the shield facing struck and what the hull hit did to a subsystem.
		var wego := {"facing": str(_num(e, "shieldFacing", "")), "subsystem": str(_num(e, "subsystem", "none")),
			"subsystem_damage": int(_num(e, "subsystemDamage")), "subsystem_destroyed": bool(_num(e, "subsystemDestroyed", false)),
			"breach": int(_num(e, "breachDamage")), "aimed": bool(_num(e, "aimed", false))}
		if shield_dmg > 0 or (bool(_num(e, "hit", false)) and hull_dmg <= 0):
			var j := new_job(Job.SHIELD, time, minf(cfg.impact_ticks * tk, 0.45)) # M4: a brief local flare, not a bubble
			j.fleet_id = target; j.target_fleet_id = target; j.source_fleet_id = shooter
			j.weapon = weapon; j.family = family; j.shield_damage = shield_dmg
			j.color = weapon_color(weapon, shooter_owner); j.vfx_key = "vfx.impact.shield"
			j.merge(wego, true)
			add_job.call(j, ev)
			add_sound.call(time, "ShieldHit", cue_keys("ShieldHit", 0), CUE_PRIORITY.ShieldHit, ev)
		if hull_dmg > 0:
			var j := new_job(Job.HULL, time, cfg.impact_ticks * 1.5 * tk)
			j.fleet_id = target; j.target_fleet_id = target; j.source_fleet_id = shooter
			j.weapon = weapon; j.family = family; j.hull_damage = hull_dmg; j.shield_damage = shield_dmg
			j.destroys_target = bool(_num(e, "destroyed", false))
			j.color = weapon_color(weapon, shooter_owner); j.vfx_key = "vfx.impact.hull"
			j.merge(wego, true)
			add_job.call(j, ev)
			add_sound.call(time, "HullHit", cue_keys("HullHit", 0), CUE_PRIORITY.HullHit, ev)
		impacts.append({"time": time, "target": target, "source": shooter, "shield_damage": shield_dmg,
			"destroyed": bool(_num(e, "destroyed", false)), "shooter_owner": shooter_owner})
		tl.impact_log.append({"time": time, "target": target, "source": shooter, "shield_damage": shield_dmg,
			"hull_damage": hull_dmg, "event_index": ev, "key": impact_key(e, ev), "destroyed": bool(_num(e, "destroyed", false))})

	for i in events.size():
		var e: Dictionary = events[i]
		var fid := int(_num(e, "fleetId"))
		var owner: int = owner_of.call(fid)
		var tick_i := clampi(int(e.tick), 0, TICKS_PER_ROUND - 1)
		var t0: float = tick_time.call(int(e.tick))
		var tick_len: float = W[tick_i + 1] - W[tick_i]
		var sc: Array = sched.get(i, [])
		var cine := not sc.is_empty()
		match str(e.kind):
			"Fire":
				var family := family_index(_num(e, "family", 0))
				var n := maxi(1, int(_num(e, "projectiles", 1)))
				var weapon := str(_num(e, "weapon", ""))
				var hit := bool(_num(e, "hit", false))
				var intercepted := bool(_num(e, "intercepted", false))
				var keys := weapon_sound_keys(weapon, family, owner)
				var base := new_job(Job.BEAM, t0, 0.0)
				base.fleet_id = fid; base.target_fleet_id = int(_num(e, "targetFleetId"))
				base.mount_index = int(_num(e, "mountIndex", -1)); base.weapon = weapon; base.family = family
				base.count = n; base.color = weapon_color(weapon, owner); base.vfx_key = vfx_weapon_key(weapon, family)
				base.outcome = Outcome.INTERCEPTED if intercepted else (Outcome.HIT if hit else Outcome.MISS)
				base.shield_damage = int(_num(e, "shieldDamage")); base.hull_damage = int(_num(e, "hullDamage"))
				base.destroys_target = bool(_num(e, "destroyed", false))
				var launch: float = sc[0] if cine else t0
				var impact_time := t0
				# Cinematic: a disruptor (beam family in the core) is drawn as a burst of bolts, as in the shows.
				var shown := 1 if cine and family == 0 and shows_as_bolts(weapon, owner, float(info_by_id[fid].get("lengthMeters", -1.0)) if info_by_id.has(fid) else -1.0) else family
				if shown == 0:
					for k in n:
						var j := base.duplicate()
						j.kind = Job.BEAM; j.index = k
						if cine:
							j.start = launch + k * 0.05
							# Proposal 4: some beams cut out and re-fire a shorter second burst (drawn only; the damage
							# lands once, with the first burst).
							var refire := unit_hash(i * 7 + k, 21) < cfg.beam_refire_share
							j.duration = cfg.beam_first_seconds if refire else beam_hold(weapon, owner, unit_hash(i, 22))
							if refire:
								var j2 := j.duplicate()
								j2.start = j.start + cfg.beam_first_seconds + cfg.beam_refire_gap
								j2.duration = cfg.beam_refire_seconds
								j2.refire = true
								if not hit:
									j2.overshoot_cells = cfg.miss_overshoot_cells
								add_job.call(j2, i)
						else:
							j.start = t0 + k * cfg.beam_emitter_stagger_ticks * tk
							j.duration = maxf(0.1, cfg.beam_ticks - k * cfg.beam_emitter_stagger_ticks) * tk
						if not hit:
							j.overshoot_cells = cfg.miss_overshoot_cells
						add_job.call(j, i)
					add_sound.call(launch, "BeamFire", keys, 3, i)
					impact_time = float(sc[1]) if cine else t0 + 0.25 * tk
				elif shown == 1:
					var bolts := n if family == 1 or not cine else clampi(maxi(2, n), 2, 4)
					if cine:
						# Proposal 3 (draw half): one core shot drawn as 3-4 pairs from both cannons, 0.125 s apart.
						var pairs := cfg.burst_pairs_min + int(unit_hash(i, 24) * float(cfg.burst_pairs_max - cfg.burst_pairs_min + 1))
						bolts = maxi(n, 2 * mini(pairs, cfg.burst_pairs_max))
					var stagger := minf(cfg.pulse_stagger_ticks, 0.9 / n)
					for k in bolts:
						var j := base.duplicate()
						j.kind = Job.PULSE; j.index = k
						var stray := false
						if cine:
							j.count = bolts
							j.start = launch + (k / 2) * cfg.burst_pair_gap + (k % 2) * 0.02
							j.duration = cfg.bolt_flight_seconds
							# A hit lands the first pair; a few later bolts go wide (drawn only, damage lands once).
							stray = hit and k >= 2 and unit_hash(i * 5 + k, 23) < 0.25
						else:
							j.start = t0 + k * stagger * tk
							j.duration = cfg.pulse_flight_ticks * tk
						if not hit or stray:
							j.overshoot_cells = cfg.miss_overshoot_cells
							if stray:
								j.outcome = Outcome.MISS
						add_job.call(j, i)
						add_sound.call(j.start, "BeamFire", keys, 3, i)
						impact_time = j.start + j.duration
					if cine:
						impact_time = float(sc[1]) # the first bolt lands with the damage; the rest follow it in
				else:
					var arrive := maxf(int(_num(e, "impactTick", e.tick)) * tk, t0 + cfg.min_torpedo_flight_ticks * tk)
					var flight := maxf(0.2 * tk, arrive - t0)
					var stagger_s := cfg.torpedo_stagger_ticks * tk
					if cine:
						# Every tube of the spread flies the same time; the last lands at the scheduled impact.
						stagger_s = torpedo_stagger(fid, cfg)
					# Torpedo spreads (coordinator, 10 Nov; drawn only): one core shot shows as a spread sized by the
					# launcher's class (escort 1-2, cruiser 2-3, capital 4-5), 0.12 s apart; the damage lands with the last.
					var tubes := n
					if cine:
						tubes = spread_tubes(i, n, float(info_by_id[fid].get("lengthMeters", 0.0)) if info_by_id.has(fid) else 0.0, cfg)
						tubes = int(salvo_tubes.get(i, tubes)) # M4 flight stage 3: one salvo per ship per round (0 = folded in)
						flight = maxf(0.3, float(sc[1]) - launch - (tubes - 1) * stagger_s)
					for k in tubes:
						var j := base.duplicate()
						j.kind = Job.TORPEDO; j.index = k; j.count = tubes
						j.start = launch + k * stagger_s
						j.arc_cells = (1.0 if k % 2 == 0 else -1.0) * cfg.torpedo_arc_cells * (1.0 + 0.3 * (k / 2))
						if intercepted:
							j.end_fraction = cfg.intercept_fraction; j.duration = flight * cfg.intercept_fraction
						elif not hit:
							j.overshoot_cells = cfg.miss_overshoot_cells; j.duration = flight * 1.35
						else:
							j.duration = flight
						add_job.call(j, i)
						impact_time = maxf(impact_time, j.start + flight)
						# One launch sound per tube, on its own stagger: the salvo ripples (R3.5).
						add_sound.call(j.start, "TorpedoLaunch", keys, 3, i)
					if tubes == 0:
						# Folded into the ship's salvo: no torpedo of its own; its damage lands at its scheduled impact.
						impact_time = float(sc[1])
						tl.consumed[i] += 1
					if cfg.player_empire > 0 and owner == cfg.player_empire and not ctx.torpedo_barked:
						tl.barks.append({"time": launch, "detail": "torpedo", "priority": 3, "fleet_id": fid})
						ctx.torpedo_barked = true
					if intercepted:
						var j := base.duplicate()
						j.kind = Job.INTERCEPT
						j.start = launch + flight * cfg.intercept_fraction; j.duration = cfg.impact_ticks * tk
						j.launch = launch # the intercepted torpedo's path starts where it left the tube
						j.vfx_key = "vfx.intercept"
						add_job.call(j, i)
						add_sound.call(j.start, "HullHit", cue_keys("HullHit", 0), CUE_PRIORITY.HullHit, i)
				if hit and not intercepted:
					add_impacts.call(e, impact_time, i, family)
					if base.destroys_target:
						kill_times[base.target_fleet_id] = impact_time
			"Ram":
				var impact_at: float = float(sc[1]) if cine else float(maxi(int(_num(e, "impactTick", e.tick)), int(e.tick)) * tk)
				var start: float = maxf(0.0, impact_at - 0.8) if cine else (t0 - 0.5 * tk if t0 - 0.5 * tk > 0 else t0)
				var j := new_job(Job.RAM, start, (impact_at + 0.4 - start) if cine else (impact_at + 0.5 * tk - start))
				j.fleet_id = fid; j.target_fleet_id = int(_num(e, "targetFleetId"))
				j.shield_damage = int(_num(e, "shieldDamage")); j.hull_damage = int(_num(e, "hullDamage"))
				j.destroys_target = bool(_num(e, "destroyed", false)); j.color = Color(1.0, 0.6, 0.2)
				add_job.call(j, i)
				var hit_event := e.duplicate()
				hit_event.hit = true
				add_impacts.call(hit_event, impact_at, i, 0)
				if j.destroys_target:
					kill_times[j.target_fleet_id] = impact_at
				ram_times[fid] = impact_at # the recoil may destroy the rammer at the same moment
			"Cloak":
				var j := new_job(Job.CLOAK, t0, cfg.cloak_ticks * tk)
				j.fleet_id = fid; j.color = Color(0.55, 0.8, 1.0); j.vfx_key = "vfx.cloak"
				add_job.call(j, i)
				var t: ShipTrack = track_for.call(fid)
				if not t.starts_cloaked:
					t.cloak_at = t0
				add_sound.call(t0, "Cloak", cue_keys("Cloak", owner), CUE_PRIORITY.Cloak, i)
			"Decloak":
				var j := new_job(Job.DECLOAK, t0, cfg.cloak_ticks * tk)
				j.fleet_id = fid; j.color = Color(0.55, 0.8, 1.0); j.vfx_key = "vfx.decloak"
				add_job.call(j, i)
				track_for.call(fid).decloak_at = t0
				add_sound.call(t0, "Decloak", cue_keys("Decloak", owner), CUE_PRIORITY.Decloak, i)
			"Destroyed":
				var when := t0 + 0.3 * (tick_len if cfg.cinematic else tk)
				if kill_times.has(fid):
					when = float(kill_times[fid]) if cfg.cinematic else maxf(t0, kill_times[fid])
				elif cfg.cinematic and ram_times.has(fid):
					when = float(ram_times[fid])
				var j := new_job(Job.EXPLOSION, when, cfg.explosion_ticks * tk)
				j.fleet_id = fid; j.target_fleet_id = fid; j.color = Color(1.0, 0.7, 0.3); j.vfx_key = "vfx.explosion"
				var t: ShipTrack = track_for.call(fid)
				var hide: float = when + 0.35 * j.duration
				if cfg.cinematic and cfg.choreo:
					# Proposal 5: the killing blow cripples the ship; it drifts, vents and tumbles, then breaks up.
					j.breakup = cfg.death_seconds
					j.duration = maxf(j.duration, cfg.death_seconds + 1.8)
					hide = when + cfg.death_seconds + 0.15
					t.kill_at = when
				add_job.call(j, i)
				t.hide_at = minf(t.hide_at, hide) if t.hide_at >= 0 else hide
				add_sound.call(when, "ShipDestroyed", cue_keys("ShipDestroyed", 0), CUE_PRIORITY.ShipDestroyed, i)
			"Retreated":
				var j := new_job(Job.WARP, t0, cfg.warp_ticks * tk)
				j.fleet_id = fid; j.color = Color(0.7, 0.85, 1.0); j.vfx_key = "vfx.warp.out"
				add_job.call(j, i)
				var t: ShipTrack = track_for.call(fid)
				t.hide_at = minf(t.hide_at, job_end(j)) if t.hide_at >= 0 else job_end(j)
				add_sound.call(t0, "Retreat", cue_keys("Retreat", 0), CUE_PRIORITY.Retreat, i)
			"Disengage":
				var span: float = maxi(1, cfg.move_end_tick) * tk
				if cfg.cinematic:
					span = maxf(tk, float(move_time.call(cfg.move_end_tick)) - t0)
				var j := new_job(Job.DISENGAGE, t0, span)
				j.fleet_id = fid; j.color = Color(0.7, 0.85, 1.0)
				add_job.call(j, i)
				# The ship leaves the fight: its warp-out sounds as it goes (R3.5: the job had no cue).
				add_sound.call(t0 + j.duration * 0.85, "Disengage", cue_keys("Retreat", 0), CUE_PRIORITY.Retreat, i)
			"Truce":
				add_job.call(new_job(Job.TRUCE, t0, tk), i)
	tl.impact_log.assign(_stable_sorted(tl.impact_log, func(a, b): return a.time < b.time))
	if cfg.cinematic and cfg.choreo:
		# Proposal 5: a hard hull hit (a tenth of the hull or more) rolls the ship and turns it away.
		for imp in tl.impact_log:
			var vt := tl.track(int(imp.target))
			if vt == null or int(imp.hull_damage) <= 0:
				continue
			var frac := float(imp.hull_damage) / maxf(1.0, vt.hull_max)
			if frac >= 0.1 or vt.hull_max <= 0.0 and int(imp.hull_damage) >= 10:
				var side := 1.0 if unit_hash(int(imp.event_index), 31) < 0.5 else -1.0
				vt.kicks.append(Vector3(float(imp.time), side, clampf(frac * 3.0, 0.4, 1.0)))

	# Barks: shields crossing 75/50/25 percent on the player's ships (the worst crossing of the round), and kills.
	impacts.sort_custom(func(a, b): return a.time < b.time)
	if cfg.player_empire > 0:
		var shields := {}
		for s in info:
			shields[int(s.fleetId)] = int(s.get("shields", 0))
		var worst := {}
		var worst_level := 100
		var kill_barked := false
		for imp in impacts:
			var target_info = info_by_id.get(imp.target)
			if target_info != null and int(target_info.get("ownerId", 0)) == cfg.player_empire \
					and int(target_info.get("maxShields", 0)) > 0 and imp.shield_damage > 0:
				var max_s := int(target_info.maxShields)
				var before: int = shields.get(imp.target, 0) * 100 / max_s
				shields[imp.target] = maxi(0, shields.get(imp.target, 0) - imp.shield_damage)
				var after: int = shields[imp.target] * 100 / max_s
				for level in [25, 50, 75]:
					if before > level and after <= level and level < worst_level:
						worst_level = level
						worst = {"time": imp.time, "detail": "shields.%d" % level,
							"priority": 7 if level == 25 else (6 if level == 50 else 5), "fleet_id": imp.target}
						break
			if imp.destroyed and not kill_barked and imp.shooter_owner == cfg.player_empire \
					and owner_of.call(imp.target) != cfg.player_empire:
				tl.barks.append({"time": imp.time + 0.3 * tk, "detail": "kill", "priority": 5, "fleet_id": imp.source})
				kill_barked = true
		if worst_level < 100:
			tl.barks.append(worst)

	tl.jobs.assign(_stable_sorted(tl.jobs, func(a, b):
		if a.start != b.start: return a.start < b.start
		if a.event_index != b.event_index: return a.event_index < b.event_index
		return a.index < b.index))
	tl.sounds.assign(_stable_sorted(tl.sounds, func(a, b): return a.time < b.time))
	tl.barks.assign(_stable_sorted(tl.barks, func(a, b): return a.time < b.time))
	tl._flown_cache.clear() # kills and kicks were set after the choreography
	tl.end_seconds = tl.round_seconds
	for j in tl.jobs:
		tl.end_seconds = maxf(tl.end_seconds, job_end(j))
	return tl


# ------------------------------------------------------------------------------------------------------------------
# Cinematic clock (Config.cinematic)
# ------------------------------------------------------------------------------------------------------------------

## How long a beam is held (M4 combat; Simon's 13 Nov floor and ceiling are 0.6 and 1.5 s). Footage, BATTLE-REFERENCE.md:
## Federation phasers 0.6-1.5 s, Cardassian beams 1.4 s and more, polaron beams 0.75 s and more, Klingon beams short (0.25-0.6 s
## measured, held to the 0.6 s floor).
static func beam_hold(weapon: String, owner_faction: int, h: float) -> float:
	if weapon == "PolaronBeam":
		return lerpf(0.9, 1.4, h)
	if owner_faction == 4:
		return lerpf(1.2, 1.5, h)
	if owner_faction == 2 or owner_faction == 3:
		return lerpf(0.6, 0.9, h)
	return lerpf(0.8, 1.4, h)


## Disruptors are beam family in the core; the cinematic playback draws them as bolts (Klingon and Romulan), as the
## shows do. Cardassian disruptors are beams on screen and stay beams.
static func shows_as_bolts(weapon: String, owner_faction: int, length_meters := -1.0) -> bool:
	# M4 combat, footage WotW 4805 s: Klingon capital ships (Vor'cha, K'Vort) fire red disruptor BEAMS; the Bird-of-Prey
	# wing cannons fire green bolts. A Klingon hull over the small-ship limit draws beams, the rest bolts.
	if owner_faction == 2 and length_meters > 260.0:
		return false
	return weapon == "Disruptor" and owner_faction != 4


## Window openings per core tick (TICKS_PER_ROUND + 1 entries). Tick clock: tick x tick_seconds. Cinematic: the
## movement ticks share [0, fire_from], then each fire tick gets a share of [fire_from, fire_to] that grows with the
## shots it holds.
static func _windows(events: Array, cfg: Config, round_secs: float) -> PackedFloat64Array:
	var w := PackedFloat64Array()
	w.resize(TICKS_PER_ROUND + 1)
	var tk := round_secs / TICKS_PER_ROUND
	if not cfg.cinematic:
		for k in w.size():
			w[k] = k * tk
		return w
	var shots := PackedInt32Array()
	shots.resize(TICKS_PER_ROUND)
	shots.fill(0)
	var first := TICKS_PER_ROUND
	for e in events:
		if e.kind == "Fire" or e.kind == "Ram":
			var k := clampi(int(e.tick), 0, TICKS_PER_ROUND - 1)
			shots[k] += 1
			first = mini(first, k)
	var fire0 := clampi(mini(first, cfg.move_end_tick + 1), 1, TICKS_PER_ROUND - 1)
	var open := cfg.fire_from * round_secs
	var close := cfg.fire_to * round_secs
	for k in fire0 + 1:
		w[k] = open * float(k) / float(fire0)
	# A tick with nothing in it is a short beat, not dead air: the round's time goes to the ticks where shots launch
	# (the core fires a mount on the same tick for every ship, so one tick often carries a whole broadside).
	var weight := func(k: int) -> float:
		if shots[k] == 0:
			return cfg.idle_weight
		return 1.0 + cfg.busy_weight * shots[k]
	var total := 0.0
	for k in range(fire0, TICKS_PER_ROUND):
		total += weight.call(k)
	for k in range(fire0, TICKS_PER_ROUND):
		w[k + 1] = w[k] + (close - open) * weight.call(k) / total
	return w


## A mount's firing arc from round_ship_info's "arcs" (Vector2(centre, half-width) in degrees of |relative bearing|,
## TacticalCombat.h arcCentre / arcHalfWidth); all round when unknown.
static func _mount_arc(ship_info: Dictionary, mount: int) -> Vector2:
	var arcs: Array = ship_info.get("arcs", [])
	if mount >= 0 and mount < arcs.size() and arcs[mount] is Vector2:
		return arcs[mount]
	return Vector2(0.0, 180.0)


## Where in its window (0..launch_span) a shot launches: the moment along the flown paths when the target sits best in
## the mount's arc, pulled toward a deterministic jitter so a broadside does not fire as one.
static func _aim_fraction(tl: BattleTimeline, fid: int, tgt: int, arc: Vector2, w0: float, ww: float, jit: float, cfg: Config) -> float:
	var want := jit * cfg.launch_span
	var s := tl.track(fid)
	var t := tl.track(tgt)
	if s == null or t == null:
		return want
	var best := INF
	var best_f := want
	for k in 13:
		var f := cfg.launch_span * k / 12.0
		var at := w0 + ww * f
		var ps := tl.pose_of(s, at)
		var pt := tl.pose_of(t, at)
		var bearing := rad_to_deg(atan2(pt.y - ps.y, pt.x - ps.x))
		var rel := absf(shortest_turn(ps.facing, bearing))
		var outside := maxf(0.0, absf(rel - arc.x) - arc.y)
		var score := outside / 30.0 + absf(f - want) * 2.0
		if score < best:
			best = score
			best_f = f
	return best_f


## The core's resolution rank of an impact, compared with key_less: the bridge's resolveOrder (rules 4: the index of the
## event's hit in RoundResult.hits, which the core fills as it resolves) when present, else (impact tick, launch-tick
## impacts before arrivals, event order), which is the core's order except between arrivals of one tick.
static func impact_key(e: Dictionary, index: int) -> Array:
	if e.has("resolveOrder") and e.resolveOrder != null and int(e.resolveOrder) >= 0:
		return [0, int(e.resolveOrder), 0, index]
	var tick := int(_num(e, "tick"))
	var impact := maxi(tick, int(_num(e, "impactTick", tick)))
	return [1, impact, 0 if impact == tick else 1, index]


static func key_less(a: Array, b: Array) -> bool:
	for k in mini(a.size(), b.size()):
		if a[k] != b[k]:
			return a[k] < b[k]
	return a.size() < b.size()


## Cinematic launch and impact times: event index -> [launch, impact]. Launches fall in their fire tick's window by
## arc and jitter; beams and bolts land a reach later, torpedoes late in their impact tick's window. Then, until
## stable: impacts on each ship keep the core's order with effect_gap_seconds between them (moved later, never
## earlier), and a ship destroyed in the round launches everything it fires before the blow that kills it.
static func _schedule(tl: BattleTimeline, events: Array, info_by_id: Dictionary, cfg: Config, launches := {}) -> Dictionary:
	var W := tl.window_starts
	var out := {}
	var together := {}     # event index -> true: launch and impact move as one (beams, bolts, rams)
	var keys := {}
	var by_target := {}    # target fleet id -> event indices that land on it
	var launches_by := {}  # shooter fleet id -> event indices it fires
	var kill_of := {}      # victim fleet id -> the event index that destroys it
	for i in events.size():
		var e: Dictionary = events[i]
		var ram: bool = e.kind == "Ram"
		if e.kind != "Fire" and not ram:
			continue
		var tick := clampi(int(e.tick), 0, TICKS_PER_ROUND - 1)
		var impact_tick := clampi(int(_num(e, "impactTick", e.tick)), tick, TICKS_PER_ROUND - 1)
		var fid := int(_num(e, "fleetId"))
		var tgt := int(_num(e, "targetFleetId"))
		var mount := int(_num(e, "mountIndex", -1))
		var family := family_index(_num(e, "family", 0))
		var n := maxi(1, int(_num(e, "projectiles", 1)))
		var w0: float = W[tick]
		var ww: float = W[tick + 1] - W[tick]
		if not ram and tick > 0:
			# Presentation only: the window opens early so ticks overlap (window_lead); the core's tick order still
			# holds on each target through the order pass below.
			var lead := minf(cfg.window_lead * ww, w0 - W[0])
			w0 -= lead
			ww += lead
		var launch := 0.0
		if launches.has(i):
			launch = float(launches[i]) # M4 flight: when the sim's geometry gave the shooter its target
		else:
			var jit := unit_hash(fid * 64 + maxi(0, mount) + 1, 7001 + i)
			var f := 0.5 * cfg.launch_span if ram else _aim_fraction(tl, fid, tgt, _mount_arc(info_by_id.get(fid, {}), mount), w0, ww, jit, cfg)
			launch = w0 + ww * f
		var impact := launch
		if ram:
			together[i] = true
		elif family == 2:
			# Flight time from the core's range (impact tick - fire tick), at a pace that reads as a torpedo run, not
			# stretched over the core's ticks; the order pass below delays it only where the core's order demands.
			var flight := minf(cfg.max_torpedo_flight_seconds, cfg.min_torpedo_flight_seconds + cfg.torpedo_seconds_per_tick * maxi(0, impact_tick - tick - 1))
			impact = launch + flight + (spread_tubes(i, n, float(info_by_id.get(fid, {}).get("lengthMeters", 0.0)), cfg) - 1) * torpedo_stagger(fid, cfg)
		else:
			var owner := int(info_by_id.get(fid, {}).get("ownerId", 0))
			var bolts := family == 1 or shows_as_bolts(str(_num(e, "weapon", "")), owner, float(info_by_id.get(fid, {}).get("lengthMeters", -1.0)))
			impact = launch + (cfg.bolt_flight_seconds if bolts else cfg.beam_reach_seconds)
			together[i] = true
		out[i] = [launch, impact]
		if not ram:
			if not launches_by.has(fid):
				launches_by[fid] = []
			launches_by[fid].append(i)
		if ram or (bool(_num(e, "hit", false)) and not bool(_num(e, "intercepted", false))):
			keys[i] = impact_key(e, i)
			if not by_target.has(tgt):
				by_target[tgt] = []
			by_target[tgt].append(i)
			if bool(_num(e, "destroyed", false)):
				kill_of[tgt] = i
	for tgt in by_target:
		var list: Array = by_target[tgt]
		list.sort_custom(func(a, b): return key_less(keys[a], keys[b]))
	# M1 battlechoreo, proposal 1: every ship fires on its own clock across the round instead of the core's mount
	# ticks. The core's order per target and the kill rule still hold through the order pass below.
	if launches.is_empty():
		_ship_clocks(tl, events, info_by_id, cfg, out, together)
	else:
		_flight_clocks(tl, events, info_by_id, cfg, out)
	var gap := cfg.effect_gap_seconds
	for _pass in 24:
		var changed := false
		for tgt in by_target:
			var prev := -INF
			for i in by_target[tgt]:
				var lag: float = prev + gap - float(out[i][1])
				if lag > 1e-6:
					out[i][1] += lag
					if together.has(i):
						out[i][0] += lag
					changed = true
				prev = out[i][1]
		for victim in kill_of:
			var k: int = kill_of[victim]
			for i in launches_by.get(victim, []):
				var lag: float = float(out[i][0]) + gap - float(out[k][1])
				if i != k and lag > 1e-6:
					out[k][1] += lag
					if together.has(k):
						out[k][0] += lag
					changed = true
		# The order pass may bunch beams again: re-apply the limit (it only delays) until both hold.
		var before := var_to_str(out)
		_beam_limit(tl, events, info_by_id, cfg, out, tl.get_meta("trains"), tl.get_meta("spans"))
		if var_to_str(out) != before:
			changed = true
		if not changed:
			break
	return out


## Flight style by the shown class's length: 2 escort (attack runs), 1 cruiser (turns to bear, drifts), 0 capital.
static func ship_style(length_meters: float, cfg: Config = null) -> int:
	if is_small_ship(length_meters, cfg):
		return 2
	return 1 if length_meters > 0.0 and length_meters <= 500.0 else 0


## Tubes drawn for one core torpedo shot (coordinator, 10 Nov 2026; presentation only): escort 1-2, cruiser 2-3,
## capital 4-5, never fewer than the core's projectiles.
## Seconds between the tubes of one launcher's salvo: 0.10-0.15 s, seeded per ship (footage WotW d_4891).
## M4 flight stage 3 (Simon, 14 Nov: "limit the maximum number of torpedoes per turn per ship"): drawn torpedoes per
## ship per round, by ship_style (capital, cruiser, escort). Presentation only: the core's shots and damage are untouched.
const TORPEDO_SALVO_MAX := [3, 2, 2]

## One torpedo salvo per ship per round (drawn only). Each ship's torpedo shots this round are grouped by target; the
## first shot on each target leads a sub-salvo and all the sub-salvos leave together, rippling 0.10-0.15 s apart, from
## the ship's first launch. The ship's TORPEDO_SALVO_MAX tubes are shared over its targets (at least one each). A further
## shot on a target already in the salvo draws no torpedo (event -> 0 tubes): it lands with that sub-salvo when no other
## ship's impact on the target falls between (the core's order on every target is kept), else at its own impact time.
## Mutates sched (launch, and impact for folded shots). Returns event index -> tubes drawn.
static func _torpedo_salvos(events: Array, sched: Dictionary, info_by_id: Dictionary, cfg: Config) -> Dictionary:
	var out := {}
	var by_ship := {}
	for i in sched:
		var e: Dictionary = events[i]
		if str(e.kind) != "Fire" or family_index(_num(e, "family", 0)) != 2:
			continue
		var fid := int(_num(e, "fleetId"))
		if not by_ship.has(fid):
			by_ship[fid] = []
		by_ship[fid].append(i)
	# Every scheduled impact per target, to keep the core's order when a shot is folded into an earlier sub-salvo.
	var impacts_on := {}
	for i in sched:
		var tgt := int(_num(events[i], "targetFleetId"))
		if not impacts_on.has(tgt):
			impacts_on[tgt] = []
		impacts_on[tgt].append([float(sched[i][1]), int(_num(events[i], "fleetId"))])
	for fid in by_ship:
		var list: Array = by_ship[fid]
		list.sort_custom(func(a, b): return float(sched[a][0]) < float(sched[b][0]) or (float(sched[a][0]) == float(sched[b][0]) and a < b))
		var meters := float(info_by_id.get(fid, {}).get("lengthMeters", 0.0))
		var cap: int = TORPEDO_SALVO_MAX[ship_style(meters, cfg)] if meters > 0.0 else TORPEDO_SALVO_MAX[0]
		var targets: Array = []
		var lead_of := {}
		for i in list:
			var tgt := int(_num(events[i], "targetFleetId"))
			if not lead_of.has(tgt):
				lead_of[tgt] = i
				targets.append(tgt)
		var share := {}
		for k in targets.size():
			share[targets[k]] = 1
		var spare := cap - targets.size()
		var k2 := 0
		while spare > 0 and not targets.is_empty():
			share[targets[k2 % targets.size()]] += 1
			spare -= 1
			k2 += 1
		var t0 := float(sched[list[0]][0])
		var stagger := torpedo_stagger(fid, cfg)
		var tube := 0
		for tgt in targets:
			var li: int = lead_of[tgt]
			var imp := float(sched[li][1])
			var launch := minf(t0 + tube * stagger, imp - 0.3 - (int(share[tgt]) - 1) * stagger)
			launch = maxf(launch, imp - 2.5) # a target hit much later is not reached by a torpedo crawling for seconds
			sched[li][0] = maxf(0.0, launch)
			out[li] = int(share[tgt])
			tube += int(share[tgt])
		for i in list:
			var tgt := int(_num(events[i], "targetFleetId"))
			var li: int = lead_of[tgt]
			if i == li:
				continue
			out[i] = 0
			var lead_imp := float(sched[li][1])
			var own := float(sched[i][1])
			var between := false
			for rec in impacts_on.get(tgt, []):
				if int(rec[1]) != fid and float(rec[0]) > lead_imp and float(rec[0]) < own:
					between = true
			sched[i][0] = sched[li][0]
			if not between:
				sched[i][1] = lead_imp
	return out


static func torpedo_stagger(fleet_id: int, cfg: Config) -> float:
	return lerpf(0.10, 0.15, unit_hash(fleet_id, 26)) * (cfg.torpedo_stagger_seconds / 0.12)


static func spread_tubes(event_index: int, projectiles: int, length_meters: float, cfg: Config) -> int:
	var lo_n: int = [4, 4, 2][ship_style(length_meters, cfg)] # M4: salvos of 4-5 from capitals and cruisers (footage WotW d_4891), 2-3 from escorts
	return maxi(projectiles, lo_n + (1 if unit_hash(event_index, 25) < 0.5 else 0))


## Proposal 1 (M1 battlechoreo): each ship's shots on its own seeded cadence. A capital's many mounts spread over the
## whole round as sustained fire, a cruiser's at 0.4-1.2 s, an escort's bunch into one strafing pass; a ship that dies
## this round fires early. Then no more than max_beams_at_once beams are held across the battle (a beam waits for one
## to end). out[i] = [launch, impact] keeps each shot's own launch-to-impact span.
static func _ship_clocks(tl: BattleTimeline, events: Array, info_by_id: Dictionary, cfg: Config, out: Dictionary, together: Dictionary) -> void:
	var R := tl.round_seconds
	var A := cfg.fire_from * R
	var B := cfg.fire_to * R
	var victims := {}
	for e in events:
		if e.kind == "Destroyed":
			victims[int(_num(e, "fleetId"))] = true
	var trains := {}
	for i in out:
		var fid := int(_num(events[i], "fleetId"))
		if not trains.has(fid):
			trains[fid] = []
		trains[fid].append(i)
	var span_of := {}
	for i in out:
		span_of[i] = float(out[i][1]) - float(out[i][0])
	for fid in trains:
		var list: Array = trains[fid]
		list.sort_custom(func(a, b):
			var ea: Dictionary = events[a]
			var eb: Dictionary = events[b]
			if int(ea.tick) != int(eb.tick): return int(ea.tick) < int(eb.tick)
			if int(_num(ea, "mountIndex", -1)) != int(_num(eb, "mountIndex", -1)): return int(_num(ea, "mountIndex", -1)) < int(_num(eb, "mountIndex", -1))
			return a < b)
		var n := list.size()
		var meters := float(info_by_id.get(fid, {}).get("lengthMeters", 0.0))
		var style := ship_style(meters, cfg)
		var lo := A
		var hi := B
		var gap := lerpf(cfg.ship_gap_min, cfg.ship_gap_max, unit_hash(fid, 11))
		if style == 2:
			# The pass: the escort needs time to run in, and to break away and come about before the round ends.
			lo = maxf(A, 0.1)
			hi = minf(B, R * cfg.flight_to - 0.9)
			gap = lerpf(cfg.escort_gap_min, cfg.escort_gap_max, unit_hash(fid, 11))
		elif style == 0 and n > 2:
			# Sustained fire: a capital's mounts spread evenly over the whole round.
			gap = maxf(0.3, (hi - lo - 0.4) / float(n - 1))
		if victims.has(fid):
			hi = minf(hi, lo + 2.5)
			gap = minf(gap, 0.5)
		if n > 1:
			gap = minf(gap, maxf(0.15, (hi - lo - 0.1) / float(n - 1)))
		var room := maxf(0.0, hi - lo - gap * (n - 1))
		# M4: ships open fire in turn across the round (rank by seeded hash), the first at once and the last late, so
		# fire runs through the whole round (a random start left 2 s of dead air at the top and 3 s at the tail).
		# A ship with torpedoes goes first: beams on a target wait for its torpedoes' impacts (the core's order), so a
		# beam shooter first in line would leave the round's opening empty.
		var rank := 0
		var my_h := unit_hash(fid, 12) + (0.0 if _has_torpedo(events, trains[fid]) else 1.0)
		for other_fid in trains:
			if unit_hash(int(other_fid), 12) + (0.0 if _has_torpedo(events, trains[other_fid]) else 1.0) < my_h:
				rank += 1
		var start := lo + room * (float(rank) / float(maxi(1, trains.size() - 1)))
		for k in n:
			var jit := (unit_hash(fid * 131 + k, 13) - 0.5) * 0.3 * gap if k > 0 else 0.0
			out[list[k]][0] = start + k * gap + jit
	for i in out:
		out[i][1] = float(out[i][0]) + float(span_of[i])
	_beam_limit(tl, events, info_by_id, cfg, out, trains, span_of)
	tl.set_meta("trains", trains)
	tl.set_meta("spans", span_of)


## M4 flight: the sim already chose every launch; this keeps the battle-wide limits (beams held at once, salvo gaps) and
## records each ship's shots for the order pass, as _ship_clocks does.
static func _flight_clocks(tl: BattleTimeline, events: Array, info_by_id: Dictionary, cfg: Config, out: Dictionary) -> void:
	var trains := {}
	var span_of := {}
	for i in out:
		var fid := int(_num(events[i], "fleetId"))
		if not trains.has(fid):
			trains[fid] = []
		trains[fid].append(i)
		span_of[i] = float(out[i][1]) - float(out[i][0])
	_beam_limit(tl, events, info_by_id, cfg, out, trains, span_of)
	tl.set_meta("trains", trains)
	tl.set_meta("spans", span_of)


## Seconds from a Fire or Ram event's launch to the impact the core's damage lands with (before the order pass): a beam's
## reach, a burst's flight, a torpedo spread's flight plus its stagger. Shared by _schedule and BattleFlight.
static func shot_span(e: Dictionary, i: int, info_by_id: Dictionary, cfg: Config) -> float:
	if e.kind == "Ram":
		return 0.0
	var fid := int(_num(e, "fleetId"))
	var family := family_index(_num(e, "family", 0))
	if family == 2:
		var tick := clampi(int(e.tick), 0, TICKS_PER_ROUND - 1)
		var impact_tick := clampi(int(_num(e, "impactTick", e.tick)), tick, TICKS_PER_ROUND - 1)
		var n := maxi(1, int(_num(e, "projectiles", 1)))
		var flight := minf(cfg.max_torpedo_flight_seconds, cfg.min_torpedo_flight_seconds + cfg.torpedo_seconds_per_tick * maxi(0, impact_tick - tick - 1))
		return flight + (spread_tubes(i, n, float(info_by_id.get(fid, {}).get("lengthMeters", 0.0)), cfg) - 1) * torpedo_stagger(fid, cfg)
	var owner := int(info_by_id.get(fid, {}).get("ownerId", 0))
	var bolts := family == 1 or shows_as_bolts(str(_num(e, "weapon", "")), owner, float(info_by_id.get(fid, {}).get("lengthMeters", -1.0)))
	return cfg.bolt_flight_seconds if bolts else cfg.beam_reach_seconds


static func _has_torpedo(events: Array, list: Array) -> bool:
	for i in list:
		if family_index(_num(events[i], "family", 0)) == 2:
			return true
	return false


## M4 combat: torpedo salvo starts across the battle keep a gap, so fire flows through the round instead of landing in
## clusters (Simon, 13 Nov: "torpedoes all get fired at once"). Only delays; each ship's own order is kept by the caller.
static func _salvo_limit(tl: BattleTimeline, events: Array, cfg: Config, out: Dictionary) -> void:
	var salvos := []
	for i in out:
		var e: Dictionary = events[i]
		if e.kind == "Fire" and family_index(_num(e, "family", 0)) == 2:
			salvos.append(i)
	if salvos.size() < 2:
		return
	salvos.sort_custom(func(a, b): return float(out[a][0]) < float(out[b][0]) or (float(out[a][0]) == float(out[b][0]) and a < b))
	var avail := maxf(1.0, tl.round_seconds * (cfg.fire_to - cfg.fire_from))
	var gap := clampf(0.8 * avail / float(salvos.size()), 0.05, 0.25)
	var prev := -INF
	for i in salvos:
		var at := maxf(float(out[i][0]), prev + gap)
		out[i][0] = at
		prev = at


## No more than max_beams_at_once beams held across the battle: a beam waits for one to end (launch and impact move
## together), and each ship's own shots keep their order. Only delays; run again after every order pass.
static func _beam_limit(tl: BattleTimeline, events: Array, info_by_id: Dictionary, cfg: Config, out: Dictionary, trains: Dictionary, span_of: Dictionary) -> void:
	var R := tl.round_seconds
	_salvo_limit(tl, events, cfg, out)
	# Footage: 1-4 beams in a fleet shot; a bigger battle shows more (one more for every six ships), never a whole line.
	var beam_cap := cfg.max_beams_at_once + info_by_id.size() / 6
	var beams := []
	for i in out:
		var e: Dictionary = events[i]
		if e.kind == "Fire" and family_index(_num(e, "family", 0)) == 0 and not shows_as_bolts(str(_num(e, "weapon", "")), int(info_by_id.get(int(_num(e, "fleetId")), {}).get("ownerId", 0)), float(info_by_id.get(int(_num(e, "fleetId")), {}).get("lengthMeters", -1.0))):
			beams.append(i)
	beams.sort_custom(func(a, b): return float(out[a][0]) < float(out[b][0]) or (float(out[a][0]) == float(out[b][0]) and a < b))
	var live := []
	var hold := cfg.beam_first_seconds + cfg.beam_refire_gap + cfg.beam_refire_seconds
	for i in beams:
		var s := float(out[i][0])
		var c_i := maxi(1, int(_num(events[i], "projectiles", 1)))
		for _try in 60:
			var c := 0
			var first_end := INF
			for iv in live:
				if iv[0] < s + 0.2 and iv[1] > s:
					c += iv[2]
					first_end = minf(first_end, iv[1])
			if c + c_i <= beam_cap or c == 0 or s > R * 0.9:
				break
			s = first_end + 0.02
		out[i][0] = s
		live.append([s, s + hold, c_i])
	# A ship's own shots keep their order and spacing after the beam limit moved some.
	for fid in trains:
		var list: Array = trains[fid]
		list.sort_custom(func(a, b): return float(out[a][0]) < float(out[b][0]))
		for k in range(1, list.size()):
			out[list[k]][0] = maxf(float(out[list[k]][0]), float(out[list[k - 1]][0]) + 0.2)
	for i in out:
		out[i][1] = maxf(float(out[i][1]), float(out[i][0]) + float(span_of[i]))


## Ship info for round `round_index` (index into encounter.rounds) of a tactical encounter as returned by
## botf_call("tactical") / ("tacticalRound"): owner, shields at round start, cloak state carried from earlier rounds
## and a fallback pose. Ships already destroyed or gone before the round are left out.
## lengthMeters is the class's canon length from the catalogue (ShipFactory.canon_length_meters); BattleScene overrides
## it from the local asset manifest when one is present.
static func round_ship_info(encounter: Dictionary, round_index: int) -> Array:
	var rounds: Array = encounter.get("rounds", [])
	var rnd: Dictionary = rounds[round_index]
	var by_id := {}
	for s in encounter.get("ships", []):
		by_id[int(s.id)] = s
	var cloaked := {}
	for r in round_index:
		for e in rounds[r].get("events", []):
			if e.kind == "Cloak":
				cloaked[int(e.fleetId)] = true
			elif e.kind == "Decloak":
				cloaked.erase(int(e.fleetId))
	# Round 1 marks ships that start cloaked with a tick-0 Cloak event.
	if int(rnd.get("index", round_index + 1)) == 1:
		for e in rnd.get("events", []):
			if e.kind == "Cloak" and int(e.tick) == 0:
				cloaked[int(e.fleetId)] = true
	var out := []
	for rs in rnd.get("ships", []):
		if int(rs.get("hullBefore", 1)) <= 0:
			continue
		var fid := int(rs.id)
		var ship: Dictionary = by_id.get(fid, {})
		var pos: Dictionary = rs.get("from", ship.get("position", {"x": 0, "y": 0}))
		# Firing arcs per mount (the encounter's weapons list is indexed by Fire events' mountIndex).
		var arcs := []
		for wpn in ship.get("weapons", []):
			arcs.append(Vector2(float(wpn.get("arcCentre", 0)), float(wpn.get("arcHalfWidth", 180))) if wpn is Dictionary else Vector2(0, 180))
		out.append({"arcs": arcs, "fleetId": fid, "ownerId": int(rs.get("owner", ship.get("owner", 0))),
			"hull": int(rs.get("hullBefore", 0)), "maxHull": int(ship.get("maxHull", rs.get("hullBefore", 0))), "shields": int(rs.get("shieldBefore", 0)), "maxShields": int(ship.get("maxShields", 0)),
			"startsCloaked": cloaked.has(fid) or bool(rs.get("startsCloaked", false)), "x": float(pos.x), "y": float(pos.y),
			"facing": float(rs.get("yaw", 0)), "lengthMeters": ShipFactory.canon_length_meters(ship),
			"elevation": ship.get("elevation") if ship.has("wego") else null})
	return out


static func new_job(kind: int, start: float, duration: float) -> Dictionary:
	return {"kind": kind, "start": start, "duration": duration, "fleet_id": 0, "target_fleet_id": 0,
		"source_fleet_id": 0, "mount_index": -1, "weapon": "", "family": 0, "index": 0, "count": 1,
		"outcome": Outcome.HIT, "color": Color.WHITE, "arc_cells": 0.0, "overshoot_cells": 0.0, "end_fraction": 1.0,
		"shield_damage": 0, "hull_damage": 0, "destroys_target": false, "event_index": -1, "vfx_key": "",
		"facing": "", "subsystem": "none", "subsystem_damage": 0, "subsystem_destroyed": false, "breach": 0, "aimed": false}


## Array.sort_custom is not stable; this merge sort is.
static func _stable_sorted(arr: Array, less: Callable) -> Array:
	if arr.size() <= 1:
		return arr.duplicate()
	var mid := arr.size() / 2
	var a := _stable_sorted(arr.slice(0, mid), less)
	var b := _stable_sorted(arr.slice(mid), less)
	var out := []
	var i := 0
	var j := 0
	while i < a.size() and j < b.size():
		if less.call(b[j], a[i]):
			out.append(b[j]); j += 1
		else:
			out.append(a[i]); i += 1
	out.append_array(a.slice(i))
	out.append_array(b.slice(j))
	return out


# ------------------------------------------------------------------------------------------------------------------
# Cinematic camera director
# ------------------------------------------------------------------------------------------------------------------

static func job_interest(kind: int) -> int:
	match kind:
		Job.EXPLOSION: return 10
		Job.HULL, Job.RAM: return 7
		Job.TORPEDO: return 6
		Job.DECLOAK, Job.INTERCEPT: return 5
		Job.WARP, Job.SHIELD: return 4
		Job.PULSE, Job.BEAM: return 3
		Job.CLOAK: return 2
		Job.DISENGAGE: return 1
	return 0


## Picks what the cinematic camera looks at: the most interesting active job, held for at least min_shot_seconds so
## the camera never twitches (explosions may cut early).
class Director:
	var min_shot_seconds := 1.4
	var subject := 0
	var other := 0
	var since := 0.0
	var kind := -1 ## Job kind that motivated the current shot (not in the C++ director; -1 = none).
	## fleet_id -> 0..3, how far a ship sits on the outside of its formation (set by the scene). Among equally
	## interesting jobs the director prefers an outside shooter: a clean cutaway angle exists for it.
	var outer := {}

	func reset() -> void:
		subject = 0; other = 0; since = 0.0; kind = -1

	## Returns true when the shot changed.
	func update(tl: BattleTimeline, t: float, preferred := 0) -> bool:
		var best = null
		var best_score := -1
		for j in tl.jobs:
			if j.start > t:
				break
			if t > j.start + j.duration or j.fleet_id == 0:
				continue
			var score := BattleTimeline.job_interest(j.kind) * 4 + int(outer.get(j.fleet_id, 0))
			if preferred != 0 and (j.fleet_id == preferred or j.target_fleet_id == preferred):
				score += 2
			if score > best_score:
				best_score = score
				best = j
		if best == null:
			return false
		var shooter_shot: bool = best.kind in [Job.BEAM, Job.PULSE, Job.TORPEDO, Job.RAM]
		var next_subject: int = best.fleet_id if shooter_shot else (best.target_fleet_id if best.target_fleet_id != 0 else best.fleet_id)
		var next_other: int = best.target_fleet_id if shooter_shot else best.source_fleet_id
		if next_subject == subject and next_other == other:
			return false
		var urgent: bool = best.kind == Job.EXPLOSION and subject != next_subject
		if subject != 0 and t - since < min_shot_seconds and not urgent:
			return false
		subject = next_subject; other = next_other; since = t; kind = best.kind
		return true
