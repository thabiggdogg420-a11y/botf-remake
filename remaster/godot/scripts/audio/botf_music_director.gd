class_name BotfMusicDirector
extends RefCounted
## F5 adaptive music: the strategic map's music follows the state of the player's empire.
##
## docs/MUSIC-IDENTIFIED.md (tools/identify_music.py) measured every segment of the original soundtrack and gave each one an
## intensity tier inside its own track: calm, build or peak. The director reads that table and picks the mood from the view:
##   calm  - at peace with every power the player has met;
##   build - at war with a power, nothing hostile near home;
##   peak  - at war and a hostile fleet sits within THREAT_RANGE sectors of one of the player's colonies, or the player lost a
##           colony since the last view.
## The audio system plays a track's segments in game order but skips segments whose tier the mood does not allow, so the music
## changes on a segment boundary. A rise to peak changes at once (crossfade); anything else waits for the segment to end.
## Battle music (approach, then tension once rounds are fought) is unchanged. Pure logic: no nodes, no audio.

const TABLE_PATH := "res://scripts/audio/music_identified.json"
const THREAT_RANGE := 3
const MOODS := ["calm", "build", "peak"]
## Which segment tiers each mood plays (first listed is preferred). Every mood falls back to the whole track when a track has
## no segment of an allowed tier.
const ALLOWED := {
	"calm": ["calm", "build"],
	"build": ["build", "calm", "peak"],
	"peak": ["peak", "build"],
}

static var _tiers: Dictionary = {}   ## "music_tNN_sMM" -> tier
static var _loaded := false


static func load_table(path := TABLE_PATH) -> int:
	_tiers.clear()
	_loaded = true
	if not FileAccess.file_exists(path):
		return 0
	var doc: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if not (doc is Dictionary):
		return 0
	for t in doc.get("tracks", []):
		for s in t.get("segments", []):
			_tiers[str(s.get("file", "")).get_basename()] = str(s.get("tier", ""))
	return _tiers.size()


## The tier of a playlist file ("" when unknown). Pack files are named music_music_tNN_sMM.ogg; the table uses music_tNN_sMM.
static func tier_of(path: String) -> String:
	if not _loaded:
		load_table()
	var base := path.get_file().get_basename()
	var i := base.rfind("music_t")
	if i < 0:
		return ""
	return str(_tiers.get(base.substr(i), ""))


static func allows(mood: String, path: String) -> bool:
	var t := tier_of(path)
	return t == "" or t in ALLOWED.get(mood, MOODS)


## The next playlist index after `pos` that the mood allows (wrapping). If no segment fits, the plain next one.
static func next_index(playlist: PackedStringArray, pos: int, mood: String) -> int:
	var n := playlist.size()
	if n == 0:
		return 0
	for step in range(1, n + 1):
		var k := (pos + step) % n
		if allows(mood, playlist[k]):
			return k
	return (pos + 1) % n


## The first index the mood allows, starting at `from` (inclusive).
static func first_index(playlist: PackedStringArray, from: int, mood: String) -> int:
	var n := playlist.size()
	if n == 0:
		return 0
	for step in n:
		var k := (from + step) % n
		if allows(mood, playlist[k]):
			return k
	return from % n


## The mood for a view (the core's campaign view dictionary; its "colonies" are the player's own, so an entry without an owner counts). `lost_colony` is true when the player held more colonies in the
## previous view.
static func mood_for(view: Dictionary, player_id: int, lost_colony := false) -> String:
	var at_war := false
	for e in view.get("empires", []):
		if int(e.get("empire", e.get("id", 0))) != player_id and e.get("atWar", false):
			at_war = true
			break
	if not at_war:
		return "calm"
	if lost_colony:
		return "peak"
	var home: Array = []
	var systems := {}
	for s in view.get("systems", []):
		systems[int(s.get("id", 0))] = s
	for c in view.get("colonies", []):
		if int(c.get("owner", c.get("ownerId", player_id))) == player_id and systems.has(int(c.get("systemId", 0))):
			var s: Dictionary = systems[int(c["systemId"])]
			home.append(Vector2i(int(s.get("x", 0)), int(s.get("y", 0))))
	for f in view.get("contacts", []):
		if not f.get("atWar", false):
			continue
		var p := Vector2i(int(f.get("x", -999)), int(f.get("y", -999)))
		for h in home:
			if maxi(absi(p.x - h.x), absi(p.y - h.y)) <= THREAT_RANGE:
				return "peak"
	return "build"


static func player_colonies(view: Dictionary, player_id: int) -> int:
	var n := 0
	for c in view.get("colonies", []):
		if int(c.get("owner", c.get("ownerId", player_id))) == player_id:
			n += 1
	return n
