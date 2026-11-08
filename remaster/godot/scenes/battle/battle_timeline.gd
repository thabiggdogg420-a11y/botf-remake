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

const TICKS_PER_ROUND := 12

enum Job { BEAM, PULSE, TORPEDO, SHIELD, HULL, INTERCEPT, RAM, CLOAK, DECLOAK, EXPLOSION, WARP, DISENGAGE, TRUCE }
const JOB_NAMES := ["beam", "pulse", "torpedo", "shield", "hull", "intercept", "ram", "cloak", "decloak",
	"explosion", "warp", "disengage", "truce"]
enum Outcome { HIT, MISS, INTERCEPTED }

## Owner faction ids used by the tactical scene: 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi.
const FACTION_PREFIX := {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer"}
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
	var torpedo_arc_cells := 0.12 ## R3.5: a slight curve, not a lob
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
	var capital_climb_cells := 0.0   ## Height change per maneuver around the home height: capital ships hold altitude (R3.5).
	var small_climb_cells := 0.35   ## Escorts change level a little, nose pitched with the climb.
	var max_pitch_degrees := 32.0
	var small_bank_scale := 1.6      ## Small craft roll hard into turns; capital ships lumber.
	var capital_bank_scale := 0.7
	var wing_radius_cells := 2.5     ## Small ships of one side starting this close together fly as a wing...
	var wing_size := 4               ## ...of up to this many, in finger-four on the first (lowest id) ship.
	var wing_spacing_cells := 0.55


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
			var e := BattleTimeline.smoothstep01(u)
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


func track(fleet_id: int) -> ShipTrack:
	for s in ships:
		if s.fleet_id == fleet_id:
			return s
	return null


## Where `s` is at time t: its own flight, or its station in its leader's wing. When the leader is gone (destroyed,
## warped out) the wingman peels off back to its own track over two ticks.
func pose_of(s: ShipTrack, t: float) -> Pose:
	var own := s.pose_at(t)
	var lead := track(s.wing_leader) if s.wing_leader != 0 else null
	if lead == null:
		return own
	var w := 1.0
	if lead.hide_at >= 0.0 and t > lead.hide_at:
		w = 1.0 - smoothstep01((t - lead.hide_at) / maxf(1e-6, 2.0 * tick_seconds))
	if w <= 0.0:
		return own
	var lp := lead.pose_at(t)
	var f := deg_to_rad(lp.facing)
	var p := Pose.new()
	p.x = own.x + (lp.x - cos(f) * s.wing_back - sin(f) * s.wing_side - own.x) * w
	p.y = own.y + (lp.y - sin(f) * s.wing_back + cos(f) * s.wing_side - own.y) * w
	p.altitude = own.altitude + (lp.altitude + s.wing_up - own.altitude) * w
	p.facing = wrap_degrees(own.facing + shortest_turn(own.facing, lp.facing) * w)
	p.bank = own.bank + (lp.bank - own.bank) * w
	p.pitch = own.pitch + (lp.pitch - own.pitch) * w
	return p


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
			# Each empire its own torpedo colour (coordinator, 8 Nov): Federation red-orange, Klingon green-gold.
			match owner_faction:
				2: return Color(0.75, 1.0, 0.15)
				3: return Color(0.2, 1.0, 0.5)
				4: return Color(1.0, 0.8, 0.1)
				5: return Color(0.6, 0.4, 1.0)
			return Color(1.0, 0.25, 0.08)
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
		s.t0 = start_tick * tk
		s.t1 = maxi(int(e.tick), start_tick + 1) * tk
		if not t.segments.is_empty():
			s.t0 = maxf(s.t0, t.segments.back().t1)
		if s.t1 <= s.t0:
			s.t1 = s.t0 + tk
		var slide := int(_num(e, "value"))
		s.slide_cells = slide
		s.maneuver = str(_num(e, "maneuver", ""))
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
	var ctx := {"torpedo_barked": false}
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
		if shield_dmg > 0 or (bool(_num(e, "hit", false)) and hull_dmg <= 0):
			var j := new_job(Job.SHIELD, time, cfg.impact_ticks * tk)
			j.fleet_id = target; j.target_fleet_id = target; j.source_fleet_id = shooter
			j.weapon = weapon; j.family = family; j.shield_damage = shield_dmg
			j.color = weapon_color(weapon, shooter_owner); j.vfx_key = "vfx.impact.shield"
			add_job.call(j, ev)
			add_sound.call(time, "ShieldHit", cue_keys("ShieldHit", 0), CUE_PRIORITY.ShieldHit, ev)
		if hull_dmg > 0:
			var j := new_job(Job.HULL, time, cfg.impact_ticks * 1.5 * tk)
			j.fleet_id = target; j.target_fleet_id = target; j.source_fleet_id = shooter
			j.weapon = weapon; j.family = family; j.hull_damage = hull_dmg; j.shield_damage = shield_dmg
			j.destroys_target = bool(_num(e, "destroyed", false))
			j.color = weapon_color(weapon, shooter_owner); j.vfx_key = "vfx.impact.hull"
			add_job.call(j, ev)
			add_sound.call(time, "HullHit", cue_keys("HullHit", 0), CUE_PRIORITY.HullHit, ev)
		impacts.append({"time": time, "target": target, "source": shooter, "shield_damage": shield_dmg,
			"destroyed": bool(_num(e, "destroyed", false)), "shooter_owner": shooter_owner})

	for i in events.size():
		var e: Dictionary = events[i]
		var fid := int(_num(e, "fleetId"))
		var owner: int = owner_of.call(fid)
		var t0 := int(e.tick) * tk
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
				var impact_time := t0
				if family == 0:
					for k in n:
						var j := base.duplicate()
						j.kind = Job.BEAM; j.index = k
						j.start = t0 + k * cfg.beam_emitter_stagger_ticks * tk
						j.duration = maxf(0.1, cfg.beam_ticks - k * cfg.beam_emitter_stagger_ticks) * tk
						if not hit:
							j.overshoot_cells = cfg.miss_overshoot_cells
						add_job.call(j, i)
					add_sound.call(t0, "BeamFire", keys, 3, i)
					impact_time = t0 + 0.25 * tk
				elif family == 1:
					var stagger := minf(cfg.pulse_stagger_ticks, 0.9 / n)
					for k in n:
						var j := base.duplicate()
						j.kind = Job.PULSE; j.index = k
						j.start = t0 + k * stagger * tk
						j.duration = cfg.pulse_flight_ticks * tk
						if not hit:
							j.overshoot_cells = cfg.miss_overshoot_cells
						add_job.call(j, i)
						add_sound.call(j.start, "BeamFire", keys, 3, i)
						impact_time = j.start + j.duration
				else:
					var arrive := maxf(int(_num(e, "impactTick", e.tick)) * tk, t0 + cfg.min_torpedo_flight_ticks * tk)
					var flight := maxf(0.2 * tk, arrive - t0)
					for k in n:
						var j := base.duplicate()
						j.kind = Job.TORPEDO; j.index = k
						j.start = t0 + k * cfg.torpedo_stagger_ticks * tk
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
					if cfg.player_empire > 0 and owner == cfg.player_empire and not ctx.torpedo_barked:
						tl.barks.append({"time": t0, "detail": "torpedo", "priority": 3, "fleet_id": fid})
						ctx.torpedo_barked = true
					if intercepted:
						var j := base.duplicate()
						j.kind = Job.INTERCEPT
						j.start = t0 + flight * cfg.intercept_fraction; j.duration = cfg.impact_ticks * tk
						j.vfx_key = "vfx.intercept"
						add_job.call(j, i)
						add_sound.call(j.start, "HullHit", cue_keys("HullHit", 0), CUE_PRIORITY.HullHit, i)
				if hit and not intercepted:
					add_impacts.call(e, impact_time, i, family)
					if base.destroys_target:
						kill_times[base.target_fleet_id] = impact_time
			"Ram":
				var impact_at := maxi(int(_num(e, "impactTick", e.tick)), int(e.tick)) * tk
				var start := t0 - 0.5 * tk if t0 - 0.5 * tk > 0 else t0
				var j := new_job(Job.RAM, start, impact_at + 0.5 * tk - start)
				j.fleet_id = fid; j.target_fleet_id = int(_num(e, "targetFleetId"))
				j.shield_damage = int(_num(e, "shieldDamage")); j.hull_damage = int(_num(e, "hullDamage"))
				j.destroys_target = bool(_num(e, "destroyed", false)); j.color = Color(1.0, 0.6, 0.2)
				add_job.call(j, i)
				var hit_event := e.duplicate()
				hit_event.hit = true
				add_impacts.call(hit_event, impact_at, i, 0)
				if j.destroys_target:
					kill_times[j.target_fleet_id] = impact_at
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
				var when := t0 + 0.3 * tk
				if kill_times.has(fid):
					when = maxf(t0, kill_times[fid])
				var j := new_job(Job.EXPLOSION, when, cfg.explosion_ticks * tk)
				j.fleet_id = fid; j.target_fleet_id = fid; j.color = Color(1.0, 0.7, 0.3); j.vfx_key = "vfx.explosion"
				add_job.call(j, i)
				var t: ShipTrack = track_for.call(fid)
				var hide: float = when + 0.35 * j.duration
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
				var j := new_job(Job.DISENGAGE, t0, maxi(1, cfg.move_end_tick) * tk)
				j.fleet_id = fid; j.color = Color(0.7, 0.85, 1.0)
				add_job.call(j, i)
				# The ship leaves the fight: its warp-out sounds as it goes (R3.5: the job had no cue).
				add_sound.call(t0 + j.duration * 0.85, "Disengage", cue_keys("Retreat", 0), CUE_PRIORITY.Retreat, i)
			"Truce":
				add_job.call(new_job(Job.TRUCE, t0, tk), i)

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
	tl.end_seconds = tl.round_seconds
	for j in tl.jobs:
		tl.end_seconds = maxf(tl.end_seconds, job_end(j))
	return tl


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
		out.append({"fleetId": fid, "ownerId": int(rs.get("owner", ship.get("owner", 0))),
			"shields": int(rs.get("shieldBefore", 0)), "maxShields": int(ship.get("maxShields", 0)),
			"startsCloaked": cloaked.has(fid) or bool(rs.get("startsCloaked", false)), "x": float(pos.x), "y": float(pos.y),
			"facing": float(rs.get("yaw", 0)), "lengthMeters": ShipFactory.canon_length_meters(ship)})
	return out


static func new_job(kind: int, start: float, duration: float) -> Dictionary:
	return {"kind": kind, "start": start, "duration": duration, "fleet_id": 0, "target_fleet_id": 0,
		"source_fleet_id": 0, "mount_index": -1, "weapon": "", "family": 0, "index": 0, "count": 1,
		"outcome": Outcome.HIT, "color": Color.WHITE, "arc_cells": 0.0, "overshoot_cells": 0.0, "end_fraction": 1.0,
		"shield_damage": 0, "hull_damage": 0, "destroys_target": false, "event_index": -1, "vfx_key": ""}


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
