extends Control
## Title screen: continue, new campaign, load, drills, settings. Port of screens/Title.tsx.
## Behind the menu a live 3D shot (TitleFleet, like Unreal's ConfigureTitleScene) films a hero fleet: the player's
## empire during a campaign (or of the latest save), otherwise a random power; a scrim keeps the text readable.
## $BOTF_TITLE_EMPIRE (1-5) pins the power, for screenshots.

const EMPIRE_NAMES := {1: "federation", 2: "klingon", 3: "romulan", 4: "cardassian", 5: "ferengi"}

var app: App
var backdrop: Control       ## The live fleet and its scrim; kept across refreshes.
var fleet: TitleFleet
var fleet_empire := 0

func refresh() -> void:
	for c in get_children():
		if c != backdrop:
			c.queue_free()
	_ensure_backdrop(title_empire())
	var root := Lcars.vbox(14)
	root.position = Vector2(size.x * 0.09, size.y * 0.16)
	root.custom_minimum_size = Vector2(620, 0)
	add_child(root)
	var emblems := Lcars.hbox(14)
	for id in range(1, 6):
		emblems.add_child(Lcars.emblem(id, 30))
	root.add_child(emblems)
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
	var tag := Lcars.para("Five powers. One quadrant. A modern reconstruction of the 1999 grand strategy classic, running on its deterministic simulation core.", Lcars.TEXT2, 17)
	tag.custom_minimum_size.x = 560
	root.add_child(tag)
	root.add_child(Control.new())
	var menu := Lcars.vbox(10)
	menu.custom_minimum_size.x = 420
	root.add_child(menu)
	var saves := app.list_saves()
	if saves.size() > 0:
		var s: Dictionary = saves[0]
		var b := Lcars.pill("Continue  ·  %s turn %d" % [s.get("playerName", ""), int(s.get("turn", 0))], "primary", func(): app.load_game(s), "", str(s.get("name", "")))
		b.name = "Continue"
		menu.add_child(b)
	var nb := Lcars.pill("New campaign", "secondary" if saves.size() > 0 else "primary", func(): app.goto("new"))
	nb.name = "NewCampaign"
	menu.add_child(nb)
	menu.add_child(Lcars.pill("Load campaign", "secondary", func(): app.goto("saves"), "", "Saved campaigns"))
	var row := Lcars.hbox(10)
	row.add_child(_wide(Lcars.pill("Tactical drill", "tertiary", func(): app.start_drill("newBattleDrill", "Tactical drill"), "", "A ready-made skirmish at Sol to practise tactical command")))
	row.add_child(_wide(Lcars.pill("Diplomacy drill", "tertiary", func(): app.start_drill("newDiplomacyDrill", "Diplomacy drill"), "", "Incoming proposals, a tribute demand and an ally's call to arms")))
	menu.add_child(row)
	var row2 := Lcars.hbox(10)
	row2.add_child(_wide(Lcars.pill("Frontier drill", "tertiary", func(): app.start_drill("newFrontierDrill", "Frontier drill"), "", "Outposts, starbases, anomalies and galactic events")))
	row2.add_child(_wide(Lcars.pill("Starbase drill", "tertiary", func(): app.start_drill("newBattleDrill", "Starbase drill", {"station": true}), "", "The skirmish at Sol under Remastered rules: 3D flight, weapon arcs and a starbase in the fight")))
	menu.add_child(row2)
	var row3 := Lcars.hbox(10)
	row3.add_child(_wide(Lcars.pill("Settings", "ghost", func(): app.open_overlay("settings"))))
	var quit := _wide(Lcars.pill("Quit", "ghost", func(): preload("res://scenes/ui/screens/overlays.gd").confirm_quit_game(app), "", "Close the game", "Alt+F4"))
	quit.name = "Quit"
	row3.add_child(quit)
	menu.add_child(row3)
	for b2 in menu.get_children():
		if b2 is Button:
			b2.custom_minimum_size.y = 46
			b2.add_theme_font_size_override("font_size", 21)
	var foot := Lcars.hbox(40)
	foot.position = Vector2(size.x * 0.09, size.y - 48)
	foot.add_child(Lcars.label("Simulation core %s · save schema %s · backend: %s" % [app.meta.get("contentVersion", "…"), str(app.meta.get("saveVersion", "…")), app.core.backend], "Mono", 13, Lcars.TEXT3))
	foot.add_child(Lcars.label("Unofficial fan reconstruction · not affiliated with any rights holder", "Muted"))
	add_child(foot)
	var first := menu.get_child(0) as Button
	if first and is_inside_tree():
		first.grab_focus.call_deferred()

func _wide(b: Button) -> Button:
	b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return b

## The power the title films: the campaign's empire, else the latest save's, else a random one.
func title_empire() -> int:
	var pinned := int(OS.get_environment("BOTF_TITLE_EMPIRE"))
	if pinned >= 1 and pinned <= 5:
		return pinned
	if app != null and not app.view.is_empty() and app.player() >= 1 and app.player() <= 5:
		return app.player()
	if fleet_empire > 0:
		return fleet_empire  # a refresh keeps the power the screen opened with
	var saves: Array = app.list_saves() if app != null else []
	if saves.size() > 0:
		var who := str(saves[0].get("playerName", "")).to_lower()
		for id in EMPIRE_NAMES:
			if who.find(EMPIRE_NAMES[id]) >= 0:
				return id
	return randi_range(1, 5)

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
	fleet = TitleFleet.new()
	fleet.animate = app == null or not app.settings.get("reducedMotion", false)
	sv.add_child(fleet)
	fleet.setup(empire, BattleAssets.new())
	backdrop.add_child(TitleScrim.new())

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
