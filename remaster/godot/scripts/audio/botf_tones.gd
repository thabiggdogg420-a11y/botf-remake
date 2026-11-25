class_name BotfTones
extends RefCounted
## Generated fallback sounds, used when no local file resolves: short LCARS-style interface tones (never the red alert),
## and four looping ambient music beds (title, galaxy, battle, tension). Battle effects come from BattleAudio.synth.
## Original in-repo synthesis: no third-party audio.

const RATE := 22050
const MUSIC_RATE := 11025
const MUSIC_SECONDS := 8.0


## The tone kind for a sound key ("" when there is none, e.g. every vo.* key).
static func kind_for(key: String) -> String:
	if key.begins_with("vo.") or key.begins_with("mus."):
		return ""
	if key.begins_with("sfx.alert"):
		return ""   # Simon (8 and 25 Nov): no generated sound ever stands in for the red alert klaxon; a missing file is silence
	match key:
		"sfx.ui.click": return "click"
		"sfx.ui.open": return "open"
		"sfx.ui.close": return "close"
		"sfx.ui.endturn": return "endturn"
		"sfx.ui.turn": return "turn"
		"sfx.ui.research": return "research"
		"sfx.ui.construction": return "construction"
		"sfx.ui.beep": return "beep"
	if key.begins_with("sfx.ui."):
		return "beep"
	return BattleAudio.battle_kind_for(key)


static func synth(kind: String) -> AudioStreamWAV:
	match kind:
		"click": return _notes([[1800.0, 0.035]], 0.0, 0.5)
		"open": return _notes([[620.0, 0.05], [930.0, 0.09]], 0.0, 0.45)
		"close": return _notes([[930.0, 0.05], [620.0, 0.09]], 0.0, 0.4)
		"beep": return _notes([[1320.0, 0.12]], 0.0, 0.5)
		"endturn": return _notes([[523.0, 0.09], [784.0, 0.09], [1046.0, 0.22]], 0.0, 0.5)
		"turn": return _notes([[784.0, 0.16], [988.0, 0.34]], 0.003, 0.4)
		"research": return _notes([[659.0, 0.1], [831.0, 0.1], [988.0, 0.1], [1318.0, 0.36]], 0.004, 0.45)
		"construction": return _notes([[392.0, 0.12], [587.0, 0.4]], 0.0, 0.5)
	if kind in ["beam", "beam_d", "pulse", "pulse_d", "torpedo", "torpedo_d", "shield", "hull", "explosion", "cloak", "warp"]:
		return BattleAudio.synth(kind)
	return null


static func _wav(data: PackedByteArray, rate: int) -> AudioStreamWAV:
	var w := AudioStreamWAV.new()
	w.format = AudioStreamWAV.FORMAT_16_BITS
	w.mix_rate = rate
	w.stereo = false
	w.data = data
	return w


## A run of notes [[hz, seconds], ...] with a soft sine plus a harmonic; vibrato is the depth of a 7 Hz wobble.
static func _notes(notes: Array, vibrato: float, gain: float) -> AudioStreamWAV:
	var total := 0.0
	for n in notes:
		total += float(n[1])
	var count := int(total * RATE)
	var data := PackedByteArray()
	data.resize(count * 2)
	var i := 0
	var phase := 0.0
	for n in notes:
		var n_len := int(float(n[1]) * RATE)
		for k in n_len:
			if i >= count:
				break
			var u := float(k) / maxf(n_len, 1)
			var t := float(i) / RATE
			phase += TAU * float(n[0]) * (1.0 + vibrato * sin(t * 44.0)) / RATE
			var env := minf(float(k) / 90.0, 1.0) * pow(1.0 - u, 1.6)
			var s := (sin(phase) * 0.75 + sin(phase * 2.0) * 0.18) * env * gain
			data.encode_s16(i * 2, clampi(int(s * 30000.0), -32768, 32767))
			i += 1
	return _wav(data, RATE)


## A looping ambient bed for "title", "galaxy", "battle" or "tension" (8 s at 11 kHz, mono). Every partial is a whole
## number of cycles per loop, so the loop is seamless.
static func music(kind: String) -> AudioStreamWAV:
	var spec := _music_spec(kind)
	var count := int(MUSIC_SECONDS * MUSIC_RATE)
	var data := PackedByteArray()
	data.resize(count * 2)
	var partials: Array = spec.partials
	var pulses: int = spec.pulses
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(kind)
	var noise := 0.0
	for i in count:
		var u := float(i) / count
		var s := 0.0
		for p in partials:
			var cycles: float = roundf(float(p[0]) * MUSIC_SECONDS)   # whole cycles in the loop
			var swell: float = 0.65 + 0.35 * sin(TAU * (u * float(p[2]) + float(p[3])))
			s += sin(TAU * cycles * u + float(p[3])) * float(p[1]) * swell
		if pulses > 0:
			var beat := fmod(u * pulses, 1.0)
			s += sin(TAU * roundf(spec.pulse_hz * MUSIC_SECONDS) * u) * spec.pulse_gain * pow(1.0 - beat, 3.0)
		if spec.noise > 0.0:
			noise = lerpf(noise, rng.randf_range(-1.0, 1.0), 0.05)
			s += noise * spec.noise
		data.encode_s16(i * 2, clampi(int(s * 14000.0), -32768, 32767))
	var w := _wav(data, MUSIC_RATE)
	w.loop_mode = AudioStreamWAV.LOOP_FORWARD
	w.loop_begin = 0
	w.loop_end = count
	return w


## partials: [hz, amplitude, swell cycles per loop, phase]
static func _music_spec(kind: String) -> Dictionary:
	match kind:
		"title":   # open fifths over a deep D, slow swell
			return {"partials": [[73.4, 0.5, 1, 0.0], [110.0, 0.4, 2, 1.0], [146.8, 0.3, 1, 2.0], [220.0, 0.22, 2, 0.5],
				[293.7, 0.16, 3, 1.5], [440.0, 0.08, 1, 2.5]], "pulses": 0, "pulse_hz": 0.0, "pulse_gain": 0.0, "noise": 0.0}
		"galaxy":  # calm, wide and high, minor-ish
			return {"partials": [[65.4, 0.4, 1, 0.0], [98.0, 0.3, 1, 0.7], [130.8, 0.28, 2, 1.4], [155.6, 0.2, 3, 2.1],
				[261.6, 0.12, 2, 0.3], [392.0, 0.07, 3, 1.0]], "pulses": 0, "pulse_hz": 0.0, "pulse_gain": 0.0, "noise": 0.01}
		"battle":  # low drone with a slow heartbeat
			return {"partials": [[55.0, 0.5, 1, 0.0], [82.4, 0.32, 2, 0.4], [110.0, 0.22, 1, 1.0], [164.8, 0.12, 3, 2.0]],
				"pulses": 8, "pulse_hz": 55.0, "pulse_gain": 0.5, "noise": 0.012}
		"tension":  # dissonant seconds and a fast pulse
			return {"partials": [[55.0, 0.5, 1, 0.0], [58.3, 0.4, 2, 0.5], [110.0, 0.2, 1, 1.0], [116.5, 0.2, 3, 1.5], [233.0, 0.1, 2, 2.0]],
				"pulses": 16, "pulse_hz": 82.5, "pulse_gain": 0.6, "noise": 0.02}
	return {"partials": [[110.0, 0.4, 1, 0.0]], "pulses": 0, "pulse_hz": 0.0, "pulse_gain": 0.0, "noise": 0.0}
