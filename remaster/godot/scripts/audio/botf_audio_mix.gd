class_name BotfAudioMix
extends RefCounted
## Volume settings and the ducking math, separate from the nodes so they can be tested.
## Five sliders (0..1) live in the client settings under vol_<bus>: master, effects, voice, interface, music.
## While a voice line plays, Effects and Music dip to DUCK_FACTOR of their slider value (Unreal: 45 %).

const KEYS := {"Master": "vol_master", "Effects": "vol_effects", "Voice": "vol_voice", "Interface": "vol_interface", "Music": "vol_music"}
const DEFAULTS := {"vol_master": 0.8, "vol_effects": 0.9, "vol_voice": 1.0, "vol_interface": 0.8, "vol_music": 0.5}
const LABELS := {
	"Master": ["Master volume", "Everything the game plays."],
	"Effects": ["Effects", "Weapons, explosions, alarms and warp."],
	"Voice": ["Voice", "Advisor, diplomacy and victory voice lines."],
	"Interface": ["Interface", "Clicks, turn, research and construction cues."],
	"Music": ["Music", "Title, star chart and battle themes."],
}
const DUCKED := ["Effects", "Music"]
const DUCK_FACTOR := 0.45
const DUCK_ATTACK := 0.12   ## seconds to dip
const DUCK_RELEASE := 0.7   ## seconds to come back
const MIN_DB := -80.0


static func key_for(bus: String) -> String:
	return str(KEYS.get(bus, ""))


## Adds any missing audio setting to `settings` (and migrates the old single "volume" to the master slider).
static func fill_defaults(settings: Dictionary) -> void:
	if settings.has("volume") and not settings.has("vol_master"):
		settings["vol_master"] = clampf(float(settings["volume"]), 0.0, 1.0)
	settings.erase("volume")
	for k in DEFAULTS:
		if not settings.has(k):
			settings[k] = DEFAULTS[k]
		else:
			settings[k] = clampf(float(settings[k]), 0.0, 1.0)


static func volume_of(settings: Dictionary, bus: String) -> float:
	var k := key_for(bus)
	return clampf(float(settings.get(k, DEFAULTS.get(k, 1.0))), 0.0, 1.0)


## The slider position -> decibels. 0 is silent (-80 dB); the curve is the usual amplitude one.
static func to_db(linear: float) -> float:
	if linear <= 0.0005:
		return MIN_DB
	return maxf(linear_to_db(linear), MIN_DB)


## Gain applied to one bus: its slider, times the duck gain for ducked buses.
static func bus_gain(settings: Dictionary, bus: String, duck_gain: float) -> float:
	var g := volume_of(settings, bus)
	if DUCKED.has(bus):
		g *= clampf(duck_gain, 0.0, 1.0)
	return g


## The overall level of a bus as heard: its gain times the master slider (Master itself is just the slider).
static func heard_gain(settings: Dictionary, bus: String, duck_gain: float) -> float:
	var g := bus_gain(settings, bus, duck_gain)
	return g if bus == "Master" else g * volume_of(settings, "Master")


## Where the duck gain is heading: DUCK_FACTOR while a voice line plays, else 1.
static func duck_target(voice_active: bool) -> float:
	return DUCK_FACTOR if voice_active else 1.0


## One frame of the duck envelope: dips at DUCK_ATTACK, recovers at DUCK_RELEASE (full travel times).
static func duck_step(current: float, target: float, delta: float) -> float:
	var travel := 1.0 - DUCK_FACTOR
	if target < current:
		return maxf(target, current - travel * delta / DUCK_ATTACK)
	return minf(target, current + travel * delta / DUCK_RELEASE)


## Equal-power crossfade gains (out, in) for progress t in 0..1.
static func crossfade(t: float) -> Vector2:
	t = clampf(t, 0.0, 1.0)
	return Vector2(cos(t * PI * 0.5), sin(t * PI * 0.5))
