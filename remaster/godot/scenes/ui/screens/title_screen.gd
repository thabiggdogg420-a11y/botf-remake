extends Control
## Title presentation: continue, new campaign, load, drills, settings. Data from TitleVM.
## Behind the menu a live 3D shot (TitleFleet, like Unreal's ConfigureTitleScene) films a hero fleet: the player's
## empire during a campaign (or of the latest save), otherwise a random power; a scrim keeps the text readable.
## $BOTF_TITLE_EMPIRE (1-5) pins the power, for screenshots.

const EMPIRE_NAMES := TitleVM.EMPIRE_NAMES

var app: App
var vm: TitleVM:
	get:
		if vm == null:
			vm = TitleVM.new(app.session if app else null)
		return vm
var backdrop: Control       ## The live fleet and its scrim; kept across refreshes.
var fleet: TitleFleet
var fleet_empire := 0
## Quick Start (ONBOARDING-AND-MOBILE-PLAN.md 1.1): the power the emblems have picked (0 = the one the title shows), and the
## galaxy seed it starts with (0 = a fresh random one; a test pins it).
var quick_empire := 0
var quick_seed := 0
var _emblem_row: HBoxContainer

func refresh() -> void:
	for c in get_children():
		if c != backdrop:
			c.queue_free()
	var t := LoadTrace.start()
	vm.fetch()
	t = LoadTrace.done("title: vm", t)
	_ensure_backdrop(title_empire())
	t = LoadTrace.done("title: backdrop", t)
	var root := Lcars.vbox(14)
	root.position = Vector2(size.x * 0.09, size.y * 0.16)
	root.custom_minimum_size = Vector2(620, 0)
	add_child(root)
	var crest: Variant = Lcars.emblem(title_empire(), 104, Lcars.owner_color(title_empire()))
	crest.style = "seal"
	crest.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	root.add_child(crest)
	var emblems := Lcars.hbox(14)
	_emblem_row = emblems
	for id in range(1, Lcars.FACTIONS.size() + 1): # all eight powers (R8: it drew five)
		if id <= QUICK_POWERS:
			emblems.add_child(_emblem_button(id))   # a Quick Start power: tap one, then Quick Start
		else:
			var em := Lcars.emblem(id, 30)
			em.modulate.a = 0.5
			em.tooltip_text = "%s: play it from Custom game" % Lcars.FACTIONS[id - 1]["short"]
			em.mouse_filter = Control.MOUSE_FILTER_PASS
			emblems.add_child(em)
	root.add_child(emblems)
	var pick_hint := Lcars.label("Tap a power, then Quick Start", "Muted", 13)
	pick_hint.name = "QuickHint"
	root.add_child(pick_hint)
	root.add_child(Lcars.label("BIRTH OF THE FEDERATION", "Caption", 22, Lcars.accent2()))
	var big := Lcars.hbox(14)
	var logo := Lcars.label("BOTF", "", 132, Lcars.TEXT)
	logo.add_theme_font_override("font", Lcars.font("display", 700))
	big.add_child(logo)
	var rem := Lcars.label("REMASTERED", "", 44, Lcars.accent())
	rem.add_theme_font_override("font", Lcars.font("display", 400))
	rem.size_flags_vertical = Control.SIZE_SHRINK_END
	big.add_child(rem)
	root.add_child(big)
	var eraname := Lcars.hbox(8)
	eraname.add_child(Lcars.badge(UiTokens.era_def()["label"] + " interface", "secondary"))
	eraname.add_child(Lcars.label("Ctrl+K in game opens the command palette", "Muted", 13))
	var tag := Lcars.para("Eight powers. One quadrant. A modern reconstruction of the 1999 grand strategy classic, running on its deterministic simulation core.", Lcars.TEXT2, 17)
	tag.custom_minimum_size.x = 560
	root.add_child(tag)
	root.add_child(eraname)
	root.add_child(Control.new())
	var menu := Lcars.vbox(10)
	menu.custom_minimum_size.x = 420
	root.add_child(menu)
	var saves := vm.saves
	var qs := Lcars.pill("Quick Start", "primary" if saves.size() == 0 else "secondary", func(): quick_start(), "", "Small galaxy, early era, classic rules and a guided first three turns. Pick an empire above, or play the one shown.")
	qs.name = "QuickStart"
	menu.add_child(qs)
	if saves.size() > 0:
		var s: Dictionary = vm.latest()
		var b := Lcars.pill("Continue  ·  %s turn %d" % [s.get("playerName", ""), int(s.get("turn", 0))], "primary", func(): app.load_game(s), "", str(s.get("name", "")))
		b.name = "Continue"
		menu.add_child(b)
	var nb := Lcars.pill("Custom game", "secondary", func(): app.goto("new"), "", "Every option: galaxy, era, rules, presets and powers")
	nb.name = "NewCampaign"
	menu.add_child(nb)
	var lb := Lcars.pill("Load campaign", "secondary", func(): app.goto("saves"), "", "Saved campaigns")
	lb.name = "LoadCampaign"
	menu.add_child(lb)
	var row := Lcars.hbox(10)
	row.add_child(_wide(Lcars.pill("Tactical drill", "tertiary", func(): app.start_drill("newBattleDrill", "Tactical drill", {}, true), "", "A ready-made skirmish at Sol to practise tactical command")))
	row.add_child(_wide(Lcars.pill("Diplomacy drill", "tertiary", func(): app.start_drill("newDiplomacyDrill", "Diplomacy drill"), "", "Incoming proposals, a tribute demand and an ally's call to arms")))
	menu.add_child(row)
	var row2 := Lcars.hbox(10)
	row2.add_child(_wide(Lcars.pill("Frontier drill", "tertiary", func(): app.start_drill("newFrontierDrill", "Frontier drill"), "", "Outposts, starbases, anomalies and galactic events")))
	row2.add_child(_wide(Lcars.pill("Starbase drill", "tertiary", func(): app.start_drill("newBattleDrill", "Starbase drill", {"station": true}), "", "The skirmish at Sol under Remastered rules: 3D flight, weapon arcs and a starbase in the fight")))
	menu.add_child(row2)
	var row3 := Lcars.hbox(10)
	if UserPacks.supported(app.session.core if app and app.session else null):
		var pe := _wide(Lcars.pill("Pack editor", "tertiary", func(): app.goto("packs"), "", "Make your own pack: ship classes, structures, techs, balance, eras and powers. Packs you save join the preset picker"))
		pe.name = "PackEditor"
		row3.add_child(pe)
	if not OS.has_feature("web"):   # LAN multiplayer is a desktop feature (the browser cannot open sockets)
		var lanb := _wide(Lcars.pill("LAN game", "tertiary", func(): app.open_lan_lobby(), "", "Host or join a game on the local network"))
		lanb.name = "LanGame"
		row3.add_child(lanb)
	row3.add_child(_wide(Lcars.pill("Settings", "ghost", func(): app.open_overlay("settings"))))
	var quit := _wide(Lcars.pill("Quit", "ghost", func(): Overlays.confirm_quit_game(app), "", "Close the game", "Alt+F4"))
	quit.name = "Quit"
	if not OS.has_feature("web"):   # a browser tab is closed by the browser: Quit would only freeze the canvas
		row3.add_child(quit)
	menu.add_child(row3)
	for b2 in menu.get_children():
		if b2 is Button:
			b2.custom_minimum_size.y = 52
			b2.add_theme_font_size_override("font_size", 22)
			b2.alignment = HORIZONTAL_ALIGNMENT_LEFT
			var nm := str(b2.name)
			var ic := UiIcon.make("save" if nm.begins_with("Load") or nm == "Continue" else ("star" if nm == "NewCampaign" or nm == "QuickStart" else "arrow_right"), 22, Lcars.ink())
			ic.position = Vector2(b2.custom_minimum_size.y * 0.5 + 6, 15)
			b2.add_child(ic)
			b2.text = "        " + b2.text
	Motion.stagger(menu)
	if OS.has_feature("web"):
		# Web only: the optional whole-game preload, top right, clear of the menu column and the fleet scene.
		var pre := PreloadWidget.make(app)
		pre.set_anchors_preset(Control.PRESET_TOP_RIGHT)
		pre.offset_left = -340
		pre.offset_right = -20
		pre.offset_top = 18
		add_child(pre)
	# A short window (a 720 px browser tab) with a Continue button: lift the column so the last row clears the footer.
	root.position.y = clampf(root.position.y, 8.0, maxf(8.0, size.y - 64.0 - root.get_combined_minimum_size().y))
	var foot := Lcars.hbox(40)
	foot.position = Vector2(size.x * 0.09, maxf(size.y - 48.0, root.position.y + root.get_combined_minimum_size().y + 14.0))   # a phone window is shorter than the column: the footer follows it, not the window bottom
	foot.add_child(Lcars.label(vm.footer(), "Mono", 13, Lcars.TEXT3))
	foot.add_child(Lcars.label("Unofficial fan reconstruction · not affiliated with any rights holder", "Muted"))
	add_child(foot)
	# 21 Nov: the build's version, small, bottom-right (stamped into application/config/version by the export scripts).
	var ver := Lcars.label(build_version(), "Mono", 12, Lcars.TEXT3)
	ver.name = "BuildVersion"
	ver.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	ver.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	ver.grow_vertical = Control.GROW_DIRECTION_BEGIN
	ver.offset_right = -14
	ver.offset_bottom = -10
	ver.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	add_child(ver)
	var first := menu.get_child(0) as Button
	if first and is_inside_tree():
		first.grab_focus.call_deferred()
	LoadTrace.done("title: menu", t)

const QUICK_POWERS := 5   ## the classic five (Federation to Ferengi) can start a Quick Start; the other three play from Custom game

## The power a Quick Start plays: the emblem picked, else the one the title is showing.
func quick_faction() -> int:
	var f := quick_empire if quick_empire > 0 else title_empire()
	return f if f >= 1 and f <= QUICK_POWERS else 1

## One tap and the player is in the galaxy: the beginner defaults (NewCampaignVM.quick) and the tutorial.
func quick_start() -> void:
	var qv := NewCampaignVM.quick(app.session, quick_faction())
	if quick_seed > 0:
		qv.seed_value = quick_seed
	app.start_campaign(qv.quick_options())

func _emblem_button(id: int) -> Button:
	var b := Button.new()
	b.name = "Emblem%d" % id
	b.toggle_mode = true
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(44, 44)
	b.tooltip_text = "%s · capital %s. Tap it, then Quick Start." % [Lcars.FACTIONS[id - 1]["short"], Lcars.FACTIONS[id - 1]["capital"]]
	var em := Lcars.emblem(id, 30)
	em.position = Vector2(7, 7)
	em.mouse_filter = Control.MOUSE_FILTER_IGNORE
	b.add_child(em)
	b.pressed.connect(func(): _pick_emblem(id))
	_style_emblem(b, id == quick_faction())
	return b

func _style_emblem(b: Button, picked: bool) -> void:
	b.button_pressed = picked
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(Lcars.owner_color(int(b.name.substr(6))), 0.16) if picked else Color(0, 0, 0, 0)
	sb.border_color = Lcars.owner_color(int(b.name.substr(6))) if picked else Color(0, 0, 0, 0)
	sb.set_border_width_all(2)
	sb.set_corner_radius_all(10)
	for st in ["normal", "hover", "pressed", "hover_pressed", "focus"]:
		b.add_theme_stylebox_override(st, sb)

func _pick_emblem(id: int) -> void:
	quick_empire = id
	if _emblem_row == null:
		return
	for c in _emblem_row.get_children():
		if c is Button:
			_style_emblem(c as Button, int(c.name.substr(6)) == id)

## "BOTF Remastered 0.1.540 (abc1234)" from the stamped project setting, or a plain "dev build" when unstamped.
static func build_version() -> String:
	var v := str(ProjectSettings.get_setting("application/config/version", ""))
	return "BOTF Remastered " + (v if v != "" else "dev build")

func _wide(b: Button) -> Button:
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return b

## The power the title films: the campaign's empire, else the latest save's, else a random one.
func title_empire() -> int:
	return vm.title_empire(int(CompatLook.flag("BOTF_TITLE_EMPIRE")), fleet_empire) # env, or a web user argument

## Rebuilds the live fleet (the web build calls this when the title empire's hero hulls arrive in the background).
func refresh_fleet() -> void:
	if backdrop != null:
		backdrop.queue_free()
		backdrop = null
	_ensure_backdrop(fleet_empire if fleet_empire > 0 else title_empire())

func _ensure_backdrop(empire: int) -> void:
	if backdrop != null and fleet_empire == empire:
		move_child(backdrop, 0)
		return
	if backdrop != null:
		backdrop.queue_free()
	fleet_empire = empire
	backdrop = Control.new()
	backdrop.name = "TitleBackdrop"
	backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(backdrop)
	move_child(backdrop, 0)
	var svc := SubViewportContainer.new()
	svc.name = "Live"
	svc.stretch = true
	svc.set_anchors_preset(Control.PRESET_FULL_RECT)
	svc.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var sv := SubViewport.new()
	sv.own_world_3d = true
	sv.msaa_3d = Viewport.MSAA_2X
	if app != null:
		DisplaySettings.apply_viewport(sv, app.settings)
	svc.add_child(sv)
	backdrop.add_child(svc)
	CompatBloom.attach(svc, sv, 0.95, 0.6, 0.8) # web: engines and lights glow, lit hulls do not
	LoadTrace.mark("title: bloom attached")
	fleet = TitleFleet.new()
	fleet.animate = app == null or not app.settings.get("reducedMotion", false)
	sv.add_child(fleet)
	backdrop.add_child(TitleScrim.new())
	if OS.has_feature("web"):
		fleet.setup_sliced(empire, BattleAssets.new()) # web: a piece per frame (smoothness pass, 19 Nov)
	else:
		fleet.setup(empire, BattleAssets.new())

## Dark at the left where the title and menu sit, clear right of centre where the fleet flies, with a low band for
## the footer: a smoothstep ramp so no edge shows.
class TitleScrim extends TextureRect:
	func _init() -> void:
		name = "Scrim"
		set_anchors_preset(Control.PRESET_FULL_RECT)
		mouse_filter = Control.MOUSE_FILTER_IGNORE
		expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		stretch_mode = TextureRect.STRETCH_SCALE
		var g := Gradient.new()
		var bg := Lcars.BG
		g.offsets = PackedFloat32Array()
		g.colors = PackedColorArray()
		for k in 9:
			var x := float(k) / 8.0 * 0.62
			var a := 0.86 * (1.0 - smoothstep(0.18, 0.62, x))
			g.add_point(x, Color(bg, a))
		g.add_point(1.0, Color(bg, 0.0))
		var tex := GradientTexture2D.new()
		tex.gradient = g
		tex.width = 256
		tex.height = 4
		tex.fill_from = Vector2(0, 0.5)
		tex.fill_to = Vector2(1, 0.5)
		texture = tex
		var low := TextureRect.new()
		low.set_anchors_preset(Control.PRESET_FULL_RECT)
		low.mouse_filter = Control.MOUSE_FILTER_IGNORE
		low.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		low.stretch_mode = TextureRect.STRETCH_SCALE
		var g2 := Gradient.new()
		g2.offsets = PackedFloat32Array()
		g2.colors = PackedColorArray()
		g2.add_point(0.0, Color(bg, 0.0))
		g2.add_point(0.8, Color(bg, 0.0))
		g2.add_point(1.0, Color(bg, 0.7))
		var t2 := GradientTexture2D.new()
		t2.gradient = g2
		t2.width = 4
		t2.height = 256
		t2.fill_from = Vector2(0.5, 0)
		t2.fill_to = Vector2(0.5, 1)
		low.texture = t2
		add_child(low)
