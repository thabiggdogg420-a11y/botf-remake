extends Control
## Tactical combat presentation around the BattleHost: forces, ship cards, maneuver buttons with scope (all / group /
## ship), the encounter choice (take command / auto / hail) and the combat log; and the battle archive (replays,
## reports) outside a battle. Encounter state and tactical orders: CombatVM (scripts/viewmodels/combat_vm.gd).

var app: App
var host: BattleHost
var vm: CombatVM:
	get:
		if vm == null:
			vm = CombatVM.new(app.session)
		return vm
var ship: int:
	get: return vm.ship
	set(v): vm.ship = v
var target: int:
	get: return vm.target
	set(v): vm.target = v
var scope: String:
	get: return vm.scope
	set(v): vm.scope = v
var choice: bool:
	get: return vm.choice
	set(v): vm.choice = v
var archive_sel: Dictionary:
	get: return vm.archive_sel
	set(v): vm.archive_sel = v

const MANEUVERS := CombatVM.MANEUVERS
const IMMOBILE_BLOCKED := CombatVM.IMMOBILE_BLOCKED

static func is_active(s: Dictionary) -> bool:
	return CombatVM.is_active(s)

static func short_name(s: Dictionary) -> String:
	return CombatVM.short_name(s)

static func round_log(e: Dictionary, r: Dictionary) -> Array:
	return CombatVM.round_log(e, r)

func hail_outlook(e: Dictionary, foe: String) -> Array:
	return CombatVM.hail_outlook(e, foe)

func _enc() -> Dictionary:
	return vm.enc()

func refresh() -> void:
	# The 3D battle view survives refreshes. Rebuilding it on every order, ship pick and round reset the camera
	# mode, recompiled the effects (a first-round freeze) and caused 40-60 ms frame spikes.
	if host and is_instance_valid(host) and host.get_parent():
		host.get_parent().remove_child(host)
	for c in get_children():
		c.queue_free()
	var _t0 := Time.get_ticks_usec()
	vm.ensure_encounter()
	var _t1 := Time.get_ticks_usec()
	if vm.live() and not _enc().is_empty():
		_live(_enc())
	else:
		_archive()
	if OS.get_environment("ROUND_TIMING") != "":
		print("ROUND_TIMING combat refresh: encounter=%d build=%d ms" % [(_t1 - _t0) / 1000, (Time.get_ticks_usec() - _t1) / 1000])
	if host and is_instance_valid(host) and not host.is_inside_tree():
		host.queue_free()
		host = null

## The battle view, created once and re-parented into each rebuilt layout.
func _battle_host() -> BattleHost:
	if host == null or not is_instance_valid(host):
		host = BattleHost.new()
		host.ship_picked.connect(_pick)
		host.subsystem_picked.connect(_aim)
	host.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	host.size_flags_vertical = Control.SIZE_EXPAND_FILL
	return host

func _unhandled_key_input(event: InputEvent) -> void:
	var k := event as InputEventKey
	if k and k.pressed and not k.echo and (k.keycode == KEY_ENTER or k.keycode == KEY_KP_ENTER) and not app.has_modal():
		var e := _enc()
		if not e.is_empty() and e.get("finished", false):
			get_viewport().set_input_as_handled()
			app.end_turn()

# ---------------------------------------------------------------------------------------------
var _root: Control = null
var _first_build := true
var _cards := {} ## ship id -> card of the live layout

func _header(e: Dictionary, mine: Array, foes: Array, foe_name: String, last: Dictionary) -> Control:
	var h := Lcars.hbox(20)
	var alert := RedAlert.new()
	alert.custom_minimum_size = Vector2(26, 60)
	h.add_child(alert)
	var t := Lcars.vbox(0)
	var title := CombatVM.title(e, last)
	t.add_child(Lcars.label(title.to_upper(), "Heading", 38, Lcars.ALERT if not e.get("finished", false) else Lcars.accent()))
	t.add_child(Lcars.caption("%s · sector %d.%d · round %d%s" % [e.get("location", ""), int(e["sectorX"]), int(e["sectorY"]), int(e["round"]), "" if e.get("finished", false) else " of up to %d" % int(e["maxRounds"])]))
	h.add_child(t)
	h.add_child(Lcars.spacer())
	h.add_child(_force(mine, "Our forces"))
	h.add_child(Lcars.label("VS", "Heading", 28, Lcars.TEXT3))
	h.add_child(_force(foes, foe_name))
	h.add_child(Lcars.spacer())
	if e.get("finished", false):
		var c := Lcars.pill("Continue turn", "primary", func(): app.end_turn(), "", "", "Enter")
		c.name = "ContinueTurn"
		h.add_child(c)
	else:
		var r := Lcars.pill("Resolve round", "secondary", func(): _round(), "", "Resolve one round: every ship executes its order (or the AI decides)")
		r.name = "ResolveRound"
		h.add_child(r)
		var au := Lcars.pill("Auto-resolve", "alert", func(): _auto(), "", "Let your captains fight the battle to its end")
		au.name = "AutoResolve"
		h.add_child(au)
	return h

func _bottom(e: Dictionary, foe_name: String, last: Dictionary) -> Control:
	# Bottom: orders + log
	var bottom := Lcars.hbox(10)
	bottom.custom_minimum_size.y = 250
	bottom.add_child(_orders(e, foe_name))
	var sel := _selected()
	if not CombatVM.wego_of(sel).is_empty():
		bottom.add_child(_systems(sel))
	var logf := Lcars.frame("Combat log", ("round %d" % int(last["index"])) if not last.is_empty() else "awaiting first round", "tertiary", "chronicle")
	logf.custom_minimum_size.x = 560
	var lb := Lcars.vbox(2)
	if last.is_empty():
		lb.add_child(Lcars.para("The fleets deploy facing each other inside phaser range. Dashed lines show what each ship will do next round.", Lcars.TEXT3, 13))
	for line in round_log(e, last):
		lb.add_child(Lcars.para(line[1], Lcars.tone_color(line[0]), 13))
	Lcars.body(logf).add_child(Lcars.scroll(lb))
	bottom.add_child(logf)
	return bottom

## After a round: update the live layout in place (header, ship cards, orders and log) instead of rebuilding every
## node. R3.5: the full rebuild of a 24v24 layout cost 220-370 ms at each round start.
func refresh_round() -> void:
	var e := _enc()
	var ok: bool = _root != null and is_instance_valid(_root) and _root.is_inside_tree() and not e.is_empty() and not e.get("finished", false) and not choice
	var ships: Array = e.get("ships", [])
	if ok:
		for sd in ships:
			if not _cards.has(int(sd["id"])) or not is_instance_valid(_cards[int(sd["id"])]):
				ok = false
		ok = ok and _cards.size() == ships.size()
	if not ok:
		refresh()
		return
	var sd_ := vm.sides()
	var mine: Array = sd_["mine"]
	var foes: Array = sd_["foes"]
	var foe_name: String = sd_["foe_name"]
	var last: Dictionary = sd_["last"]
	var old_h: Control = _root.get_child(0)
	var nh := _header(e, mine, foes, foe_name, last)
	_root.add_child(nh)
	_root.move_child(nh, 0)
	old_h.queue_free()
	for sd in ships:
		var old: Button = _cards[int(sd["id"])]
		var hostile: bool = not sd.get("mine", false)
		if bool(old.get_meta("hidden", false)) == bool(sd.get("hidden", false)):
			_fill_card(old, sd, ships, target if hostile else ship, hostile)
			continue
		var nc := _ship_card(sd, ships, target if hostile else ship, hostile)
		var par := old.get_parent()
		par.add_child(nc)
		par.move_child(nc, old.get_index())
		par.remove_child(old)
		old.queue_free()
		_cards[int(sd["id"])] = nc
	var old_b: Control = _root.get_child(_root.get_child_count() - 1)
	var nb := _bottom(e, foe_name, last)
	_root.add_child(nb)
	old_b.queue_free()
	_root.remove_child(old_b)
	_bottom_box = nb
	_set_play_layout(_playing_layout)
	host.set_selection(ship, target)

func _stage_bottom(root: Control, ph: Control, e: Dictionary, foe_name: String, last: Dictionary) -> void:
	for _i in 3:
		await get_tree().process_frame
	if not is_instance_valid(ph) or not ph.is_inside_tree() or _bottom_box != ph:
		return
	var bottom := _bottom(e, foe_name, last)
	root.add_child(bottom)
	root.move_child(bottom, ph.get_index())
	root.remove_child(ph)
	ph.queue_free()
	_bottom_box = bottom
	_set_play_layout(_playing_layout)

func _live(e: Dictionary) -> void:
	_cards.clear()
	# F2: the first build of a battle's screen (the frame it opens) stages its cards and orders over the next frames.
	_stage_cards = _first_build and DisplayServer.get_name() != "headless"
	_first_build = false
	var sd_ := vm.sides()
	var ships: Array = sd_["ships"]
	var mine: Array = sd_["mine"]
	var foes: Array = sd_["foes"]
	var foe_name: String = sd_["foe_name"]
	var last: Dictionary = sd_["last"]
	var root := Lcars.vbox(10)
	Lcars.screen_margin(self, Vector4(14, 14, 14, 14)).add_child(root)
	_root = root
	var h := _header(e, mine, foes, foe_name, last)
	root.add_child(h)
	# Middle: ship lists and the battle host
	var mid := Lcars.hbox(10)
	mid.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_side_l = _ship_list("Our ships", mine, ships, ship, false)
	mid.add_child(_side_l)
	var hc := PanelContainer.new()
	hc.add_theme_stylebox_override("panel", Lcars.box(Color(0, 0, 0, 0.4), 16, Color(Lcars.ALERT, 0.35), 1, Vector4(2, 2, 2, 2)))
	hc.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hc.add_child(_battle_host())
	mid.add_child(hc)
	_side_r = _ship_list("Hostiles", foes, ships, target, true)
	mid.add_child(_side_r)
	root.add_child(mid)
	host.load_encounter.call_deferred(e)
	host.set_selection.call_deferred(ship, target)
	_plan.call_deferred()
	if _stage_cards:
		var ph := Control.new()
		ph.custom_minimum_size.y = 250
		root.add_child(ph)
		_bottom_box = ph
		_stage_bottom.call_deferred(root, ph, e, foe_name, last)
	else:
		var bottom := _bottom(e, foe_name, last)
		root.add_child(bottom)
		_bottom_box = bottom
	_stage_cards = false
	_set_play_layout(_playing_layout)
	if choice and not e.get("finished", false) and int(e["round"]) == 0:
		_encounter_choice(e, mine, foes, foe_name)

func _force(ships: Array, label: String) -> Control:
	var h := Lcars.hbox(10)
	var fo := CombatVM.force(ships)
	var own := int(ships[0]["owner"]) if ships.size() > 0 else 0
	var ring := RingGauge.make(58, int(fo["hull"]), max(1, int(fo["max"])), Lcars.owner_color(own), str(int(fo["alive"])), "",
		{"title": label, "value": "%d of %d ships" % [int(fo["alive"]), ships.size()], "parts": [["Hull remaining", "%d / %d" % [int(fo["hull"]), int(fo["max"])], ""]], "effect": "The ring is the side's combined hull. Ships at zero hull are lost."})
	h.add_child(ring)
	h.add_child(Lcars.emblem(own, 30))
	var v := Lcars.vbox(2)
	v.add_child(Lcars.label(label.to_upper(), "SubHeading", 17, Lcars.TEXT))
	v.add_child(Lcars.meter(fo["hull"], max(1, int(fo["max"])), Color("#8fd18a"), 5, 160))
	v.add_child(Lcars.label("%d/%d ships" % [int(fo["alive"]), ships.size()], "Mono", 12))
	h.add_child(v)
	return h

func _ship_list(title: String, ships: Array, all: Array, sel: int, hostile: bool) -> Control:
	var f := Lcars.frame(title, "", "alert" if hostile else "primary", "alert" if hostile else "ship")
	f.custom_minimum_size.x = 330
	var list := Lcars.vbox(6)
	if _stage_cards:
		_fill_list_staged.call_deferred(list, ships, all, sel, hostile)
	else:
		for s in ships:
			var cd := _ship_card(s, all, sel, hostile)
			_cards[int(s["id"])] = cd
			list.add_child(cd)
	Lcars.body(f).add_child(Lcars.scroll(list))
	return f

## F2: on the frame the battle opens, the ship cards (pictures, gauges, text) are built two a frame after it, so the
## screen's first frame carries only the header, the view and the frames (the cards were most of the open hitch).
var _stage_cards := false
func _fill_list_staged(list: Control, ships: Array, all: Array, sel: int, hostile: bool) -> void:
	var n := 0
	for s in ships:
		if n % 2 == 0:
			await get_tree().process_frame
		n += 1
		if not is_instance_valid(list) or not list.is_inside_tree():
			return
		var cd := _ship_card(s, all, sel, hostile)
		_cards[int(s["id"])] = cd
		list.add_child(cd)
		Motion.enter(cd, 0, "panel")

func _ship_card(s: Dictionary, all: Array, sel: int, hostile: bool) -> Control:
	var id := int(s["id"])
	var btn := Button.new()
	btn.name = "Ship%d" % id
	btn.theme_type_variation = "GhostButton"
	btn.custom_minimum_size.y = 96
	btn.disabled = s.get("hidden", false)
	var v := Lcars.vbox(3)
	v.set_anchors_preset(Control.PRESET_FULL_RECT)
	v.offset_left = 10
	v.offset_top = 8
	v.offset_right = -10
	v.offset_bottom = -6
	var h := Lcars.hbox(6)
	h.add_child(Lcars.emblem(int(s["owner"]), 14))
	var n := Lcars.label(short_name(s), "", 15, Lcars.TEXT)
	n.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	n.clip_text = true
	h.add_child(n)
	h.add_child(Lcars.label("#%d" % id, "Mono", 11, Lcars.TEXT3))
	v.add_child(h)
	btn.set_meta("hidden", s.get("hidden", false))
	if s.get("hidden", false):
		v.add_child(Lcars.label("Cloaked · decloaks to fire", "Muted", 12))
	else:
		var sh := Lcars.meter(0, 1, Lcars.SHIELD, 3)
		var hb := Lcars.meter(0, 1, Color("#8fd18a"), 4)
		var ln := Lcars.label("", "", 12, Lcars.TEXT2)
		var fl := Lcars.label("", "Mono", 11, Lcars.TEXT3)
		for x in [sh, hb, ln, fl]:
			v.add_child(x)
		btn.set_meta("parts", [sh, hb, ln, fl])
	_fill_card(btn, s, all, sel, hostile)
	for x in [v] + v.find_children("*", "Control", true, false):
		x.mouse_filter = Control.MOUSE_FILTER_IGNORE
	btn.add_child(v)
	btn.pressed.connect(func(): _pick(id))
	return btn

## Sets a card's changing state (selection frame, meters, next order, flags) without rebuilding it.
func _fill_card(btn: Button, s: Dictionary, all: Array, sel: int, hostile: bool) -> void:
	var id := int(s["id"])
	btn.disabled = s.get("hidden", false)
	var c := Lcars.owner_color(int(s["owner"]))
	var cs := CombatVM.card_state(s, all, hostile)
	var status: String = cs["status"]
	var sb := Lcars.box(Color(c, 0.18) if sel == id else Color(Lcars.PANEL2, 0.85), 12, c if sel == id else Lcars.LINE2, 2 if sel == id else 1)
	btn.add_theme_stylebox_override("normal", sb)
	btn.add_theme_stylebox_override("disabled", sb)
	btn.modulate = Color(1, 1, 1, 0.5) if status != "" else Color.WHITE
	if not btn.has_meta("parts"):
		return
	var parts: Array = btn.get_meta("parts")
	parts[0].max_value = max(1, int(s["maxShields"]))
	parts[0].value = clampf(float(s["shields"]), 0.0, parts[0].max_value)
	parts[1].max_value = max(1, int(s["maxHull"]))
	parts[1].value = clampf(float(max(0, int(s["hull"]))), 0.0, parts[1].max_value)
	parts[2].text = cs["line"]
	parts[2].add_theme_color_override("font_color", Lcars.accent2() if cs["ordered"] else Lcars.TEXT2)
	parts[3].text = cs["flags"]

## Plan phase (rules 4): the selected ship's arcs, range bands, move preview and forecast on the 3D view.
func _plan() -> void:
	if host == null or not is_instance_valid(host) or not host.is_inside_tree():
		return
	var sel := _selected()
	var fc := vm.forecast()
	if fc.is_empty() or sel.is_empty():
		host.clear_plan()
		return
	host.show_plan(sel, fc)

## A subsystem pip clicked on the target in the 3D view.
func _aim(_target_id: int, key: String) -> void:
	vm.set_subsystem(key)
	refresh()

## Rules 4 ship systems: six shield facings, five subsystem gauges, the crew rating and the aim selector.
func _systems(s: Dictionary) -> Control:
	var w := CombatVM.wego_of(s)
	var crew := str(w.get("crew", "Regular"))
	var f := Lcars.frame("Ship systems", short_name(s), "tertiary", "ship")
	f.name = "ShipSystems"
	f.custom_minimum_size.x = 400
	var b := Lcars.body(f)
	var top := Lcars.hbox(8)
	var xp_text := " · %d xp" % int(w["experience"]) if int(w.get("experience", -1)) >= 0 else ""  # Schema 23: campaign experience behind the rating
	var cb := Lcars.badge("Crew · " + crew + xp_text, CombatVM.CREW_TONES.get(crew, "primary"))
	cb.name = "CrewBadge"
	cb.tooltip_text = "Crew rating %s: it sets repair speed, fire accuracy and how fast destroyed systems come back." % crew
	if int(w.get("experience", -1)) >= 0:
		cb.tooltip_text += " Experience %d of 100 from battles, raids and academies." % int(w["experience"])
	top.add_child(cb)
	if w.get("coreBreached", false):
		top.add_child(Lcars.badge("Core breached", "alert"))
	if not w.get("canWarp", true):
		top.add_child(Lcars.badge("No warp", "warn"))
	b.add_child(top)
	var fac: Array = w.get("facings", [])
	var mx: Array = w.get("maxFacings", [])
	var g := Lcars.grid(2)
	g.name = "FacingBars"
	g.add_theme_constant_override("h_separation", 14)
	g.add_theme_constant_override("v_separation", 2)
	for i in CombatVM.FACINGS.size():
		var row := Lcars.hbox(6)
		var cap := Lcars.label(str(CombatVM.FACINGS[i]).substr(0, 4).to_upper(), "Mono", 11, Lcars.TEXT3)
		cap.custom_minimum_size.x = 38
		row.add_child(cap)
		var m := Lcars.meter(float(fac[i]) if i < fac.size() else 0.0, float(mx[i]) if i < mx.size() else 1.0, Lcars.SHIELD, 6, 70)
		m.name = "Facing_" + CombatVM.FACINGS[i]
		row.add_child(m)
		row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		g.add_child(row)
	b.add_child(g)
	var subs: Dictionary = w.get("subsystems", {})
	var sr := Lcars.hbox(8)
	sr.name = "SubsystemGauges"
	for def in CombatVM.SUBSYSTEMS:
		var hp := int(subs.get(def[0], 100))
		var col := Color("#8fd18a") if hp >= 60 else (Color("#e8b44a") if hp > 0 else Lcars.ALERT)
		var cell := Lcars.vbox(1)
		cell.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		cell.tooltip_text = "%s %d%%" % [def[2], hp]
		cell.add_child(Lcars.label("%s %d" % [def[1], hp], "Mono", 11, Lcars.TEXT2))
		var m2 := Lcars.meter(hp, 100, col, 6)
		m2.name = "Sys_" + def[0]
		cell.add_child(m2)
		sr.add_child(cell)
	b.add_child(sr)
	b.add_child(Lcars.caption("Aim at the target's subsystem"))
	var ar := Lcars.hbox(4)
	ar.name = "AimSelector"
	var opts := [["none", "NONE", "No aim: fire spreads over the target's facing"]]
	for def in CombatVM.SUBSYSTEMS:
		opts.append([def[0], def[1], "Aim at the target's %s" % def[2].to_lower()])
	for o in opts:
		var key: String = o[0]
		var tg := Lcars.toggle(o[1], vm.subsystem == key, func(_on): vm.set_subsystem(key); refresh(), o[2])
		tg.name = "Aim_" + key
		tg.add_theme_font_size_override("font_size", 12)
		ar.add_child(tg)
	b.add_child(ar)
	return f

func _pick(id: int) -> void:
	vm.pick(id)
	refresh()

func _selected() -> Dictionary:
	return vm.selected()

func _orders(e: Dictionary, foe_name: String) -> Control:
	var sel := _selected()
	var f := Lcars.frame("Orders", vm.scope_label(), "secondary", "orders")
	f.name = "TacticalOrders"
	f.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var b := Lcars.body(f)
	var sc := Lcars.hbox(6)
	sc.add_child(Lcars.caption("Scope"))
	for pair in CombatVM.SCOPES:
		var k: String = pair[0]
		var tg := Lcars.toggle(pair[1], scope == k, func(_on): scope = k; refresh(), pair[2])
		tg.name = "Scope_" + k
		tg.disabled = vm.scope_disabled(k)
		sc.add_child(tg)
	b.add_child(sc)
	var grid := Lcars.grid(6)
	grid.add_theme_constant_override("h_separation", 6)
	for mv in MANEUVERS:
		var key: String = mv["key"]
		if key == "Retreat" and scope == "ship" and CombatVM.retreat_hidden(sel):
			continue # rules 4: no warp, no withdrawal (warp core or engines down, or no safe sector)
		var p := Lcars.pill(mv["name"], CombatVM.maneuver_tone(key), func(): order(key), vm.maneuver_block(key), vm.maneuver_hint(mv))
		p.name = "Maneuver_" + key
		p.text = str(mv["name"]).to_upper() # the ghost Hail pill kept mixed case, and Antonio's small "i" dot read as "Hall"
		p.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		if scope == "ship" and sel.get("maneuver", "") == key:
			p.add_theme_stylebox_override("normal", Lcars.box(Lcars.accent(), 16, Color.WHITE, 2, Vector4(16, 5, 16, 5)))
		grid.add_child(p)
	b.add_child(grid)
	var ho := hail_outlook(e, foe_name)
	b.add_child(Lcars.para(ho[0], Lcars.tone_color(ho[1]), 13))
	if sel.get("cloaked", false) and scope != "all":
		b.add_child(Lcars.para("%s is cloaked: it cannot fire while cloaked. An attack order decloaks it with a +25%% ambush volley." % short_name(sel), Lcars.TEXT3, 12))
	return f

## Orders a maneuver for the current scope via tacticalOrder.
func order(maneuver: String) -> void:
	var note := vm.order(maneuver)
	if note == "":
		return
	app.toast(note, "info", 1.8)
	refresh()

func _round() -> void:
	var _t0 := Time.get_ticks_usec()
	if not vm.resolve(false):
		return
	var rounds: Array = _enc().get("rounds", [])
	var timing := OS.get_environment("ROUND_TIMING") != ""
	if timing:
		print("ROUND_TIMING resolve=%d ms" % ((Time.get_ticks_usec() - _t0) / 1000))
	if not (host and is_instance_valid(host) and host.is_inside_tree() and rounds.size() > 0 and _ui_ready()):
		refresh_round() # rebuilds the layout when the panels cannot be updated in place
		if host and rounds.size() > 0:
			_set_play_layout(true)
			host.clear_plan()
			host.play_round.call_deferred(rounds.back(), _enc())
			if not host.round_finished.is_connected(_plan_after_round):
				host.round_finished.connect(_plan_after_round, CONNECT_ONE_SHOT)
		return
	# F2: the core's resolve, the round's playback build and the (folded away) panels' refresh each get their own frame;
	# together on one frame they were the 55-90 ms hitch at the start of every round.
	_set_play_layout(true)
	host.clear_plan()
	if not host.round_finished.is_connected(_plan_after_round):
		host.round_finished.connect(_plan_after_round, CONNECT_ONE_SHOT)
	await get_tree().process_frame
	if not is_inside_tree() or host == null or not is_instance_valid(host):
		return
	host.play_round(rounds.back(), _enc())
	await get_tree().process_frame
	if not is_inside_tree():
		return
	var _t1 := Time.get_ticks_usec()
	refresh_round()
	if timing:
		print("ROUND_TIMING refresh_round=%d ms" % ((Time.get_ticks_usec() - _t1) / 1000))

## True when refresh_round can update the panels in place (no full rebuild, which re-parents the battle view).
func _ui_ready() -> bool:
	var e := _enc()
	if _root == null or not is_instance_valid(_root) or not _root.is_inside_tree() or e.is_empty() or e.get("finished", false) or choice:
		return false
	var ships: Array = e.get("ships", [])
	if _cards.size() != ships.size():
		return false
	for sd in ships:
		if not _cards.has(int(sd["id"])) or not is_instance_valid(_cards[int(sd["id"])]):
			return false
	return true

## While a round plays the ship lists and the orders bar fold away so the battle fills the screen (M4 combat 2: the 3D view sat
## in a small panel between the lists and every ship was tiny); they come back for the plan phase.
var _side_l: Control
var _side_r: Control
var _bottom_box: Control
var _playing_layout := false
var _chain := 0

func _set_play_layout(on: bool) -> void:
	_playing_layout = on
	if app != null and app.has_method("set_chrome_folded") and is_inside_tree():
		app.set_chrome_folded(on) # F2: the top bar and rail fold away too while the round plays
	for n in [_side_l, _side_r, _bottom_box]:
		if n != null and is_instance_valid(n):
			n.visible = not on
			if not on and is_inside_tree():
				# The plan layout fades back in instead of snapping into the middle of the view.
				n.modulate.a = 0.0
				create_tween().tween_property(n, "modulate:a", 1.0, 0.3)
			else:
				n.modulate.a = 1.0

func _plan_after_round(_idx: int) -> void:
	# A round of nothing but closing in has just run through at speed: go straight on into the next, so one Engage reaches the
	# first shots (at most four such rounds in a row; the orders stay as given).
	var vn: Node = host.view_node if host != null and is_instance_valid(host) else null
	if vn != null and bool(vn.get("last_round_idle")) and not _enc().get("finished", false) and _chain < 4:
		_chain += 1
		if vm.maneuver != "":
			vm.order(vm.maneuver)
		_round.call_deferred()
		return
	_chain = 0
	_set_play_layout(false)
	_plan()

func _auto() -> void:
	if not vm.resolve(true):
		return
	refresh()

func _hail() -> void:
	vm.hail_all()
	_round()

func _encounter_choice(e: Dictionary, mine: Array, foes: Array, foe_name: String) -> void:
	var col := Lcars.vbox(12)
	col.name = "EncounterChoice"
	col.add_child(Lcars.para("%d of our ships meet %d %s ship%s at %s. The strategic turn is paused until the battle is decided." % [mine.size(), foes.size(), foe_name, "" if foes.size() == 1 else "s", e.get("location", "")], Lcars.TEXT, 15))
	var hidden: int = foes.filter(func(s): return s.get("hidden", false)).size()
	if hidden > 0:
		col.add_child(Lcars.para("Sensors read %d cloaked contact%s: they must decloak to fire, and their first volley gains a +25%% ambush bonus." % [hidden, "" if hidden == 1 else "s"], Lcars.TEXT3, 13))
	var ho := hail_outlook(e, foe_name)
	col.add_child(Lcars.para(ho[0], Lcars.tone_color(ho[1]), 14))
	var row := Lcars.hbox(10)
	var take := Lcars.pill("Take command", "alert", func(): choice = false; app.close_modal(), "", "Command your ships round by round")
	take.name = "TakeCommand"
	row.add_child(take)
	row.add_child(Lcars.pill("Auto-resolve", "secondary", func(): app.close_modal(); _auto(), "", "Your ships fight under automatic command"))
	row.add_child(Lcars.pill("HAIL", "ghost", func(): app.close_modal(); _hail(), "", "Every ship hails and one round resolves"))
	col.add_child(row)
	app.open_modal.call_deferred(col, TrekText.term(0, "contact_title", "Enemy contact"), Vector2(720, 0), "alert")
	choice = false

# ---------------------------------------------------------------------------------------------
func _archive() -> void:
	var v := app.view
	var replays := vm.replays()
	var row := Lcars.hbox(14)
	Lcars.screen_margin(self).add_child(row)
	var f := Lcars.frame("Battle records", "%d recent" % v.get("battles", []).size(), "alert")
	f.custom_minimum_size.x = 460
	var b := Lcars.body(f)
	if replays.is_empty() and v.get("battles", []).is_empty():
		b.add_child(Lcars.empty_note("No battles fought yet. Declare war and engage an enemy, or try the Tactical Drill from the title screen."))
	for r in replays:
		var idx := int(r["index"])
		b.add_child(Lcars.pill("Turn %d · sector %d.%d · %d rounds" % [int(r["turn"]), int(r["x"]), int(r["y"]), int(r["rounds"])], "ghost", func(): archive_sel = {"kind": "replay", "index": idx}; refresh()))
	for bt in v.get("battles", []):
		var idx2 := int(bt["index"])
		b.add_child(Lcars.pill("Report · turn %d · %d.%d · %d lost" % [int(bt["turn"]), int(bt["x"]), int(bt["y"]), int(bt["lost"])], "ghost", func(): archive_sel = {"kind": "report", "index": idx2}; refresh()))
	b.add_child(Lcars.pill("Back to chart", "ghost", func(): app.goto("galaxy")))
	row.add_child(f)
	var right := Lcars.frame("Replay", "", "secondary")
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var rb := Lcars.body(right)
	if archive_sel.get("kind", "") == "replay":
		var e: Variant = vm.archive_entry()
		if e is Dictionary:
			rb.add_child(_battle_host())
			host.load_encounter.call_deferred(e)
			for line in round_log(e, e["rounds"].back() if e["rounds"].size() > 0 else {}):
				rb.add_child(Lcars.para(line[1], Lcars.tone_color(line[0]), 13))
	elif archive_sel.get("kind", "") == "report":
		var rep: Variant = vm.archive_entry()
		if rep is Dictionary:
			for p in rep.get("participants", []):
				var h := Lcars.hbox(8)
				h.add_child(Lcars.emblem(int(p["owner"]), 16))
				h.add_child(Lcars.label("%s #%d" % [p["name"], int(p["id"])], "", 14))
				h.add_child(Lcars.badge("lost", "alert") if rep.get("lost", []).has(p["id"]) else Lcars.badge("survived", "good"))
				rb.add_child(h)
		else:
			rb.add_child(Lcars.empty_note(app.core.last_error))
	else:
		rb.add_child(Lcars.empty_note("Select a battle."))
	row.add_child(right)
