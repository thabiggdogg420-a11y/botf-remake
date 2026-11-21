class_name LanLobby
extends RefCounted
## The LAN game modal (MULTIPLAYER.md "LAN"): host a game (port, power, turn timer off by default) or join one by address or
## from "Find games" on the local network; the lobby lists who is in and the host starts the campaign.

const POWERS := [1, 2, 3, 4, 5]

static func open(app: App) -> Control:
	var lan: LanSession = app.lan
	var root := Lcars.vbox(12)
	root.name = "LanLobby"
	root.add_child(Lcars.para("Play with friends on the same network. Every player runs the game on their own PC; the host's game is the one everyone plays. Turns are taken together: the turn resolves when every player has ended theirs.", Lcars.TEXT, 15))
	var power := OptionButton.new()
	power.name = "LanPower"
	for e in POWERS:
		power.add_item(app.session.empire_name(e), e)
	var port := SpinBox.new()
	port.name = "LanPort"
	port.min_value = 1024
	port.max_value = 65000
	port.value = LanSession.PORT
	var timer := SpinBox.new()
	timer.name = "LanTimer"
	timer.min_value = 0
	timer.max_value = 3600
	timer.step = 15
	timer.value = 0
	timer.suffix = "s"
	timer.tooltip_text = "Seconds each player has to plan a turn. 0 = no timer (the default)."
	var addr := LineEdit.new()
	addr.name = "LanAddress"
	addr.text = "127.0.0.1"
	addr.custom_minimum_size.x = 220
	var row := Lcars.hbox(10)
	row.add_child(Lcars.label("Your power", "", 15))
	row.add_child(power)
	row.add_child(Lcars.label("Port", "", 15))
	row.add_child(port)
	root.add_child(row)

	var host_box := Lcars.hbox(10)
	host_box.add_child(Lcars.label("Turn timer", "", 15))
	host_box.add_child(timer)
	host_box.add_child(Lcars.caption("0 = off"))
	var dom := CheckBox.new()
	dom.name = "LanDomination"
	dom.text = "Domination victory"
	dom.button_pressed = lan.domination
	dom.tooltip_text = "Off: conquest (every colony) is the military victory. Every player's game follows the host's choice."
	dom.toggled.connect(func(on): lan.domination = on)
	host_box.add_child(dom)
	host_box.add_child(Lcars.pill("Host a game", "primary", func():
		var err := lan.host(int(port.value), power.get_selected_id(), int(timer.value))
		if err != "":
			app.toast(err, "alert")))
	root.add_child(host_box)

	var join_box := Lcars.hbox(10)
	join_box.add_child(Lcars.label("Host address", "", 15))
	join_box.add_child(addr)
	join_box.add_child(Lcars.pill("Join", "secondary", func():
		var err := lan.join(addr.text.strip_edges(), int(port.value), power.get_selected_id())
		if err != "":
			app.toast(err, "alert")))
	join_box.add_child(Lcars.pill("Find games", "tertiary", func(): lan.find_games(int(port.value)), "", "Look for games hosted on this network"))
	root.add_child(join_box)

	var found := Lcars.vbox(4)
	found.name = "LanFound"
	root.add_child(found)
	var players := Lcars.vbox(4)
	players.name = "LanPlayers"
	root.add_child(players)
	var status := Lcars.caption("")
	status.name = "LanStatus"
	root.add_child(status)
	var acts := Lcars.hbox(10)
	var start := Lcars.pill("Start the game", "primary", func():
		var err := lan.start_game()
		if err != "":
			app.toast(err, "warn")
		else:
			app.close_modal())
	start.name = "LanStart"
	acts.add_child(start)
	acts.add_child(Lcars.pill("Leave", "ghost", func(): lan.close(); app.close_modal(), "", "", "Esc"))
	root.add_child(acts)

	var redraw := func():
		if not is_instance_valid(root):
			return
		status.text = lan.last_status
		start.visible = lan.is_host and not lan.in_game
		start.disabled = lan.lobby.size() < 2
		for c in players.get_children():
			c.queue_free()
		if not lan.lobby.is_empty():
			players.add_child(Lcars.label("PLAYERS", "Heading", 16, Lcars.accent()))
			for id in lan.lobby:
				var pr := Lcars.hbox(8)
				pr.add_child(Lcars.emblem(int(lan.lobby[id]["empire"]), 22))
				pr.add_child(Lcars.label("%s%s" % [lan.lobby[id]["name"], "  (host)" if int(id) == 1 else ""], "", 15))
				players.add_child(pr)
		for c in found.get_children():
			c.queue_free()
		for g in lan.found:
			var fr := Lcars.hbox(8)
			fr.add_child(Lcars.label("%s · %s:%d · %d player%s%s" % [g.get("name", "Game"), g["address"], int(g["port"]), int(g.get("players", 1)), "" if int(g.get("players", 1)) == 1 else "s", (" · %d s timer" % int(g["timer"])) if int(g.get("timer", 0)) > 0 else ""], "", 14))
			fr.add_child(Lcars.pill("Join", "secondary", func():
				addr.text = str(g["address"])
				port.value = int(g["port"])
				var err := lan.join(str(g["address"]), int(g["port"]), power.get_selected_id())
				if err != "":
					app.toast(err, "alert")))
			found.add_child(fr)
		if lan.in_game and is_instance_valid(root) and root.is_inside_tree():
			app.close_modal()
	lan.changed.connect(redraw)
	root.tree_exiting.connect(func():
		if lan.changed.is_connected(redraw):
			lan.changed.disconnect(redraw))
	redraw.call()
	return app.open_modal(root, "LAN game", Vector2(860, 0), "primary")
