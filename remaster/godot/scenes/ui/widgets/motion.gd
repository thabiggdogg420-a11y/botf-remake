class_name Motion
extends RefCounted
## Motion tokens applied (docs/UI-DESIGN-SYSTEM.md §4). Fades only touch `modulate`, so layout and input are live
## from the first frame; reduced motion makes every duration 0.

## Fades `c` in after `order` stagger steps (at most 6).
static func enter(c: CanvasItem, order: int = 0, token: String = "panel") -> void:
	var d := UiTokens.dur(token)
	if d <= 0.0 or c == null:
		return
	c.modulate.a = 0.0
	var tw := c.create_tween()
	tw.tween_interval(UiTokens.dur("stagger") * mini(order, 6))
	tw.tween_property(c, "modulate:a", 1.0, d).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)

## Staggers the children of a container.
static func stagger(parent: Node) -> void:
	var n := 0
	for ch in parent.get_children():
		if ch is CanvasItem:
			enter(ch, n)
			n += 1
