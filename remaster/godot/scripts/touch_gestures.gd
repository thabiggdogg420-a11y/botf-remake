class_name TouchGestures
extends RefCounted
## Finger tracking for the phone mode: long-press (a held finger that has not moved) and pinch (two fingers). Pure logic: it is
## fed InputEventScreenTouch / InputEventScreenDrag and a clock, and emits signals; PlatformRuntime turns them into tooltips
## and map zoom. A tap needs nothing here: the engine turns a touch into a mouse click (Input.emulate_mouse_from_touch).

signal long_press(pos: Vector2)
signal pinch(factor: float, center: Vector2)
signal pinch_started(center: Vector2)

const LONG_PRESS_SEC := 0.5
const MOVE_SLOP := 14.0   ## a finger that moves further than this is a drag, not a press

var _fingers := {}        ## index -> {pos, start, held}
var _fired := false       ## the long-press for the current touch has fired
var _pinch_dist := 0.0

func feed(event: InputEvent) -> void:
	var t := event as InputEventScreenTouch
	if t != null:
		if t.pressed:
			_fingers[t.index] = {"pos": t.position, "start": t.position, "held": 0.0}
			if _fingers.size() == 1:
				_fired = false
			elif _fingers.size() == 2:
				_pinch_dist = _two_dist()
				pinch_started.emit(_two_center())
		else:
			_fingers.erase(t.index)
			_pinch_dist = 0.0
			if _fingers.is_empty():
				_fired = false
		return
	var d := event as InputEventScreenDrag
	if d != null and _fingers.has(d.index):
		_fingers[d.index]["pos"] = d.position
		if _fingers.size() == 2 and _pinch_dist > 1.0:
			var nd := _two_dist()
			if nd > 1.0:
				pinch.emit(nd / _pinch_dist, _two_center())
				_pinch_dist = nd

## Advance the clock: a single finger held still for LONG_PRESS_SEC fires long_press once.
func tick(delta: float) -> void:
	if _fingers.size() != 1 or _fired:
		return
	var f: Dictionary = _fingers.values()[0]
	if (f["pos"] as Vector2).distance_to(f["start"]) > MOVE_SLOP:
		f["held"] = -1e9   # it moved: a drag, never a press
		return
	f["held"] += delta
	if f["held"] >= LONG_PRESS_SEC:
		_fired = true
		long_press.emit(f["pos"])

func finger_count() -> int:
	return _fingers.size()

func is_pinching() -> bool:
	return _fingers.size() >= 2

func fired() -> bool:
	return _fired

func _two_pts() -> Array:
	var keys := _fingers.keys()
	keys.sort()
	return [_fingers[keys[0]]["pos"], _fingers[keys[1]]["pos"]]

func _two_dist() -> float:
	var p := _two_pts()
	return (p[0] as Vector2).distance_to(p[1])

func _two_center() -> Vector2:
	var p := _two_pts()
	return ((p[0] as Vector2) + (p[1] as Vector2)) * 0.5
