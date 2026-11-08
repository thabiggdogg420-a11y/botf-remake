class_name ShipSilhouette
extends Control
## A procedural top-down hull silhouette (no studio artwork), seeded by the design id.
var seed_id := 0
var color := Color.WHITE
func _draw() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = seed_id * 7919 + 13
	var c := Vector2(size.x / 2.0, size.y / 2.0)
	var L: float = min(size.x * 0.5, 320.0)
	var W: float = size.y * 0.32
	draw_rect(Rect2(Vector2.ZERO, size), Color(Lcars.BG2, 0.6))
	for k in range(0, int(size.x), 24):
		draw_line(Vector2(k, 0), Vector2(k, size.y), Color(1, 1, 1, 0.03))
	var saucer := rng.randf() < 0.5
	var col := Color(color, 0.85)
	if saucer:
		draw_circle(c + Vector2(-L * 0.25, 0), W * 0.9, Color(col, 0.25))
		draw_arc(c + Vector2(-L * 0.25, 0), W * 0.9, 0, TAU, 48, col, 2.0, true)
		draw_rect(Rect2(c + Vector2(-L * 0.1, -W * 0.12), Vector2(L * 0.45, W * 0.24)), Color(col, 0.35))
		for sgn in [-1, 1]:
			var p := c + Vector2(L * 0.15, sgn * W * 0.7)
			draw_rect(Rect2(p, Vector2(L * 0.4, W * 0.14)), Color(col, 0.5))
			draw_line(c + Vector2(L * 0.1, 0), p + Vector2(L * 0.05, 0), col, 2.0)
	else:
		var pts := PackedVector2Array([c + Vector2(-L * 0.45, 0), c + Vector2(-L * 0.1, -W * rng.randf_range(0.3, 0.6)), c + Vector2(L * 0.3, -W * rng.randf_range(0.8, 1.1)),
			c + Vector2(L * 0.4, -W * 0.2), c + Vector2(L * 0.4, W * 0.2), c + Vector2(L * 0.3, W * rng.randf_range(0.8, 1.1)), c + Vector2(-L * 0.1, W * rng.randf_range(0.3, 0.6))])
		draw_colored_polygon(pts, Color(col, 0.25))
		pts.append(pts[0])
		draw_polyline(pts, col, 2.0, true)
	draw_line(c + Vector2(-L * 0.5, 0), c + Vector2(L * 0.5, 0), Color(col, 0.2), 1.0)
