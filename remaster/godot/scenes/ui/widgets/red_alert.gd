class_name RedAlert
extends Control
## The pulsing red-alert bar.

func _process(_d: float) -> void:
	queue_redraw()

func _draw() -> void:
	var t := Time.get_ticks_msec() / 1000.0
	var a: float = 0.45 + 0.55 * absf(sin(t * 3.0))
	draw_rect(Rect2(Vector2.ZERO, size), Color(Lcars.ALERT, a))
