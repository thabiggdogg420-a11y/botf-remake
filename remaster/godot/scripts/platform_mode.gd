class_name PlatformMode
extends RefCounted
## Platform mode: "desktop" or "mobile" (onboarding-and-mobile plan section 2, stage M-A). One build serves both; the mode only
## changes how the UI is scaled and fed (bigger UI scale, touch gestures, long-press tooltips, no bloom, adaptive render scale).
## Desktop mode is the game exactly as it was: every mobile branch is behind is_mobile().
##
## Where the mode comes from, in order:
##   web: the host page sets globalThis.BOTF_MODE before the engine starts (web-host/botf-mode.js: ?mode= query, then the
##        remembered switch, then touch + narrow screen). If the page did not (an old host page), the same decision is made here
##        from the same inputs through decide().
##   PC exe: --mobile / --desktop (or --platform-mode=mobile) on the command line, the BOTF_MODE environment variable, then the
##        "platformMode" setting ("auto" = desktop; "mobile" / "desktop"), set from Settings.
## decide() is the single reference for the web rule; fixtures/platform-mode-cases.json holds the cases both it and the page's
## JavaScript are tested against.

const MODES := ["desktop", "mobile"]
## "Narrow" for the auto rule: the screen's shorter side, in CSS pixels, at or below this (phones; tablets stay desktop).
const NARROW_MAX := 640
## The size a 1080-line base layout is shown at on a phone: text of 14 base px reads as about 9 CSS px, 18 as 12 (UI_TARGET * base).
const UI_TARGET := 0.66
const UI_FACTOR_MAX := 3.0

static var _mode := ""
static var _source := ""
static var _setting := "auto"
## The slow-frame render scale cap (AdaptiveRender lowers it); DisplaySettings.apply_viewport honours it.
static var render_cap := 1.0
## Test hook: true makes a web-less run read the page's values from web_inputs instead of JavaScript.
static var web_inputs := {}


## The rule: an explicit ?mode= query wins, then the remembered choice, then touch plus a narrow screen means mobile.
## `short_side` is the shorter side of the screen in CSS pixels (0 = unknown, never narrow).
static func decide(query: String, stored: String, touch: bool, short_side: int) -> Dictionary:
	var q := query.strip_edges().to_lower()
	if MODES.has(q):
		return {"mode": q, "source": "query"}
	var s := stored.strip_edges().to_lower()
	if MODES.has(s):
		return {"mode": s, "source": "stored"}
	if touch and short_side > 0 and short_side <= NARROW_MAX:
		return {"mode": "mobile", "source": "auto"}
	return {"mode": "desktop", "source": "auto"}


static func is_mobile() -> bool:
	return mode() == "mobile"


static func mode() -> String:
	if _mode == "":
		_resolve()
	return _mode


## "query", "stored", "auto", "cli", "env", "setting" or "forced": where the current mode came from.
static func source() -> String:
	mode()
	return _source


## Reads the PC setting (App calls this once the settings file is loaded) and settles the mode.
static func init_from(settings: Dictionary) -> void:
	_setting = str(settings.get("platformMode", "auto")).to_lower()
	_mode = ""
	_resolve()


## The mode for tests and tools: skips every source.
static func force(m: String) -> void:
	_mode = m if MODES.has(m) else "desktop"
	_source = "forced"


static func reset() -> void:
	_mode = ""
	_source = ""
	_setting = "auto"
	render_cap = 1.0
	web_inputs = {}


static func _resolve() -> void:
	if OS.has_feature("web") or not web_inputs.is_empty():
		var d := _from_web()
		_mode = d["mode"]
		_source = d["source"]
		return
	var cli := from_args(OS.get_cmdline_args())
	if cli != "":
		_mode = cli
		_source = "cli"
		return
	var env := OS.get_environment("BOTF_MODE").to_lower()
	if MODES.has(env):
		_mode = env
		_source = "env"
		return
	if MODES.has(_setting):
		_mode = _setting
		_source = "setting"
		return
	_mode = "desktop"
	_source = "auto"


## "mobile" / "desktop" from --mobile, --desktop or --platform-mode=<m> in an argument list, else "" (the last one wins).
static func from_args(args: Array) -> String:
	var out := ""
	for a in args:
		var s := str(a).to_lower()
		if s == "--mobile":
			out = "mobile"
		elif s == "--desktop":
			out = "desktop"
		elif s.begins_with("--platform-mode="):
			var v := s.substr(len("--platform-mode="))
			if MODES.has(v):
				out = v
	return out


static func _from_web() -> Dictionary:
	if not web_inputs.is_empty():
		return decide(str(web_inputs.get("query", "")), str(web_inputs.get("stored", "")), bool(web_inputs.get("touch", false)), int(web_inputs.get("short", 0)))
	var page := str(JavaScriptBridge.eval("globalThis.BOTF_MODE || ''"))
	if MODES.has(page):
		return {"mode": page, "source": str(JavaScriptBridge.eval("globalThis.BOTF_MODE_SRC || 'page'"))}
	# The host page did not decide (an older page): the same inputs, the same rule.
	var query := str(JavaScriptBridge.eval("new URLSearchParams(location.search).get('mode') || ''"))
	var stored := str(JavaScriptBridge.eval("(function(){try{return localStorage.getItem('botf-mode')||''}catch(e){return ''}})()"))
	var touch := int(JavaScriptBridge.eval("(navigator.maxTouchPoints||0) > 0 || ('ontouchstart' in window) ? 1 : 0")) == 1
	var short_side := int(JavaScriptBridge.eval("Math.min(screen.width, screen.height) || 0"))
	return decide(query, stored, touch, short_side)


# ---------------------------------------------------------------------------------------------
# What the mode changes
# ---------------------------------------------------------------------------------------------

## The extra content scale a phone needs so base text reads: `window_h` is the window height in device pixels, `dpr` the
## device pixel ratio (the browser's canvas is CSS size x dpr). Never below 1, so a big window in mobile mode is unchanged.
static func ui_factor(window_h: float, dpr: float = 1.0) -> float:
	if window_h <= 0.0:
		return 1.0
	var css_h := window_h / maxf(dpr, 0.5)
	return clampf(UI_TARGET * 1080.0 / css_h, 1.0, UI_FACTOR_MAX)


static var _user_scale := 1.0
static var _hooked := {}

## Sets the window's content scale: the player's own scale on desktop (as before), times the phone factor and with the wide
## screen's extra width given to the layout (not letterbox bars) on mobile. Re-applied when the window turns or resizes.
static func apply_window(win: Window, user_scale: float) -> void:
	if win == null:
		return
	_user_scale = user_scale
	if not is_mobile():
		win.content_scale_factor = user_scale
		return
	var dpr := 1.0
	if DisplayServer.get_name() != "headless":
		dpr = maxf(DisplayServer.screen_get_scale(DisplayServer.window_get_current_screen()), 1.0) if OS.has_feature("web") else 1.0
	win.content_scale_aspect = Window.CONTENT_SCALE_ASPECT_EXPAND
	win.content_scale_factor = user_scale * ui_factor(float(win.size.y), dpr)
	Input.emulate_mouse_from_touch = true   # a tap is a click, everywhere
	var id := win.get_instance_id()
	if not _hooked.has(id):
		_hooked[id] = true
		win.size_changed.connect(func() -> void:
			if is_mobile():
				apply_window(win, _user_scale))


## Adds or removes the running half (gestures, long-press tooltips, adaptive render scale) under `app` to match the mode.
static func sync_runtime(app: Node) -> void:
	var have := app.get_node_or_null("PlatformRuntime")
	if is_mobile() and have == null:
		var r := PlatformRuntime.new()
		r.app = app
		app.add_child(r)
	elif not is_mobile() and have != null:
		have.queue_free()
		render_cap = 1.0


## Phone mode, the top bar: puts a horizontal scroller (taking the free width) into `row` and returns the HBox inside it, so the
## figures scroll while what follows the scroller in `row` (palette, Menu, End turn) stays in view.
static func scroll_row(row: HBoxContainer) -> HBoxContainer:
	var sc := ScrollContainer.new()
	sc.name = "StatsScroll"
	sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_SHOW_NEVER
	sc.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	sc.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sc.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var inner := HBoxContainer.new()
	inner.add_theme_constant_override("separation", 26)
	inner.size_flags_vertical = Control.SIZE_EXPAND_FILL
	sc.add_child(inner)
	row.add_child(sc)
	return inner


## Phone mode, the rail: puts `col` in a vertical scroller placed at `rect` and returns the scroller (the caller adds it).
static func scroll_column(col: Control, rect: Rect2) -> ScrollContainer:
	var sc := ScrollContainer.new()
	sc.name = "RailScroll"
	sc.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	sc.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_SHOW_NEVER
	sc.position = rect.position
	sc.size = rect.size
	col.position = Vector2.ZERO
	sc.add_child(col)
	return sc


## Screens that stay exactly the size of the stage: the star chart and the battle take every drag themselves.
const FIXED_SCREENS := ["galaxy", "combat"]

## How far a screen's content reaches (right and bottom edge, in the screen's own pixels). Walks what is drawn, laid out; an inner
## ScrollContainer counts as its own rectangle (what scrolls inside it is no overflow) except for the width inside one that cannot
## scroll sideways.
static func content_reach(node: Control, offset := Vector2.ZERO) -> Vector2:
	var reach := Vector2.ZERO
	for ch in node.get_children():
		var c := ch as Control
		if c == null or not c.visible:
			continue
		var at := offset + c.position
		reach = Vector2(maxf(reach.x, at.x + c.size.x), maxf(reach.y, at.y + c.size.y))
		if c is ScrollContainer:
			# A scroller that cannot scroll sideways clips what is wider than it: that width is overflow. Its height is not.
			if (c as ScrollContainer).horizontal_scroll_mode == ScrollContainer.SCROLL_MODE_DISABLED:
				reach.x = maxf(reach.x, content_reach(c, at).x)
		elif not c.clip_contents:
			var inner := content_reach(c, at)
			reach = Vector2(maxf(reach.x, inner.x), maxf(reach.y, inner.y))
	return reach


## Phone mode: the 1080-line screens need more room than a phone window has even at the larger UI scale. A screen whose content
## is wider or taller than the stage is put in a pan wrapper (drag to scroll, both ways) instead of running off the window.
## Measured two frames after the screen is placed, once its containers have laid out. Desktop never gets here.
static func fit_overflow(stage: Node, screen: Control, id: String) -> void:
	if not is_mobile() or FIXED_SCREENS.has(id) or screen == null:
		return
	var tree := stage.get_tree()
	for _i in 2:
		await tree.process_frame
	if not is_instance_valid(screen) or not screen.is_inside_tree() or not is_mobile():
		return
	var app := stage.get_parent()
	var rect: Rect2 = app.call("stage_rect")
	var need := content_reach(screen)
	var wrap := screen.get_parent() as ScrollContainer
	if wrap == null:
		if need.x <= rect.size.x + 1.0 and need.y <= rect.size.y + 1.0:
			return
		wrap = ScrollContainer.new()
		wrap.name = "PanWrap"
		wrap.set_meta("pan_wrap", true)
		wrap.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
		wrap.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
		stage.add_child(wrap)
		screen.reparent(wrap, false)
		screen.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		screen.size_flags_vertical = Control.SIZE_EXPAND_FILL
		screen.tree_exited.connect(wrap.queue_free)
		wrap.get_h_scroll_bar().custom_minimum_size.y = 26.0   # bars a fingertip can take hold of
		wrap.get_v_scroll_bar().custom_minimum_size.x = 26.0
	wrap.position = rect.position
	wrap.size = rect.size
	screen.custom_minimum_size = Vector2(maxf(need.x, rect.size.x), maxf(need.y, rect.size.y))


## Bloom (the Environment's glow and CompatBloom) is off on a phone: it costs fill rate the phone does not have.
static func glow_allowed() -> bool:
	return not is_mobile()


## The render scale a viewport should use: the setting, capped by the slow-frame cap on mobile.
static func effective_render_scale(setting: float) -> float:
	return minf(setting, render_cap) if is_mobile() else setting
