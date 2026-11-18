class_name HailViewscreen
extends Control
## The hail scene (TREK-PASS item 7): a comm viewscreen with the power's leader as a silhouette bust (HailBust), backlit
## in the empire's colours, its insignia in the corner, the channel line and the leader's name strip.
## Where the private portrait pack holds a picture for the power (PortraitSlot keys, Simon's 16 Nov portraits) that
## picture fills the screen instead (in a Mirror game the Mirror character's picture, else the normal one); the public
## build and any power without a picture get the bust.

var empire := 0
var key := ""            ## HailBust key ("" = from empire and the active pack)
var art_key := ""        ## PortraitSlot art key ("" = the power's own); minors pass theirs
var caption := ""
var status := ""         ## the channel line ("Channel open", "At war", ...)
var live := true
var force_bust := false  ## capture and tests: always draw the bust
var _tex: Texture2D
var _t := 0.0
var _waiting := false    ## web, cold visit: the pack holding the portrait has not landed yet; the screen shows static, not the bust
var _gave_up := false    ## the pack failed to load, held no portrait for this power, or the timeout passed: the bust is drawn
## How long a hail waits for its art pack before it falls back to the bust (seconds).
const WAIT_TIMEOUT := 20.0

static func make(empire_: int, px: Vector2, caption_: String = "", status_: String = "", art_key_: String = "") -> HailViewscreen:
	var h := HailViewscreen.new()
	h.empire = empire_
	h.caption = caption_
	h.status = status_
	h.art_key = art_key_
	h.custom_minimum_size = px
	h.mouse_filter = Control.MOUSE_FILTER_IGNORE
	h.clip_contents = true
	return h

func bust_key() -> String:
	if key != "":
		return key
	return HailBust.key_for(empire, HailBust.mirror_active())

## The portrait pack keys tried in order (Simon, 18 Nov: hail scenes show the character portrait for every power, Mirror
## games included). Under the Mirror pack the power's Mirror character comes first (portrait.terran, portrait.mirror.kli,
## ...), then its normal portrait; the bust is drawn only when the pack holds neither (always in the public repo).
func portrait_keys() -> Array[String]:
	var out: Array[String] = []
	if art_key != "":
		out.append(art_key)
		return out
	if HailBust.mirror_active() and PortraitSlot.MAJOR_KEYS.has(empire):
		out.append("portrait." + HailBust.key_for(empire, true))
	var k := str(PortraitSlot.MAJOR_KEYS.get(empire, ""))
	if k != "":
		out.append(k)
	return out

func shows_bust() -> bool:
	return _tex == null and not _waiting

## What the screen is actually drawing right now: "picture", "static" (the portrait is on its way, the viewscreen shows the
## incoming-transmission static) or "bust". Capture logs and tests report this, not what was intended.
func drawn() -> String:
	if _tex != null:
		return "picture"
	return "static" if _waiting else "bust"

func _find_portrait() -> void:
	for ak in portrait_keys():
		_tex = AppLoader.art(ak, "portrait")
		if _tex:
			break

func _ready() -> void:
	if not force_bust:
		_find_portrait()
		if _tex == null and AppLoader.portrait_pack_pending() and not portrait_keys().is_empty():
			_start_waiting()
	set_process((live and not UiTokens.reduced_motion) or _waiting)

## Web, first visit: the portraits sit in art_web.pck, still downloading. Raise that pack to the front of the queue, show the
## static until it mounts, and use the bust only if the pack fails, holds no portrait for this power, or the timeout passes.
func _start_waiting() -> void:
	_waiting = true
	AppLoader.art_events.packs_mounted.connect(_on_packs_mounted)
	AppLoader.raise_pack(AppLoader.PORTRAIT_PACK, _on_pack_done)
	get_tree().create_timer(WAIT_TIMEOUT).timeout.connect(_on_pack_done)

func _on_packs_mounted(_path: String = "") -> void:
	if not _waiting:
		return
	_find_portrait()
	if _tex != null:
		_stop_waiting()

## The fetch finished (loaded or failed) or the timeout passed: one last look for the picture, else the bust.
func _on_pack_done() -> void:
	if not _waiting or not is_inside_tree():
		return
	_find_portrait()
	_gave_up = _tex == null
	_stop_waiting()

func _stop_waiting() -> void:
	_waiting = false
	if AppLoader.art_events.packs_mounted.is_connected(_on_packs_mounted):
		AppLoader.art_events.packs_mounted.disconnect(_on_packs_mounted)
	set_process(live and not UiTokens.reduced_motion)
	queue_redraw()

func _process(delta: float) -> void:
	_t = fmod(_t + delta * 0.22, 1.0)
	queue_redraw()

## The "incoming transmission" static: banded noise and rolling bars in the empire's colour, no face.
func _draw_static(inner: Rect2, col: Color) -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = int(Time.get_ticks_msec() / 70) * 2654435761 + empire
	var step := 6.0
	var y := inner.position.y
	while y < inner.end.y:
		var x := inner.position.x
		while x < inner.end.x:
			var v := rng.randf()
			draw_rect(Rect2(x, y, minf(step, inner.end.x - x), minf(step, inner.end.y - y)), Color(col.lerp(Color.WHITE, 0.55), v * v * 0.55))
			x += step
		y += step
	var by := fmod(_t * 3.0, 1.0) * inner.size.y
	draw_rect(Rect2(inner.position.x, inner.position.y + by, inner.size.x, 18), Color(col.lightened(0.5), 0.18))
	var f := Lcars.font("display", 600)
	var msg := "INCOMING TRANSMISSION"
	draw_string(f, Vector2(inner.position.x, inner.position.y + inner.size.y * 0.5), msg, HORIZONTAL_ALIGNMENT_CENTER, inner.size.x, 20, Color(col.lightened(0.55), 0.9))

func _draw() -> void:
	var r := Rect2(Vector2.ZERO, size)
	var col := Lcars.owner_color(empire) if empire > 0 else Lcars.accent()
	var rad := float(UiTokens.radius())
	var bg := StyleBoxFlat.new()
	bg.bg_color = Color(0.01, 0.012, 0.02)
	bg.set_corner_radius_all(int(rad))
	draw_style_box(bg, r)
	var inner := r.grow(-3)
	if _tex:
		draw_texture_rect_region(_tex, inner, PortraitSlot.cover_src(_tex.get_size(), inner.size))
	elif _waiting:
		_draw_static(inner, col)
	else:
		# the backlight: a soft disc of the empire's colour behind the head, then faint stars
		var c := Vector2(size.x * 0.5, size.y * 0.42)
		for k in 10:
			var rr: float = maxf(size.x, size.y) * (0.75 - k * 0.065)
			draw_circle(c, rr, Color(col.darkened(0.35), 0.035 + k * 0.012))
		var rng := RandomNumberGenerator.new()
		rng.seed = 7919 * (empire + 3)
		for i in 40:
			var p := Vector2(rng.randf() * size.x, rng.randf() * size.y * 0.7)
			draw_circle(p, rng.randf_range(0.5, 1.3), Color(1, 1, 1, rng.randf_range(0.15, 0.45)))
		var bh := size.y * 0.92
		HailBust.draw(self, bust_key(), Rect2(Vector2((size.x - bh) * 0.5, size.y - bh), Vector2(bh, bh)), col.lightened(0.25))
	# insignia in the top corner
	var ins := minf(size.x, size.y) * 0.16
	Insignia.draw(self, empire if empire > 0 else "neutral", Rect2(size.x - ins - 10, 10, ins, ins), Color(col.lightened(0.2), 0.8))
	# scan lines and the sweep
	for y in range(3, int(size.y) - 3, 3):
		draw_line(Vector2(3, y), Vector2(size.x - 3, y), Color(0, 0, 0, 0.16), 1.0)
	if live and not UiTokens.reduced_motion:
		draw_rect(Rect2(3, size.y * _t, size.x - 6, 4), Color(col.lightened(0.5), 0.08))
	var fr := StyleBoxFlat.new()
	fr.draw_center = false
	fr.border_color = Color(col, 0.85)
	fr.set_border_width_all(3)
	fr.set_corner_radius_all(int(rad))
	fr.anti_aliasing = true
	draw_style_box(fr, r)
	var f := Lcars.font("display", 600)
	if status != "":
		draw_rect(Rect2(10, 10, minf(size.x * 0.6, 12 + status.length() * 9.0), 22), Color(0, 0, 0, 0.6))
		draw_string(f, Vector2(16, 26), status.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, size.x * 0.6, 14, Color(col.lightened(0.45), 0.95))
	if caption != "":
		draw_rect(Rect2(0, size.y - 28, size.x, 28), Color(0, 0, 0, 0.62))
		draw_string(f, Vector2(12, size.y - 9), caption.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, size.x - 24, 16, col.lightened(0.3))
