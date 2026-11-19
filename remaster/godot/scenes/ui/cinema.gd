class_name Cinema
extends Control
## MOD-9 (F4 round 3, 20 Nov 2026): the game's cinematics. One full-screen layer above everything, skipped by any key or
## click after a short grace:
##   - movies from the original disc (res://movies/<name>.ogv, built by tools/mod9/build_mod9.py, private, never committed):
##     "open" is the opening sequence at start-up, "fed" "kli" "rom" "car" "fer" the race intro when a campaign starts;
##   - the Dominion's race intro, drawn in-engine (the disc has none): its emblem, name and motto over the stars and
##     Weyoun's "The Dominion has endured..." line;
##   - the Borg cutscenes, drawn in-engine with the battle's own hull and backdrop: "arrival" (a cube or sphere drops out of
##     a transwarp conduit, the Collective's hail) and "assimilation" (the cube's green beam on one of the player's worlds,
##     Locutus).
## Lines come from the audio pack (vo.borg.hail, vo.borg.locutus, vo.victory.domination.dom) and their subtitles from
## shared/speeches.json. Missing media is skipped silently (the web build carries no movies).
## Presentation only: nothing here reads or changes the game state.

signal finished

const MOVIE_ROOT := "res://movies/"
const GRACE := 0.5
const BORG_LINES := {"arrival": "vo.borg.hail", "assimilation": "vo.borg.locutus"}

var kind := ""
var _t := 0.0
var _done := false
var _length := 0.0
var _player: VideoStreamPlayer = null
var _music_was_muted := false
var _scene: Node3D = null
var _cam: Camera3D = null
var _ship: Node3D = null
var _anim := {}


static func movie_path(name: String) -> String:
	return MOVIE_ROOT + name + ".ogv"


static func has_movie(name: String) -> bool:
	return ResourceLoader.exists(movie_path(name)) or FileAccess.file_exists(movie_path(name))


## The speeches table (shared pack): {key: {text, seconds, speaker}}. Empty when the pack has none.
static func speeches() -> Dictionary:
	var d: Variant = AppLoader.json("res://scripts/audio/pack/shared/speeches.json")
	return d.get("speeches", {}) if d is Dictionary and d.get("speeches") is Dictionary else {}


## The race intro for a new campaign as `empire` (1-5 from the disc, 6 drawn in-engine); null when there is none.
static func race_intro(host: Control, empire: int) -> Cinema:
	if empire == 6:
		return dominion(host)
	var tok: String = {1: "fed", 2: "kli", 3: "rom", 4: "car", 5: "fer"}.get(empire, "")
	return play_movie(host, tok) if tok != "" and has_movie(tok) else null


static func _make(host: Control, k: String) -> Cinema:
	var c := Cinema.new()
	c.kind = k
	c.name = "Cinema"
	c.set_anchors_preset(Control.PRESET_FULL_RECT)
	c.mouse_filter = Control.MOUSE_FILTER_STOP
	c.focus_mode = Control.FOCUS_ALL
	var bg := ColorRect.new()
	bg.color = Color.BLACK
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(bg)
	host.add_child(c)
	c.z_index = 100
	c._mute_music(true)
	return c


# ------------------------------------------------------------------------------------------------------------------
# Movies
# ------------------------------------------------------------------------------------------------------------------

static func play_movie(host: Control, name: String) -> Cinema:
	if not has_movie(name):
		return null
	var stream: VideoStream = load(movie_path(name)) as VideoStream
	if stream == null:
		return null
	var c := _make(host, "movie:" + name)
	var arc := AspectRatioContainer.new()
	arc.set_anchors_preset(Control.PRESET_FULL_RECT)
	arc.ratio = 800.0 / 300.0   # the disc's movies are 800x300, letterboxed on any screen
	arc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(arc)
	var p := VideoStreamPlayer.new()
	p.stream = stream
	p.expand = true
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	p.bus = "Master"
	arc.add_child(p)
	c._player = p
	c._add_skip_hint()
	p.finished.connect(c.finish)
	p.play()
	c._length = p.get_stream_length() if p.get_stream_length() > 0.0 else 120.0
	return c


# ------------------------------------------------------------------------------------------------------------------
# The Dominion's race intro
# ------------------------------------------------------------------------------------------------------------------

static func dominion(host: Control) -> Cinema:
	var c := _make(host, "dominion")
	var stars := ColorRect.new()
	stars.set_anchors_preset(Control.PRESET_FULL_RECT)
	var mat := ShaderMaterial.new()
	mat.shader = AppLoader.resource("res://scenes/galaxy/starfield.gdshader")
	stars.material = mat
	stars.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(stars)
	var tint := ColorRect.new()   # the Dominion's violet haze over the stars
	tint.color = Color(0.22, 0.08, 0.3, 0.35)
	tint.set_anchors_preset(Control.PRESET_FULL_RECT)
	tint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(tint)
	c._letterbox()
	var col := Lcars.vbox(18)
	col.set_anchors_preset(Control.PRESET_CENTER)
	col.grow_horizontal = Control.GROW_DIRECTION_BOTH
	col.grow_vertical = Control.GROW_DIRECTION_BOTH
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	col.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var em := Lcars.emblem(6, 220)
	em.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	col.add_child(em)
	var title := Lcars.label("THE DOMINION", "", 72, Color(0.86, 0.74, 1.0))
	title.add_theme_font_override("font", Lcars.font("display", 700))
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	col.add_child(title)
	var motto := Lcars.label("Order is the only peace.", "", 24, Color(0.78, 0.8, 0.9))
	motto.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	col.add_child(motto)
	col.modulate.a = 0.0
	c.add_child(col)
	var tw := c.create_tween()
	tw.tween_property(col, "modulate:a", 1.0, 1.6)
	c._length = c._voice_line("vo.victory.domination.dom", 11.0, 1.2) + 2.5
	c._add_skip_hint()
	return c


# ------------------------------------------------------------------------------------------------------------------
# Borg cutscenes
# ------------------------------------------------------------------------------------------------------------------

## kind "arrival" (asset key of the cube or sphere) or "assimilation" (caption names the world).
static func borg(host: Control, k: String, asset_key: String = "ship.borg.tactical_cube", caption: String = "") -> Cinema:
	var c := _make(host, "borg:" + k)
	var svc := SubViewportContainer.new()
	svc.set_anchors_preset(Control.PRESET_FULL_RECT)
	svc.stretch = true
	svc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(svc)
	var vp := SubViewport.new()
	vp.own_world_3d = true
	vp.msaa_3d = Viewport.MSAA_4X
	svc.add_child(vp)
	var root := Node3D.new()
	vp.add_child(root)
	c._scene = root
	var env := BattleEnvironment.build(root, Vector2(16, 12) * BattleScene.CELL, "GasGiant")
	var sun: DirectionalLight3D = env.sun
	sun.rotation_degrees = Vector3(-24.0, -52.0, 0.0)
	sun.light_energy = CompatLook.sun(2.0)
	var ship := BattleShip.new()
	root.add_child(ship)
	ship.setup({"id": 1, "owner": 7, "name": "Borg", "assetKey": asset_key, "maxHull": 300}, BattleAssets.shared())
	c._ship = ship
	var cam := Camera3D.new()
	cam.fov = 38.0
	cam.near = 0.5
	cam.far = 20000.0
	root.add_child(cam)
	cam.current = true
	c._cam = cam
	var green := Color(0.35, 1.0, 0.45)
	var l: float = maxf(ship.length, 4.0)
	var line_key: String = BORG_LINES.get(k, "vo.borg.hail")
	if k == "arrival":
		# A transwarp conduit opens (a green disc and flash ahead of the camera), the ship comes through it and slows.
		var conduit := MeshInstance3D.new()
		var quad := QuadMesh.new()
		quad.size = Vector2(l * 4.0, l * 4.0)
		conduit.mesh = quad
		# The conduit's mouth: a swirling green ring with a bright rim and a dark core, not a flat disc.
		var cm := ShaderMaterial.new()
		var sh := Shader.new()
		sh.code = """shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never;
uniform vec4 albedo_color = vec4(0.35, 1.0, 0.45, 0.0);
void fragment() {
	vec2 p = UV * 2.0 - 1.0;
	float r = length(p);
	float a = atan(p.y, p.x);
	float swirl = 0.55 + 0.45 * sin(a * 7.0 + r * 14.0 - TIME * 5.0);
	float ring = smoothstep(0.35, 0.82, r) * (1.0 - smoothstep(0.82, 1.0, r));
	float glow = ring * swirl + 0.25 * (1.0 - smoothstep(0.0, 1.0, r));
	ALBEDO = albedo_color.rgb * (0.6 + 1.2 * ring);
	ALPHA = clamp(glow * albedo_color.a, 0.0, 1.0);
}"""
		cm.shader = sh
		cm.set_shader_parameter("albedo_color", Color(green, 0.0))
		conduit.material_override = cm
		conduit.rotation_degrees = Vector3(0, 0, 0)
		conduit.position = Vector3(0, 0, -l * 6.0)
		root.add_child(conduit)
		var flash := OmniLight3D.new()
		flash.light_color = green
		flash.omni_range = l * 8.0
		flash.light_energy = 0.0
		flash.position = conduit.position + Vector3(0, l * 0.5, l)
		root.add_child(flash)
		ship.position = conduit.position
		ship.rotation_degrees = Vector3(0, 180, 0)   # bow (-Z) toward the camera
		ship.scale = Vector3.ONE * 0.02
		cam.position = Vector3(l * 0.35, l * 0.25, l * 2.4)
		cam.look_at(conduit.position, Vector3.UP)
		var tw := c.create_tween().set_parallel(true)
		var setc := func(a: float): cm.set_shader_parameter("albedo_color", Color(green, a))
		tw.tween_method(setc, 0.0, 1.0, 1.2).set_delay(0.4)
		tw.tween_property(flash, "light_energy", 6.0, 1.0).set_delay(0.6)
		tw.tween_property(ship, "scale", Vector3.ONE, 1.4).set_delay(1.6).set_trans(Tween.TRANS_EXPO).set_ease(Tween.EASE_OUT)
		tw.tween_property(ship, "position", Vector3(0, 0, -l * 1.2), 9.0).set_delay(1.6).set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
		tw.tween_method(setc, 1.0, 0.0, 1.6).set_delay(3.2)
		tw.tween_property(flash, "light_energy", 0.0, 2.0).set_delay(3.0)
		c._anim = {"look_from": conduit.position, "look_to": Vector3(0, 0, -l * 1.2), "cam_to": Vector3(-l * 0.55, l * 0.12, l * 0.9), "t0": 1.6, "t1": 14.0}
	else:
		# Over one of the player's worlds: the cube holds above the planet and its green beam reaches down.
		var planet := MeshInstance3D.new()
		var sm := SphereMesh.new()
		sm.radius = 1.0
		sm.height = 2.0
		sm.radial_segments = 96
		sm.rings = 48
		planet.mesh = sm
		planet.scale = Vector3.ONE * l * 2.2
		planet.position = Vector3(0, -l * 3.4, -l * 1.0)
		root.add_child(planet)
		# The system view's photoreal class-M set, lit by the scene's sun (an inhabited world: city lights on its night side).
		PlanetBody.attach(planet, "M", {"hi": true, "variant": 1, "inhabited": true, "sun_energy": 1.5,
			"sun_node": sun, "sun_dir": sun.global_transform.basis.z, "sun_color": Color(1.0, 0.93, 0.82)})
		ship.position = Vector3(0, l * 0.3, -l * 0.4)
		ship.rotation_degrees = Vector3(0, 140, 0)
		# The tractor beam (F4 polish, 20 Nov): a volume of light, not a flat cone. Two open cylinders (a wide sheath and a
		# bright core), drawn from both sides and brighter where the eye looks through more of them, with scan bands running
		# down, fine vertical striations and a flicker; where it meets the surface, a glow and rings spreading on the ground.
		var top_y: float = ship.position.y - l * 0.38
		var hit := Vector3(0, 0, ship.position.z - l * 0.25)
		var pr: float = l * 2.2
		var dz: float = hit.z - planet.position.z
		hit.y = planet.position.y + sqrt(maxf(pr * pr - dz * dz, 0.0))
		var beam_len: float = top_y - hit.y + l * 0.04
		var beam_mats: Array[ShaderMaterial] = []
		for layer in [[0.07, 0.36, 0.55, 1.0], [0.025, 0.12, 1.0, 2.2]]:
			var beam := MeshInstance3D.new()
			var bm := CylinderMesh.new()
			bm.top_radius = l * float(layer[0])
			bm.bottom_radius = l * float(layer[1])
			bm.height = beam_len
			bm.radial_segments = 48
			bm.rings = 8
			bm.cap_top = false
			bm.cap_bottom = false
			beam.mesh = bm
			var bsm := ShaderMaterial.new()
			bsm.shader = _beam_shader()
			bsm.set_shader_parameter("tint", green)
			bsm.set_shader_parameter("core", float(layer[2]))
			bsm.set_shader_parameter("boost", float(layer[3]))
			bsm.set_shader_parameter("intensity", 0.0)
			beam.material_override = bsm
			beam.position = Vector3(hit.x, hit.y - l * 0.04 + beam_len * 0.5, hit.z)
			root.add_child(beam)
			beam_mats.append(bsm)
		# Where it lands: rings spreading on the ground (a disc laid on the surface) and a soft glow facing the camera.
		var up_n: Vector3 = (hit - planet.position).normalized()
		var ground := MeshInstance3D.new()
		var gq := QuadMesh.new()
		gq.size = Vector2(l * 1.3, l * 1.3)
		ground.mesh = gq
		var gsm := ShaderMaterial.new()
		gsm.shader = _beam_ground_shader()
		gsm.set_shader_parameter("tint", green)
		gsm.set_shader_parameter("intensity", 0.0)
		ground.material_override = gsm
		ground.position = hit + up_n * l * 0.012
		ground.basis = Basis(Quaternion(Vector3.BACK, up_n))
		root.add_child(ground)
		beam_mats.append(gsm)
		var halo := MeshInstance3D.new()
		var hq := QuadMesh.new()
		hq.size = Vector2(l * 1.0, l * 1.0)
		halo.mesh = hq
		var hsm := ShaderMaterial.new()
		hsm.shader = _beam_halo_shader()
		hsm.set_shader_parameter("tint", green)
		hsm.set_shader_parameter("intensity", 0.0)
		halo.material_override = hsm
		halo.position = hit + up_n * l * 0.03
		root.add_child(halo)
		beam_mats.append(hsm)
		var glow := OmniLight3D.new()
		glow.light_color = green
		glow.omni_range = l * 1.6
		glow.light_energy = 0.0
		glow.position = hit + up_n * l * 0.25
		root.add_child(glow)
		var under := OmniLight3D.new()
		under.light_color = green
		under.omni_range = l * 1.2
		under.light_energy = 0.0
		under.position = Vector3(hit.x, top_y - l * 0.1, hit.z)
		root.add_child(under)
		cam.position = Vector3(l * 2.8, l * 0.1, l * 3.4)
		cam.look_at(ship.position + Vector3(0, -l * 0.6, 0), Vector3.UP)
		var tw := c.create_tween().set_parallel(true)
		for bmat in beam_mats:
			tw.tween_method(func(x: float): bmat.set_shader_parameter("intensity", x), 0.0, 1.0, 1.2).set_delay(1.4)
		tw.tween_property(glow, "light_energy", 4.0, 1.2).set_delay(1.4)
		tw.tween_property(under, "light_energy", 2.0, 1.2).set_delay(1.4)
		# The push-in keeps the cube and the beam's foot on the planet in frame to the end (the foot fell under the letterbox).
		c._anim = {"look_from": ship.position + Vector3(0, -l * 0.6, 0), "look_to": ship.position + Vector3(0, -l * 0.5, 0), "cam_to": Vector3(l * 2.7, -l * 0.2, l * 3.9), "t0": 0.0, "t1": 9.0}
	c._anim["cam_from"] = cam.position
	c._letterbox()
	var cap := caption if caption != "" else ("Borg vessel detected entering the quadrant" if k == "arrival" else "Assimilation")
	var head := Lcars.label(cap.to_upper(), "", 22, Color(0.55, 1.0, 0.6))
	head.set_anchors_preset(Control.PRESET_CENTER_TOP)
	head.grow_horizontal = Control.GROW_DIRECTION_BOTH
	head.offset_top = 36
	head.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	head.mouse_filter = Control.MOUSE_FILTER_IGNORE
	c.add_child(head)
	c._length = c._voice_line(line_key, 15.0 if k == "arrival" else 7.0, 1.4 if k == "arrival" else 1.0) + 2.0
	c._add_skip_hint()
	return c


## The assimilation beam's volume: brightest where the eye looks through the middle, a thin rim, scan bands sweeping down
## toward the planet, vertical striations and an uneven flicker. UV.y runs 0 at the ship to 1 at the ground.
static func _beam_shader() -> Shader:
	var sh := Shader.new()
	sh.code = """shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never, shadows_disabled;
uniform vec3 tint = vec3(0.35, 1.0, 0.45);
uniform float intensity = 0.0;
uniform float core = 0.55;
uniform float boost = 1.0;
float h(float x) { return fract(sin(x * 91.7) * 43758.5); }
void fragment() {
	float facing = abs(dot(normalize(NORMAL), normalize(VIEW)));
	float body = pow(facing, 2.0);
	float rim = pow(1.0 - facing, 4.0) * 0.35;
	float v = UV.y;
	float bands = pow(0.5 + 0.5 * sin(v * 46.0 - TIME * 11.0), 8.0);
	float sweep = smoothstep(0.07, 0.0, abs(fract(v * 0.8 - TIME * 0.55) - 0.5));
	float stri = 0.62 + 0.38 * sin(UV.x * 6.2832 * 11.0 + sin(TIME * 1.7 + v * 5.0) * 1.5);
	float t = floor(TIME * 24.0);
	float flick = 0.8 + 0.2 * h(t) + 0.08 * sin(TIME * 41.0);
	float ends = smoothstep(0.0, 0.1, v) * (0.75 + 0.25 * v);
	float a = (body * core + rim + (bands * 0.45 + sweep * 0.9) * body) * stri * flick * ends * intensity;
	ALBEDO = mix(tint, vec3(0.85, 1.0, 0.88), clamp(sweep * body * 0.7 + bands * 0.2, 0.0, 1.0)) * boost;
	ALPHA = clamp(a, 0.0, 1.0);
}"""
	return sh


## The ground under the beam: rings spreading out from the point it lands, over a bright pool.
static func _beam_ground_shader() -> Shader:
	var sh := Shader.new()
	sh.code = """shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never, shadows_disabled;
uniform vec3 tint = vec3(0.35, 1.0, 0.45);
uniform float intensity = 0.0;
void fragment() {
	float r = length(UV * 2.0 - 1.0);
	float pool = pow(1.0 - smoothstep(0.0, 0.55, r), 2.0);
	float rings = pow(0.5 + 0.5 * sin(r * 30.0 - TIME * 6.0), 6.0) * (1.0 - smoothstep(0.3, 1.0, r));
	ALBEDO = mix(tint, vec3(0.9, 1.0, 0.9), pool * 0.6) * 1.6;
	ALPHA = clamp((pool * 0.9 + rings * 0.55) * intensity, 0.0, 1.0);
}"""
	return sh


## A soft glow at the beam's foot, always facing the camera, pulsing with the flicker.
static func _beam_halo_shader() -> Shader:
	var sh := Shader.new()
	sh.code = """shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never, shadows_disabled;
uniform vec3 tint = vec3(0.35, 1.0, 0.45);
uniform float intensity = 0.0;
void vertex() {
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
}
void fragment() {
	float r = length(UV * 2.0 - 1.0);
	float g = pow(1.0 - smoothstep(0.0, 1.0, r), 3.0);
	float pulse = 0.85 + 0.15 * sin(TIME * 7.0);
	ALBEDO = mix(tint, vec3(1.0), g * 0.5) * 1.5;
	ALPHA = clamp(g * pulse * intensity, 0.0, 1.0);
}"""
	return sh


# ------------------------------------------------------------------------------------------------------------------
# Shared pieces
# ------------------------------------------------------------------------------------------------------------------

## Plays `key` after `delay` s and shows its subtitle on the letterbox. Returns when the line ends (s from now).
func _voice_line(key: String, fallback_seconds: float, delay: float) -> float:
	var sp: Dictionary = Cinema.speeches().get(key, {})
	var secs: float = float(sp.get("seconds", fallback_seconds))
	var text := str(sp.get("text", ""))
	if text != "":
		var sub := Lcars.label(text, "", 22, Color(0.93, 0.95, 1.0))
		sub.name = "Subtitle"
		sub.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		sub.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
		sub.grow_horizontal = Control.GROW_DIRECTION_BOTH
		sub.grow_vertical = Control.GROW_DIRECTION_BEGIN
		sub.custom_minimum_size.x = 1100
		sub.offset_left = -550
		sub.offset_right = 550
		sub.offset_bottom = -26
		sub.visible_ratio = 0.0
		sub.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(sub)
		var tw := create_tween()
		tw.tween_interval(delay)
		tw.tween_property(sub, "visible_ratio", 1.0, maxf(secs * 0.92, 0.5))
	get_tree().create_timer(delay).timeout.connect(func():
		if not _done and BotfAudioSystem.instance != null:
			BotfAudioSystem.instance.play_keys(PackedStringArray([key]), 10))
	return delay + secs


func _letterbox() -> void:
	for top in [true, false]:
		var bar := ColorRect.new()
		bar.color = Color.BLACK
		bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
		bar.anchor_left = 0.0
		bar.anchor_right = 1.0
		bar.anchor_top = 0.0 if top else 0.86
		bar.anchor_bottom = 0.14 if top else 1.0
		add_child(bar)


func _add_skip_hint() -> void:
	var hint := Lcars.label("Any key or click to skip", "", 14, Color(0.6, 0.64, 0.72, 0.7))
	hint.name = "SkipHint"
	hint.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	hint.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	hint.grow_vertical = Control.GROW_DIRECTION_BEGIN
	hint.offset_right = -20
	hint.offset_bottom = -12
	hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(hint)


func _mute_music(on: bool) -> void:
	var i := AudioServer.get_bus_index("Music")
	if i < 0:
		return
	if on:
		_music_was_muted = AudioServer.is_bus_mute(i)
		AudioServer.set_bus_mute(i, true)
	else:
		AudioServer.set_bus_mute(i, _music_was_muted)


func _process(delta: float) -> void:
	if _done:
		return
	_t += delta
	if _cam != null and not _anim.is_empty():
		var k := clampf((_t - float(_anim.t0)) / maxf(float(_anim.t1) - float(_anim.t0), 0.1), 0.0, 1.0)
		var e := k * k * (3.0 - 2.0 * k)
		_cam.position = (_anim.cam_from as Vector3).lerp(_anim.cam_to, e)
		var look := (_anim.look_from as Vector3).lerp(_anim.look_to, e)
		if _ship != null and kind == "borg:arrival" and _t > float(_anim.t0):
			look = look.lerp(_ship.position, 0.6)
		_cam.look_at(look, Vector3.UP)
	if _player == null and _length > 0.0 and _t >= _length:
		finish()


func _gui_input(event: InputEvent) -> void:
	if _skip_event(event):
		accept_event()
		finish()


func _unhandled_input(event: InputEvent) -> void:
	if _skip_event(event):
		get_viewport().set_input_as_handled()
		finish()


func _skip_event(event: InputEvent) -> bool:
	if _t < GRACE or _done:
		return false
	return (event is InputEventKey and event.pressed and not event.echo) or (event is InputEventMouseButton and event.pressed)


func finish() -> void:
	if _done:
		return
	_done = true
	if _player != null:
		_player.stop()
	if BotfAudioSystem.instance != null and BotfAudioSystem.instance._voice != null and kind != "movie:open":
		BotfAudioSystem.instance._voice.stop()   # a skipped line stops with its scene
	_mute_music(false)
	finished.emit()
	queue_free()
