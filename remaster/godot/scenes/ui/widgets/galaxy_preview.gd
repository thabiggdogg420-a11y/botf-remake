class_name GalaxyPreview
extends Control
## The New campaign galaxy preview (docs/UI-DESIGN-SYSTEM.md §5, WS02 §2): a star field in the chosen shape
## (spiral, ring, irregular, elliptical) and density, seeded, with homeworld markers in empire colours and minor-race
## markers. It shows the option set; the core's generator places the real systems.

var shape := "spiral"
var seed_value := 1
var systems := 50
var empires := 5
var minors := 6
var player := 1
var _rot := 0.0
var _stars: Array = []   ## [Vector2 unit-disc position, brightness, kind (0 star, 1 home, 2 minor), empire]

func configure(shape_: String, seed_: int, systems_: int, empires_: int, minors_: int, player_: int) -> void:
	shape = shape_
	seed_value = seed_
	systems = systems_
	empires = empires_
	minors = minors_
	player = player_
	_generate()
	queue_redraw()

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	if _stars.is_empty():
		_generate()
	set_process(not UiTokens.reduced_motion)

func _process(delta: float) -> void:
	_rot = fmod(_rot + delta * TAU / 120.0, TAU)
	queue_redraw()

func _point(rng: RandomNumberGenerator) -> Vector2:
	match shape:
		"ring":
			var a := rng.randf() * TAU
			var r := rng.randfn(0.72, 0.08)
			return Vector2(cos(a), sin(a)) * r
		"irregular":
			var c := Vector2(rng.randfn(0, 0.35), rng.randfn(0, 0.3))
			c += Vector2(sin(c.y * 5.0 + seed_value), cos(c.x * 4.0)) * 0.12
			return c
		"elliptical":
			var a2 := rng.randf() * TAU
			var r2 := sqrt(rng.randf()) * 0.85
			return Vector2(cos(a2) * r2, sin(a2) * r2 * 0.6)
		_:
			var arm := rng.randi_range(0, 2)
			var t := pow(rng.randf(), 0.8)
			var a3 := t * 3.4 + arm * TAU / 3.0
			var r3 := 0.08 + t * 0.85
			return Vector2(cos(a3), sin(a3)) * r3 + Vector2(rng.randfn(0, 0.05), rng.randfn(0, 0.05))

func _generate() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_value * 31 + hash(shape)
	_stars.clear()
	# dust: 30 background points per system (visual density), then the systems themselves.
	for n in systems * 40:
		_stars.append([_point(rng), rng.randf_range(0.08, 0.35), -1, 0])
	for n in systems:
		_stars.append([_point(rng), rng.randf_range(0.6, 1.0), 0, 0])
	for e in range(1, empires + 1):
		var a := TAU * (e - 1) / float(empires) + 0.4
		var r := 0.62 if shape != "ring" else 0.72
		_stars.append([Vector2(cos(a), sin(a)) * r * (0.9 + rng.randf() * 0.1), 1.0, 1, e])
	for n in minors:
		_stars.append([_point(rng), 0.9, 2, 0])

func _draw() -> void:
	var c := size / 2.0
	var tilt := 0.6
	var R: float = min(size.x * 0.47, size.y / tilt * 0.46)
	# core glow
	for k in 10:
		draw_circle(c, R * (0.36 - k * 0.032), Color(0.95, 0.82, 0.6, 0.03 if shape != "ring" else 0.006))
	for s in _stars:
		var p: Vector2 = s[0]
		var rp := Vector2(p.x * cos(_rot) - p.y * sin(_rot), p.x * sin(_rot) + p.y * cos(_rot))
		var sp := c + Vector2(rp.x, rp.y * tilt) * R
		match int(s[2]):
			-1:
				draw_rect(Rect2(sp, Vector2(2, 2)), Color(0.72, 0.8, 1.0, s[1] * 1.3))
			0:
				draw_circle(sp, 4.0, Color(1.0, 0.9, 0.7, s[1] * 0.18))
				draw_circle(sp, 2.0, Color(1.0, 0.96, 0.88, s[1]))
			1:
				var col := Lcars.owner_color(int(s[3]))
				draw_circle(sp, 14.0, Color(col, 0.16))
				draw_circle(sp, 5.5, col)
				if int(s[3]) == player:
					draw_arc(sp, 17.0, 0, TAU, 32, col, 2.0, true)
			2:
				draw_arc(sp, 4.0, 0, TAU, 12, Color(0.9, 0.9, 0.95, 0.85), 1.5, true)
