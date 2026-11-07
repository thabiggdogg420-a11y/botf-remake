extends Control
## Tactical combat orders around the BattleHost: forces, ship cards, maneuver buttons with scope
## (all / group / ship), encounter choice (take command / auto / hail), combat log; and the battle
## archive (replays, reports) outside a battle. Port of screens/Combat.tsx.

var app: App
var host: BattleHost
var ship := 0
var target := 0
var scope := "all"
var choice := true
var archive_sel := {}

const ATTACK := "Attack order: a cloaked ship decloaks and fires a +25% ambush volley."
const MANEUVERS := [
	{"key": "Engage", "name": "Engage", "hint": "Advance to optimal weapon range and fire.\n" + ATTACK},
	{"key": "Assault", "name": "Assault", "hint": "Disciplined advance to the medium beam/torpedo envelope.\n" + ATTACK},
	{"key": "Charge", "name": "Charge", "hint": "Close to point-blank range: +20% beam damage, but +15% incoming.\n" + ATTACK},
	{"key": "Strafe", "name": "Strafe", "hint": "Fast close pass: +25% beam, -20% torpedo, +10% incoming.\n" + ATTACK},
	{"key": "Flyby", "name": "Flyby", "hint": "Fast pass at medium range: -15% incoming.\n" + ATTACK},
	{"key": "Harry", "name": "Harry", "hint": "Close evasive circling: -10% beam, -20% incoming.\n" + ATTACK},
	{"key": "Circle", "name": "Circle", "hint": "Stand-off orbit: -15% beam, -25% incoming. Keeps a cloak."},
	{"key": "Evade", "name": "Evade", "hint": "Long-range evasion: -30% fire, -35% incoming. Keeps a cloak."},
	{"key": "Hold", "name": "Hold", "hint": "Brace in place: -20% incoming damage. Shields do not recharge. Keeps a cloak."},
	{"key": "Ram", "name": "Ram", "hint": "Collision: the target takes the rammer's hull as damage and the rammer takes the target's hull + shields as recoil."},
	{"key": "Hail", "name": "Hail", "hint": "Offer a truce. If every active ship hails and the enemy accepts, the battle ends before any fire."},
	{"key": "Retreat", "name": "Retreat", "hint": "Disengage: one round at -25% incoming with weapons offline, then escape if still retreating."},
]
const IMMOBILE_BLOCKED := ["Retreat", "Charge", "Circle", "Flyby", "Strafe", "Evade", "Ram", "Harry", "Assault"]

static func is_active(s: Dictionary) -> bool:
	return s.get("alive", false) and s.get("active", false) and not s.get("retreated", false)

static func short_name(s: Dictionary) -> String:
	if s.is_empty():
		return "nearest enemy"
	return "Cloaked contact" if s.get("hidden", false) else str(s["name"]).split(" — ")[0]

func _enc() -> Dictionary:
	return app.encounter if app.encounter is Dictionary else {}

func refresh() -> void:
	# The 3D battle view survives refreshes. Rebuilding it on every order, ship pick and round reset the camera
	# mode, recompiled the effects (a first-round freeze) and caused 40-60 ms frame spikes.
	if host and is_instance_valid(host) and host.get_parent():
		host.get_parent().remove_child(host)
	for c in get_children():
		c.queue_free()
	var live: bool = app.view.get("session", {}).get("active", false) or app.view.get("session", {}).get("battlePending", false)
	if live and _enc().is_empty():
		var e: Variant = app.core.request("tactical")
		app.encounter = e
	if live and not _enc().is_empty():
		_live(_enc())
	else:
		_archive()
	if host and is_instance_valid(host) and not host.is_inside_tree():
		host.queue_free()
		host = null

## The battle view, created once and re-parented into each rebuilt layout.
func _battle_host() -> BattleHost:
	if host == null or not is_instance_valid(host):
		host = BattleHost.new()
		host.ship_picked.connect(_pick)
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
func _live(e: Dictionary) -> void:
	var ships: Array = e["ships"]
	var mine: Array = ships.filter(func(s): return s.get("mine", false))
	var foes: Array = ships.filter(func(s): return not s.get("mine", false))
	if ship == 0 or mine.filter(func(s): return int(s["id"]) == ship and is_active(s)).is_empty():
		var a: Array = mine.filter(func(s): return is_active(s))
		ship = int(a[0]["id"]) if a.size() > 0 else 0
	if target == 0 or foes.filter(func(s): return int(s["id"]) == target and is_active(s) and not s.get("hidden", false)).is_empty():
		var b: Array = foes.filter(func(s): return is_active(s) and not s.get("hidden", false))
		target = int(b[0]["id"]) if b.size() > 0 else 0
	var foe_name: String = foes[0]["ownerName"] if foes.size() > 0 else "enemy"
	var rounds: Array = e.get("rounds", [])
	var last: Dictionary = rounds.back() if rounds.size() > 0 else {}
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 14)
	add_child(m)
	var root := Lcars.vbox(10)
	m.add_child(root)
	# Header
	var h := Lcars.hbox(20)
	var alert := RedAlert.new()
	alert.custom_minimum_size = Vector2(26, 60)
	h.add_child(alert)
	var t := Lcars.vbox(0)
	var title := "Truce agreed" if e.get("finished", false) and last.get("truce", false) else ("Engagement concluded" if e.get("finished", false) else "Red alert")
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
	root.add_child(h)
	# Middle: ship lists and the battle host
	var mid := Lcars.hbox(10)
	mid.size_flags_vertical = Control.SIZE_EXPAND_FILL
	mid.add_child(_ship_list("Our ships", mine, ships, ship, false))
	var hc := PanelContainer.new()
	hc.add_theme_stylebox_override("panel", Lcars.box(Color(0, 0, 0, 0.4), 16, Color(Lcars.ALERT, 0.35), 1, Vector4(2, 2, 2, 2)))
	hc.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hc.add_child(_battle_host())
	mid.add_child(hc)
	mid.add_child(_ship_list("Hostiles", foes, ships, target, true))
	root.add_child(mid)
	host.load_encounter.call_deferred(e)
	host.set_selection.call_deferred(ship, target)
	# Bottom: orders + log
	var bottom := Lcars.hbox(10)
	bottom.custom_minimum_size.y = 250
	bottom.add_child(_orders(e, foe_name))
	var logf := Lcars.frame("Combat log", ("round %d" % int(last["index"])) if not last.is_empty() else "awaiting first round", "tertiary")
	logf.custom_minimum_size.x = 560
	var lb := Lcars.vbox(2)
	if last.is_empty():
		lb.add_child(Lcars.para("Ships deploy at opposite edges of the arena. Dashed lines show what each ship will do next round.", Lcars.TEXT3, 13))
	for line in round_log(e, last):
		lb.add_child(Lcars.para(line[1], Lcars.tone_color(line[0]), 13))
	Lcars.body(logf).add_child(Lcars.scroll(lb))
	bottom.add_child(logf)
	root.add_child(bottom)
	if choice and not e.get("finished", false) and int(e["round"]) == 0:
		_encounter_choice(e, mine, foes, foe_name)

func _force(ships: Array, label: String) -> Control:
	var h := Lcars.hbox(8)
	h.add_child(Lcars.emblem(int(ships[0]["owner"]) if ships.size() > 0 else 0, 30))
	var v := Lcars.vbox(2)
	v.add_child(Lcars.label(label.to_upper(), "SubHeading", 17, Lcars.TEXT))
	var hull := 0
	var mx := 0
	var alive := 0
	for s in ships:
		if not s.get("hidden", false):
			hull += max(0, int(s["hull"]))
			mx += int(s["maxHull"])
		if s.get("alive", false) and not s.get("retreated", false):
			alive += 1
	v.add_child(Lcars.meter(hull, max(1, mx), Color("#8fd18a"), 5, 160))
	v.add_child(Lcars.label("%d/%d ships" % [alive, ships.size()], "Mono", 12))
	h.add_child(v)
	return h

func _ship_list(title: String, ships: Array, all: Array, sel: int, hostile: bool) -> Control:
	var f := Lcars.frame(title, "", "alert" if hostile else "primary")
	f.custom_minimum_size.x = 330
	var list := Lcars.vbox(6)
	for s in ships:
		list.add_child(_ship_card(s, all, sel, hostile))
	Lcars.body(f).add_child(Lcars.scroll(list))
	return f

func _name_of(all: Array, id: int) -> String:
	for s in all:
		if int(s["id"]) == id:
			return "%s #%d" % [short_name(s), id]
	return "#%d" % id

func _ship_card(s: Dictionary, all: Array, sel: int, hostile: bool) -> Control:
	var id := int(s["id"])
	var btn := Button.new()
	btn.name = "Ship%d" % id
	btn.theme_type_variation = "GhostButton"
	btn.custom_minimum_size.y = 96
	btn.disabled = s.get("hidden", false)
	var c := Lcars.owner_color(int(s["owner"]))
	var status := "DESTROYED" if not s.get("alive", true) else ("WITHDRAWN" if s.get("retreated", false) else "")
	var sb := Lcars.box(Color(c, 0.18) if sel == id else Color(Lcars.PANEL2, 0.85), 12, c if sel == id else Lcars.LINE2, 2 if sel == id else 1)
	btn.add_theme_stylebox_override("normal", sb)
	btn.add_theme_stylebox_override("disabled", sb)
	if status != "":
		btn.modulate = Color(1, 1, 1, 0.5)
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
	if s.get("hidden", false):
		v.add_child(Lcars.label("Cloaked · decloaks to fire", "Muted", 12))
	else:
		v.add_child(Lcars.meter(s["shields"], max(1, int(s["maxShields"])), Lcars.SHIELD, 3))
		v.add_child(Lcars.meter(max(0, int(s["hull"])), max(1, int(s["maxHull"])), Color("#8fd18a"), 4))
		var nx: Variant = s.get("next")
		var line := status
		if line == "":
			if nx is Dictionary:
				line = "%s%s%s  (%s)" % ["will " if hostile else "next: ", nx["maneuver"], (" → " + _name_of(all, int(nx["target"]))) if int(nx.get("target", 0)) != 0 else "", {"ai": "AI", "standing": "standing", "order": "ordered"}.get(nx.get("source", ""), "")]
			else:
				line = "beam %d · torp %d" % [int(s["beam"]), int(s["torpedo"])]
		v.add_child(Lcars.label(line, "", 12, Lcars.accent2() if nx is Dictionary and nx.get("source", "") == "order" else Lcars.TEXT2))
		var flags := []
		if s.get("cloaked", false) and s.get("alive", false):
			flags.append("Cloaked")
		if s.get("station", false):
			flags.append("%s · immobile" % s.get("stationType", "Station"))
		if s.get("disengaging", false):
			flags.append("Disengaging")
		if str(s.get("group", "")) != "":
			flags.append(str(s["group"]))
		if not hostile:
			flags.append("B%d · T%d" % [int(s["beam"]), int(s["torpedo"])])
		v.add_child(Lcars.label("  ·  ".join(flags), "Mono", 11, Lcars.TEXT3))
	for x in [v] + v.find_children("*", "Control", true, false):
		x.mouse_filter = Control.MOUSE_FILTER_IGNORE
	btn.add_child(v)
	btn.pressed.connect(func(): _pick(id))
	return btn

func _pick(id: int) -> void:
	var e := _enc()
	for s in e.get("ships", []):
		if int(s["id"]) == id and not s.get("hidden", false):
			if s.get("mine", false):
				ship = id
				if scope == "all":
					scope = "ship"
			else:
				target = id
	refresh()

func _selected() -> Dictionary:
	for s in _enc().get("ships", []):
		if int(s["id"]) == ship:
			return s
	return {}

func hail_outlook(e: Dictionary, foe: String) -> Array:
	var h: Dictionary = e.get("hail", {})
	var why := ("formally at war · trust %d · their strength %d vs ours %d" % [int(h.get("trust", 0)), int(h.get("theirStrength", 0)), int(h.get("ourStrength", 0)) ]) if h.get("atWar", false) else "not formally at war"
	if h.get("accepts", false):
		return ["If every one of our ships hails, the %s would accept a truce (%s)." % [foe, why], "good"]
	return ["The %s would refuse a truce unless they hail too (%s)." % [foe, why], "alert"]

func _orders(e: Dictionary, foe_name: String) -> Control:
	var sel := _selected()
	var scope_label := "all active ships" if scope == "all" else (("%s group" % sel.get("group", "")) if scope == "group" else short_name(sel))
	var f := Lcars.frame("Orders", scope_label, "secondary")
	f.name = "TacticalOrders"
	f.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var b := Lcars.body(f)
	var sc := Lcars.hbox(6)
	sc.add_child(Lcars.caption("Scope"))
	for pair in [["all", "All ships", "Every active ship"], ["group", "Group", "The selected ship's like-class tactical group"], ["ship", "Ship", "Only the selected ship"]]:
		var k: String = pair[0]
		var tg := Lcars.toggle(pair[1], scope == k, func(_on): scope = k; refresh(), pair[2])
		tg.name = "Scope_" + k
		tg.disabled = k != "all" and (sel.is_empty() or not is_active(sel))
		sc.add_child(tg)
	b.add_child(sc)
	var grid := Lcars.grid(6)
	grid.add_theme_constant_override("h_separation", 6)
	for mv in MANEUVERS:
		var key: String = mv["key"]
		var cannot := ""
		if e.get("finished", false):
			cannot = "The battle is over"
		elif scope == "ship" and sel.get("station", false) and IMMOBILE_BLOCKED.has(key):
			cannot = "Stations are immobile: they hold position and fire"
		elif key == "Retreat" and scope == "ship" and not sel.is_empty() and not sel.get("canRetreat", false):
			cannot = "No safe supplied withdrawal sector"
		elif scope != "all" and (sel.is_empty() or not is_active(sel)):
			cannot = "Select one of your active ships"
		var tone := "alert" if key in ["Retreat", "Ram"] else ("ghost" if key == "Hail" else "secondary")
		var hint: String = mv["hint"]
		if target != 0 and not key in ["Retreat", "Hail"]:
			hint += "\nTarget: #%d" % target
		var p := Lcars.pill(mv["name"], tone, func(): order(key), cannot, hint)
		p.name = "Maneuver_" + key
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
	var sel := _selected()
	var t := 0 if maneuver in ["Retreat", "Hail"] else target
	var args := {"maneuver": maneuver, "targetFleetId": t}
	if scope == "all":
		args["all"] = true
	elif scope == "group":
		args["groupId"] = int(sel.get("groupId", 0))
	else:
		args["fleetId"] = ship
	var r := app.core.invoke("tacticalOrder", args)
	if not r["ok"]:
		app.report_error(r["error"])
		return
	app.encounter = app.core.request("tactical")
	app.toast("%s ordered for %s ship%s%s." % [maneuver, Lcars.num(r["result"]), "" if int(r["result"]) == 1 else "s", (", target #%d" % t) if t != 0 else ""], "info", 1.8)
	refresh()

func _round() -> void:
	var r := app.core.invoke("tacticalRound", {})
	if not r["ok"]:
		app.report_error(r["error"])
		return
	app.encounter = r["result"]
	choice = false
	refresh()
	var rounds: Array = _enc().get("rounds", [])
	if host and rounds.size() > 0:
		host.play_round.call_deferred(rounds.back(), _enc())

func _auto() -> void:
	var r := app.core.invoke("tacticalAuto", {})
	if not r["ok"]:
		app.report_error(r["error"])
		return
	app.encounter = r["result"]
	choice = false
	refresh()

func _hail() -> void:
	app.core.invoke("tacticalOrder", {"all": true, "maneuver": "Hail"})
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
	row.add_child(Lcars.pill("Hail", "ghost", func(): app.close_modal(); _hail(), "", "Every ship hails and one round resolves"))
	col.add_child(row)
	app.open_modal.call_deferred(col, "Enemy contact", Vector2(720, 0), "alert")
	choice = false

## Human-readable log lines for a round: [[tone, text], ...].
static func round_log(e: Dictionary, r: Dictionary) -> Array:
	var out := []
	if r.is_empty():
		return out
	var names := {}
	for s in e.get("ships", []):
		names[int(s["id"])] = short_name(s)
	for st in r.get("ships", []):
		if st.get("decloaked", false):
			out.append(["warn", "%s decloaks." % names.get(int(st["id"]), "?")])
	for sv in r.get("salvos", []):
		var dmg := int(sv.get("beam", 0)) + int(sv.get("torpedo", 0))
		var txt := "%s → %s: %d damage%s%s%s" % [names.get(int(sv["from"]), "?"), names.get(int(sv["to"]), "?"), dmg, " (ram)" if sv.get("ram", false) else "", " (ambush)" if sv.get("ambush", false) else "", " (absorbed by shields)" if sv.get("absorbed", false) else ""]
		out.append(["alert" if sv.get("ram", false) else "info", txt])
	for id in r.get("lost", []):
		out.append(["alert", "%s destroyed." % names.get(int(id), "?")])
	for id in r.get("retreatedIds", []):
		out.append(["warn", "%s withdrew." % names.get(int(id), "?")])
	if r.get("truce", false):
		out.append(["good", "Truce agreed."])
	if r.get("ended", false):
		out.append(["good", "The engagement is over."])
	return out

# ---------------------------------------------------------------------------------------------
func _archive() -> void:
	var v := app.view
	var reps: Variant = app.core.request("replays")
	var replays: Array = reps if reps is Array else []
	var m := MarginContainer.new()
	m.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		m.add_theme_constant_override("margin_" + side, 16)
	add_child(m)
	var row := Lcars.hbox(14)
	m.add_child(row)
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
		var e: Variant = app.core.request("replay", {"index": int(archive_sel["index"])})
		if e is Dictionary:
			rb.add_child(_battle_host())
			host.load_encounter.call_deferred(e)
			for line in round_log(e, e["rounds"].back() if e["rounds"].size() > 0 else {}):
				rb.add_child(Lcars.para(line[1], Lcars.tone_color(line[0]), 13))
	elif archive_sel.get("kind", "") == "report":
		var rep: Variant = app.core.request("battleReport", {"index": int(archive_sel["index"])})
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

class RedAlert extends Control:
	func _process(_d: float) -> void:
		queue_redraw()
	func _draw() -> void:
		var t := Time.get_ticks_msec() / 1000.0
		var a: float = 0.45 + 0.55 * absf(sin(t * 3.0))
		draw_rect(Rect2(Vector2.ZERO, size), Color(Lcars.ALERT, a))
