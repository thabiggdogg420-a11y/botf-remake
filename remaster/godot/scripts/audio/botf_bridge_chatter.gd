class_name BotfBridgeChatter
extends RefCounted
## The bridge chatter layer: low-priority crew lines in a fight, drawn from the original BOTF combat groups
## (vo.combat.underattack / phaser / torpedo / shields.* / evade / kill / encounter; the originals have no "target" line), chosen by what has
## just happened. It speaks only into quiet: BattleTimeline barks and order acknowledgements reset its gap, a detail
## is not repeated within `repeat_seconds`, and nothing is said before the fight has produced events.
## Pure and deterministic (time and seed passed in), unit-tested in scripts/audio/tests/test_audio.gd.

var min_gap := 7.0            ## seconds between chatter lines (and after any other voice line)
var window := 8.0             ## activity looked at, seconds
var repeat_seconds := 24.0    ## the same detail is not chosen again inside this
var max_per_fight := 14       ## stops a long fight turning into a radio play

var lines_said := 0
var _rng := RandomNumberGenerator.new()
var _events: Array = []       ## [{kind, at}]
var _last_voice := -100.0
var _spoken := {}             ## detail -> time
var _last_detail := ""


func _init(seed_value := 1) -> void:
	_rng.seed = seed_value


## A new fight: forgets the activity and the count (the gap clock is kept so a bark right at the start still holds it off).
func reset() -> void:
	_events.clear()
	_spoken.clear()
	_last_detail = ""
	lines_said = 0


## kind: beam, torpedo, shield, hull, destroyed.
func note(kind: String, now: float) -> void:
	_events.append({"kind": kind, "at": now})
	if _events.size() > 160:
		_events = _events.slice(_events.size() - 120)


## Any other voice line played (a bark, an order acknowledgement): holds chatter off for min_gap, and a bark's own
## detail counts as spoken.
func note_voice(now: float, detail := "") -> void:
	_last_voice = now
	if detail != "":
		_spoken[detail] = now


func _count(kind: String, now: float) -> int:
	var n := 0
	for e in _events:
		if e.kind == kind and now - e.at <= window:
			n += 1
	return n


## Weighted choice among the details the last few seconds justify. Returns the detail or "" (nothing to say).
## has_files(detail) -> bool (optional) drops details with no clip. Does not commit: call commit() when the line was played.
func next_line(now: float, has_files := Callable()) -> String:
	if lines_said >= max_per_fight or now - _last_voice < min_gap:
		return ""
	var beams := _count("beam", now)
	var torps := _count("torpedo", now)
	var sh := _count("shield", now)
	var hull := _count("hull", now)
	var dead := _count("destroyed", now)
	if beams + torps + sh + hull + dead == 0:
		return ""
	var w := {}
	if torps > 0:
		w["torpedo"] = 3.0 * minf(torps, 3.0)
	if beams > 0:
		w["phaser"] = 2.0 * minf(beams / 3.0 + 0.34, 3.0)
	if sh >= 12:
		w["shields.25"] = 3.0
	elif sh >= 6:
		w["shields.50"] = 3.0
	elif sh >= 3:
		w["shields.75"] = 2.0
	if hull > 0:
		w["underattack"] = 2.0 * minf(hull, 3.0)
	if hull >= 2:
		w["evade"] = 1.5
	if dead > 0:
		w["kill"] = 5.0
	w["encounter"] = 0.4
	for d in w.keys():
		var spent: bool = d == _last_detail or now - float(_spoken.get(d, -100.0)) < repeat_seconds
		if spent or (has_files.is_valid() and not has_files.call(d)):
			w.erase(d)
	if w.is_empty():
		return ""
	var total := 0.0
	for d in BotfAudioCues.CHATTER_DETAILS:   # fixed order: the seeded draw is reproducible
		total += float(w.get(d, 0.0))
	var r := _rng.randf() * total
	for d in BotfAudioCues.CHATTER_DETAILS:
		r -= float(w.get(d, 0.0))
		if r <= 0.0 and w.has(d):
			return d
	return ""


## The line was played (or tried): the gap restarts and the detail is spent.
func commit(detail: String, now: float, played := true) -> void:
	_spoken[detail] = now
	_last_detail = detail
	if played:
		_last_voice = now
		lines_said += 1
