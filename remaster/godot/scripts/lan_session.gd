class_name LanSession
extends Node
## LAN multiplayer for the desktop build (MULTIPLAYER.md, "LAN"; Simon 20 Nov 2026: desktop hotseat then LAN, no turn timer
## unless the host sets one). Lockstep on commands, not state sync:
##   1. Every peer holds the same canonical campaign at the start of a turn (the host's save; a hotseat game where every LAN
##      player is one of the humans). A peer plans on its own seat; the core journals each order it accepts.
##   2. End turn sends the journal to the host. With every human's journal in (or the host's turn timer run out), the host
##      sends the full set to everyone. Each peer reloads the canonical state, replays every seat's orders in seating order
##      (bridge "lanApply"), ends the turn with the same core (battles fought by the tactical AI, the same on every peer) and
##      sends the host its fingerprint.
##   3. Matching fingerprints: the turn stands. A mismatch stops the turn with a desync report (written to
##      user://lan/desync-turn-N.txt and shown), and the host's state goes to everyone.
## Transport: ENetMultiplayerPeer, JSON messages on one reliable channel; "Find games" is a UDP broadcast on port + 1.
## No internet relay, no accounts: direct IP on the local network.

signal changed   ## lobby or turn state changed (the lobby and the status strip redraw)

const PORT := 47474
const PROTOCOL := 1
const FIND_ASK := "BOTF_LAN_FIND 1"
const DEFAULT_EMPIRES := 5   ## the core's default lineup (newCampaign "empires"); a seat above it widens the lineup
const MAX_EMPIRES := 8       ## the playable powers (race slots 1..8)

var app: App
var peer: ENetMultiplayerPeer
var active := false        ## a LAN game (or lobby) is running
var is_host := false
var in_game := false
var my_empire := 0
var my_id := 0
var timer_secs := 0        ## host-set turn timer, 0 = off (the default)
var port := PORT
var lobby := {}            ## host: peer id -> {"empire": int, "name": String}; clients get a copy
var humans: Array = []     ## the human empires, ascending (seating order)
var turn := 0
var canonical := {}        ## the save blob every peer holds at the start of this turn
var sent := false          ## this seat's orders are in for this turn
var orders := {}           ## host: empire -> journal for this turn
var prints := {}           ## host: empire -> fingerprint after this turn
var dropped := {}          ## host: empires whose player left (they send no orders; their seat plans nothing)
var deadline := 0.0        ## the turn timer's end (seconds, Time.get_ticks_msec()/1000), 0 = none
var last_fp := ""          ## this peer's fingerprint after the last turn
var history: Array = []    ## [turn, fingerprint, "ok"|"desync"] per resolved turn
var last_status := ""
var auto := {}             ## BOTF_LAN_AUTO test driver settings (see _auto_*)
var _udp: PacketPeerUDP    ## host: answers "find games"
var _find: PacketPeerUDP   ## client: collects answers
var found: Array = []      ## client: [{"address", "port", "name", "players"}]
var _host_faction := 1
var _round_deadline := 0.0
var _seed := 0
var domination := true   ## C7: the host's New Campaign domination setting (the opening save carries it to every seat)

func _init(app_: App) -> void:
	app = app_
	name = "Lan"

# ---------------------------------------------------------------------------------------------
# Hosting and joining
# ---------------------------------------------------------------------------------------------
## Opens a game on `port_` for the local network. `faction` is the host's power, `timer` the per-turn seconds (0 = off).
func host(port_: int, faction: int, timer: int, seed_: int = 0) -> String:
	close()
	peer = ENetMultiplayerPeer.new()
	var err := peer.create_server(port_, 7)
	if err != OK:
		peer = null
		return "Cannot open port %d (error %d)." % [port_, err]
	port = port_
	peer.peer_disconnected.connect(peer_left)
	is_host = true
	active = true
	my_id = 1
	_host_faction = faction
	_seed = seed_
	timer_secs = maxi(timer, 0)
	lobby = {1: {"empire": faction, "name": app.session.empire_name(faction)}}
	_udp = PacketPeerUDP.new()
	if _udp.bind(port_ + 1) != OK:
		_udp = null   # find-games is a convenience; direct IP still works
	_status("Hosting on port %d. Waiting for players." % port_)
	return ""

## Joins the game at `address`:`port_` asking for `faction` (the host gives another free power if it is taken).
func join(address: String, port_: int, faction: int) -> String:
	close()
	peer = ENetMultiplayerPeer.new()
	var err := peer.create_client(address, port_)
	if err != OK:
		peer = null
		return "Cannot reach %s:%d (error %d)." % [address, port_, err]
	port = port_
	is_host = false
	active = true
	_host_faction = faction
	_status("Connecting to %s:%d." % [address, port_])
	return ""

## Broadcasts "find games" on the local network; answers arrive in `found` over the next second.
func find_games(port_: int = PORT) -> void:
	found.clear()
	_find = PacketPeerUDP.new()
	_find.set_broadcast_enabled(true)
	_find.bind(0)
	_find.set_dest_address("255.255.255.255", port_ + 1)
	_find.put_packet(FIND_ASK.to_utf8_buffer())
	_find.set_dest_address("127.0.0.1", port_ + 1)   # a game hosted on this PC
	_find.put_packet(FIND_ASK.to_utf8_buffer())

func close() -> void:
	if peer != null:
		peer.close()
	peer = null
	if _udp != null:
		_udp.close()
	_udp = null
	active = false
	in_game = false
	is_host = false
	sent = false
	lobby = {}
	orders = {}
	prints = {}
	dropped = {}
	deadline = 0.0
	changed.emit()

## Host: starts the campaign with every lobby player as a hotseat human, then sends everyone the opening save.
func start_game() -> String:
	if not is_host:
		return "Only the host starts the game."
	if lobby.size() < 2:
		return "Waiting for at least one other player."
	var extra := []
	var top := maxi(DEFAULT_EMPIRES, _host_faction)
	for id in lobby:
		top = maxi(top, int(lobby[id]["empire"]))
		if int(id) != 1:
			extra.append(int(lobby[id]["empire"]))
	# The campaign must hold every seated power: a player on power 6-8 needs the lineup to reach it (R8: setHotseat failed
	# quietly and the game started with no second human).
	var opts := {"faction": _host_faction, "hotseat": extra, "empires": top}
	if _seed != 0:
		opts["seed"] = _seed
	opts["domination"] = domination
	if not app.start_campaign(opts):
		return "The campaign did not start."
	humans = app.hotseat_humans().duplicate()
	my_empire = _host_faction
	canonical = app.session.save_blob()
	turn = int(canonical.get("turn", 0))
	in_game = true
	_send_all({"t": "start", "blob": canonical, "humans": humans, "lobby": _lobby_wire(), "timer": timer_secs})
	_begin_turn()
	return ""

# ---------------------------------------------------------------------------------------------
# The turn
# ---------------------------------------------------------------------------------------------
## End turn on a LAN game: this seat's orders go to the host; the turn resolves when every human's are in.
func end_turn_pressed() -> void:
	if not in_game or sent:
		return
	var j: Dictionary = app.session.core.invoke("journal", {})
	var journal: Array = j["result"] if j["ok"] and j["result"] is Array else []
	sent = true
	deadline = 0.0
	var msg := {"t": "orders", "turn": turn, "empire": my_empire, "journal": journal}
	if is_host:
		_on_orders(1, msg)
	else:
		_send(1, msg)
	_status("Orders sent (%d). Waiting for the other players." % journal.size())

## Host: whether peer `from` holds the seat the message names. The peer id is set by the transport, the empire by the sender, so a
## message counts only for the seat the host gave that peer (R8, Gemini audit: any peer could send orders for another seat).
func _from_seat(from: int, m: Dictionary) -> bool:
	return lobby.has(from) and int(lobby[from].get("empire", -1)) == int(m.get("empire", -2))

func _on_orders(from: int, m: Dictionary) -> void:
	if not is_host or int(m.get("turn", -1)) != turn or not _from_seat(from, m):
		return
	orders[int(m["empire"])] = m.get("journal", [])
	_status("Turn %d: orders from %s." % [turn, ", ".join(_names(orders.keys()))])
	_try_resolve()

func _try_resolve(force: bool = false) -> void:
	for h in humans:
		if not orders.has(int(h)) and not dropped.has(int(h)) and not force:
			return
	var seats := []
	for h in humans:
		seats.append({"empire": int(h), "journal": orders.get(int(h), [])})
	orders = {}
	prints = {}
	var msg := {"t": "turn", "turn": turn, "seats": seats}
	_send_all(msg)
	_apply_turn(msg)

## Every peer: replays the turn on the canonical state and reports its fingerprint.
func _apply_turn(m: Dictionary) -> void:
	var core := app.session.core
	var r := core.invoke("load", {"data": canonical.get("data", ""), "scenario": canonical.get("scenario", "campaign")})
	if not r["ok"]:
		_status("Cannot reload the turn: %s" % r["error"])
		return
	var seats: Array = m.get("seats", []).duplicate(true)
	if _inject_desync:
		_inject_desync = false
		seats.append({"empire": my_empire, "journal": [{"method": "submit", "id": 0, "args": {"command": {"type": "AllocateResearch", "researchAllocation": [0, 0, 0, 0, 0, 100]}}}]})
	var applied := core.invoke("lanApply", {"seats": seats})
	if not applied["ok"]:
		_status("The turn's orders did not apply: %s" % applied["error"])
		return
	print("LAN turn %d replay: %d orders applied, %d refused %s" % [turn, int(applied["result"]["applied"]), int(applied["result"]["skipped"]), str(applied["result"]["refusals"])])
	var res := core.invoke("endTurn", {})
	var guard := 0
	while res["ok"] and str(res["result"].get("status", "")) == "battle" and guard < 256:
		core.invoke("tacticalAuto", {})   # LAN battles are fought by the tactical AI on every peer (the same result)
		res = core.invoke("endTurn", {})
		guard += 1
	if not res["ok"]:
		_status("The turn did not resolve: %s" % res["error"])
		return
	var fp := core.invoke("fingerprint", {})
	last_fp = str(fp["result"]) if fp["ok"] else ""
	canonical = app.session.save_blob()
	turn = int(canonical.get("turn", turn + 1))
	_show_own_seat(res["result"].get("briefing"))
	var msg := {"t": "fp", "turn": turn, "empire": my_empire, "fp": last_fp}
	if is_host:
		_on_fp(1, msg)
	else:
		_send(1, msg)

func _on_fp(from: int, m: Dictionary) -> void:
	if not is_host or int(m.get("turn", -1)) != turn or not _from_seat(from, m):
		return
	prints[int(m["empire"])] = str(m.get("fp", ""))
	for h in humans:
		if not prints.has(int(h)) and not dropped.has(int(h)):
			return
	var mine := str(prints.get(my_empire, ""))
	var ok := true
	for e in prints:
		ok = ok and str(prints[e]) == mine
	var wire := {}
	for e in prints:
		wire[str(e)] = prints[e]
	var verdict := {"t": "verdict", "turn": turn, "ok": ok, "prints": wire}
	if not ok:
		verdict["blob"] = canonical
	_send_all(verdict)
	_on_verdict(1, verdict)

func _on_verdict(_from: int, m: Dictionary) -> void:
	var ok := bool(m.get("ok", false))
	history.append([int(m.get("turn", 0)), last_fp, "ok" if ok else "desync"])
	if ok:
		print("LAN turn %d fingerprint %s match (%s)" % [int(m["turn"]), last_fp, "host" if is_host else "client"])
		if not auto.is_empty() and (int(m["turn"]) <= 3 or int(m["turn"]) % 4 == 0):
			await get_tree().create_timer(0.6).timeout
			await _auto_shot("turn%02d" % int(m["turn"]))
		_begin_turn()
		return
	var report := _desync_report(m)
	push_warning(report)
	print("LAN DESYNC turn %d (%s)" % [int(m["turn"]), "host" if is_host else "client"])
	if not is_host and m.get("blob") is Dictionary:
		canonical = m["blob"]   # the host's state goes to everyone
		var core := app.session.core
		core.invoke("load", {"data": canonical.get("data", ""), "scenario": canonical.get("scenario", "campaign")})
		_show_own_seat(null)
	_open_desync(m, report)
	if not auto.is_empty():
		await get_tree().create_timer(1.0).timeout
		await _auto_shot("desync%02d" % int(m.get("turn", 0)))
		app.close_modal()
	_begin_turn()

func _begin_turn() -> void:
	sent = false
	deadline = (Time.get_ticks_msec() / 1000.0 + timer_secs) if timer_secs > 0 else 0.0
	_round_deadline = (deadline + 10.0) if timer_secs > 0 else 0.0   # host: a seat silent 10 s past the timer plans nothing
	_status("Turn %d. Plan your turn%s." % [turn, (", %d s on the clock" % timer_secs) if timer_secs > 0 else ""])
	if not auto.is_empty():
		_auto_plan.call_deferred()

## Seats this peer's human on the canonical state for planning and shows their view and briefing.
func _show_own_seat(brief: Variant) -> void:
	var core := app.session.core
	var saved = core.request("save")   # who holds the chair now (one serialisation, not two)
	var blob: Dictionary = saved if saved is Dictionary else {}
	if int(blob.get("player", my_empire)) != my_empire and str(app.session.view.get("meta", {}).get("outcome", "ongoing")) == "ongoing":
		var r := core.invoke("switchSeat", {"empire": my_empire})
		if not r["ok"]:
			_status("Cannot take the chair: %s" % r["error"])
	core.invoke("journal", {})   # nothing of the replay belongs to this seat's next journal
	app.session.refresh()
	app.view = app.session.view
	var b = core.request("briefing")
	app.briefing = b if b is Dictionary else brief
	app.close_all_modals()
	app.refresh()
	if app.screen != "galaxy":
		app.goto("galaxy", false)
	var outcome := str(app.view.get("meta", {}).get("outcome", "ongoing"))
	if outcome != "ongoing":
		app.open_overlay("end")
	elif auto.is_empty() and app.briefing is Dictionary:
		app.open_overlay("briefing")

# ---------------------------------------------------------------------------------------------
# Messages
# ---------------------------------------------------------------------------------------------
func _process(_delta: float) -> void:
	_poll_find()
	if peer == null:
		return
	if peer.get_connection_status() == MultiplayerPeer.CONNECTION_DISCONNECTED:
		peer = null   # the host closed the game, or never answered
		_status("Disconnected from the host." if not is_host else "The game is closed.")
		return
	peer.poll()
	while peer != null and peer.get_available_packet_count() > 0:
		var from := peer.get_packet_peer()
		var raw := peer.get_packet().get_string_from_utf8()
		var m = JSON.parse_string(raw)
		if m is Dictionary:
			_on_message(from, m)
	if peer == null:
		return
	var st := peer.get_connection_status()
	if not is_host and my_id == 0 and st == MultiplayerPeer.CONNECTION_CONNECTED:
		my_id = peer.get_unique_id()
		var ci = app.session.core.request("contentInfo")
		_send(1, {"t": "hello", "protocol": PROTOCOL, "faction": _host_faction, "pack": ci if ci is Dictionary else {}})
	if deadline > 0.0 and not sent and Time.get_ticks_msec() / 1000.0 >= deadline:
		_status("Time is up: the turn ends with the orders given.")
		end_turn_pressed()
	if is_host and in_game and _round_deadline > 0.0 and Time.get_ticks_msec() / 1000.0 >= _round_deadline and not orders.is_empty():
		_round_deadline = 0.0
		_status("A player's orders did not arrive in time: their seat plans nothing this turn.")
		_try_resolve(true)


func _on_message(from: int, m: Dictionary) -> void:
	match str(m.get("t", "")):
		"hello": _on_hello(from, m)
		"lobby":
			lobby = {}
			for k in m.get("lobby", {}):
				lobby[int(k)] = m["lobby"][k]
			if lobby.has(my_id):
				my_empire = int(lobby[my_id]["empire"])
			_status("In the lobby as %s. Waiting for the host to start." % app.session.empire_name(my_empire))
		"refused":
			_status("The host refused: %s" % m.get("reason", ""))
			close()
		"start": _on_start(m)
		"orders": _on_orders(from, m)
		"turn": _apply_turn(m)
		"fp": _on_fp(from, m)
		"verdict": _on_verdict(from, m)

func _on_hello(from: int, m: Dictionary) -> void:
	if not is_host:
		return
	if int(m.get("protocol", 0)) != PROTOCOL:
		_send(from, {"t": "refused", "reason": "a different game version"})
		return
	var mine = app.session.core.request("contentInfo")
	var theirs: Dictionary = m.get("pack", {})
	if mine is Dictionary and str(mine.get("checksum", "")) != str(theirs.get("checksum", "")):
		_send(from, {"t": "refused", "reason": "a different content pack (%s)" % str(mine.get("id", ""))})
		return
	if in_game:
		_send(from, {"t": "refused", "reason": "the game has started"})
		return
	var taken := []
	for id in lobby:
		taken.append(int(lobby[id]["empire"]))
	var want := int(m.get("faction", 0))
	if want < 1 or taken.has(want):
		want = 0
		for e in range(1, MAX_EMPIRES + 1):
			if not taken.has(e):
				want = e
				break
	if want == 0:
		_send(from, {"t": "refused", "reason": "every power is taken"})
		return
	lobby[from] = {"empire": want, "name": app.session.empire_name(want)}
	_send_all({"t": "lobby", "lobby": _lobby_wire()})
	_status("%s joined as %s." % ["Player %d" % lobby.size(), app.session.empire_name(want)])
	if not auto.is_empty() and auto.get("role", "") == "host" and lobby.size() >= int(auto.get("players", 2)):
		var err := start_game()
		if err != "":
			_status(err)

func _on_start(m: Dictionary) -> void:
	canonical = m.get("blob", {})
	humans = m.get("humans", [])
	timer_secs = int(m.get("timer", 0))
	lobby = {}
	for k in m.get("lobby", {}):
		lobby[int(k)] = m["lobby"][k]
	if lobby.has(my_id):
		my_empire = int(lobby[my_id]["empire"])
	if not app.load_game({"data": canonical.get("data", ""), "scenario": canonical.get("scenario", "campaign"), "name": "LAN game"}):
		_status("Cannot load the host's game.")
		return
	turn = int(canonical.get("turn", 0))
	in_game = true
	_show_own_seat(null)
	_begin_turn()

func _send(to: int, m: Dictionary) -> void:
	if peer == null:
		return
	peer.set_target_peer(to)
	peer.set_transfer_mode(MultiplayerPeer.TRANSFER_MODE_RELIABLE)
	peer.put_packet(JSON.stringify(m).to_utf8_buffer())

func _send_all(m: Dictionary) -> void:
	if peer == null:
		return
	peer.set_target_peer(MultiplayerPeer.TARGET_PEER_BROADCAST)
	peer.set_transfer_mode(MultiplayerPeer.TRANSFER_MODE_RELIABLE)
	peer.put_packet(JSON.stringify(m).to_utf8_buffer())

func _lobby_wire() -> Dictionary:
	var out := {}
	for id in lobby:
		out[str(id)] = lobby[id]
	return out

func _poll_find() -> void:
	if _udp != null:
		while _udp.get_available_packet_count() > 0:
			var ask := _udp.get_packet().get_string_from_utf8()
			if ask == FIND_ASK and not in_game:
				var reply := {"name": "%s's game" % app.session.empire_name(_host_faction), "port": port, "players": lobby.size(), "timer": timer_secs}
				_udp.set_dest_address(_udp.get_packet_ip(), _udp.get_packet_port())
				_udp.put_packet(JSON.stringify(reply).to_utf8_buffer())
	if _find != null:
		while _find.get_available_packet_count() > 0:
			var d = JSON.parse_string(_find.get_packet().get_string_from_utf8())
			if d is Dictionary:
				d["address"] = _find.get_packet_ip()
				var dup := false
				for f in found:
					dup = dup or (f["address"] == d["address"] and int(f["port"]) == int(d["port"]))
				if not dup:
					found.append(d)
					changed.emit()

func peer_left(id: int) -> void:
	if not is_host or not lobby.has(id):
		return
	var e := int(lobby[id]["empire"])
	dropped[e] = true
	_status("%s left the game; their seat plans nothing until they rejoin." % app.session.empire_name(e))
	if in_game:
		_try_resolve()

# ---------------------------------------------------------------------------------------------
# Status, desync report
# ---------------------------------------------------------------------------------------------
func _status(text: String) -> void:
	last_status = text
	print("LAN: " + text)
	changed.emit()

func seconds_left() -> int:
	return maxi(0, int(ceil(deadline - Time.get_ticks_msec() / 1000.0))) if deadline > 0.0 else -1

func _names(ids: Array) -> PackedStringArray:
	var out := PackedStringArray()
	for e in ids:
		out.append(app.session.empire_name(int(e)))
	return out

func _desync_report(m: Dictionary) -> String:
	var lines := PackedStringArray()
	lines.append("BOTF LAN desync report")
	lines.append("Turn %d, %s" % [int(m.get("turn", 0)), Time.get_datetime_string_from_system()])
	lines.append("This peer: %s (%s), fingerprint %s" % [app.session.empire_name(my_empire), "host" if is_host else "client", last_fp])
	var pr: Dictionary = m.get("prints", {})
	var host_fp := ""
	for e in pr:
		if int(e) == _host_empire():
			host_fp = str(pr[e])
	for e in pr:
		lines.append("  %s%s: %s%s" % [app.session.empire_name(int(e)), " (host)" if int(e) == _host_empire() else "", pr[e], "" if str(pr[e]) == host_fp else "  <- differs from the host"])
	lines.append("The host's state was sent to every peer; play continues from it.")
	lines.append("Turns so far: " + ", ".join(history.map(func(h): return "%d %s" % [h[0], h[2]])))
	var text := "\n".join(lines)
	DirAccess.make_dir_recursive_absolute("user://lan")
	var f := FileAccess.open(_report_path(m), FileAccess.WRITE)
	if f != null:
		f.store_string(text)
	return text

func _report_path(m: Dictionary) -> String:
	return "user://lan/desync-turn-%d-%s.txt" % [int(m.get("turn", 0)), "host" if is_host else "client"]

func _host_empire() -> int:
	return int(lobby[1]["empire"]) if lobby.has(1) else (my_empire if is_host else 0)

func _open_desync(m: Dictionary, report: String) -> void:
	var col := Lcars.vbox(10)
	col.name = "LanDesync"
	col.add_child(Lcars.label("DESYNC ON TURN %d" % int(m.get("turn", 0)), "Heading", 34, Lcars.ALERT))
	col.add_child(Lcars.para("The players' games no longer match after this turn. The host's game has been sent to everyone and play goes on from it.", Lcars.TEXT, 15))
	var mono := Lcars.label(report, "Mono", 13)
	mono.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	col.add_child(mono)
	col.add_child(Lcars.caption("Report saved to %s" % ProjectSettings.globalize_path(_report_path(m))))
	col.add_child(Lcars.pill("Continue", "primary", func(): app.close_modal(), "", "", "Esc"))
	app.open_modal(col, "LAN game", Vector2(860, 0), "alert")

# ---------------------------------------------------------------------------------------------
# Test driver (BOTF_LAN_AUTO): "host:<port>:<turns>[:<timer>]" or "join:<address>:<port>:<turns>". Each seat gives a few
# orders a turn (research split, a build at its capital) and ends its turn; BOTF_LAN_DESYNC=<turn> makes this peer add
# one order the others never get on that turn, to show the desync report. Used by tools/lan_capture.gd's proof runs.
# ---------------------------------------------------------------------------------------------
func start_auto(spec: String) -> void:
	var p := spec.split(":")
	if p.size() < 3:
		return
	app.open_lan_lobby()
	get_tree().create_timer(2.5).timeout.connect(func(): _auto_shot("lobby"))
	var think := OS.get_environment("BOTF_LAN_THINK")   # seconds this seat takes to plan (a slow player, for the timer proof)
	if p[0] == "host":
		auto = {"role": "host", "turns": int(p[2]), "players": 2}
		var err := host(int(p[1]), 1, int(p[3]) if p.size() > 3 else 0, 424242)
		if err != "":
			_status(err)
	else:
		auto = {"role": "join", "turns": int(p[3])}
	if think != "":
		auto["think"] = float(think)
	if p[0] != "host":
		var err := join(p[1], int(p[2]), 2)
		if err != "":
			_status(err)

func _auto_plan() -> void:
	if auto.is_empty() or not in_game or sent:
		return
	if turn >= int(auto.get("turns", 10)) + 1 or str(app.view.get("meta", {}).get("outcome", "ongoing")) != "ongoing":
		auto["done"] = true
		print("LAN AUTO DONE turn %d history %s" % [turn, str(history)])
		await get_tree().create_timer(1.0).timeout
		await _auto_shot("done")
		await get_tree().create_timer(3.0).timeout   # the other peer's last verdict is on its way
		app.get_tree().quit()
		return
	var planning := turn
	await get_tree().create_timer(float(auto.get("think", 0.4))).timeout
	if sent or turn != planning:   # the turn timer ended this seat's turn while it was still thinking
		print("LAN AUTO turn %d: the timer ended this seat's turn before it gave orders" % planning)
		return
	var core := app.session.core
	var split := [40, 20, 10, 10, 10, 10] if turn % 2 == 0 else [10, 10, 40, 20, 10, 10]
	core.invoke("submit", {"command": {"type": "AllocateResearch", "researchAllocation": split}})
	var cols: Array = app.view.get("colonies", [])
	if not cols.is_empty():
		var sid := int(cols[0].get("systemId", 0))
		core.invoke("submit", {"command": {"type": "EnqueueBuilding", "systemId": sid, "building": "Factory" if turn % 3 == 0 else "Farm"}})
	var desync := int(OS.get_environment("BOTF_LAN_DESYNC")) if OS.get_environment("BOTF_LAN_DESYNC") != "" else -1
	_inject_desync = desync == turn   # forced desync for the proof: this peer alone replays one extra order this turn
	app.session.refresh()
	app.view = app.session.view
	end_turn_pressed()

var _inject_desync := false

func _auto_shot(tag: String) -> void:
	var dir := OS.get_environment("BOTF_LAN_SHOTS")
	if dir == "" or DisplayServer.get_name() == "headless":
		return
	DirAccess.make_dir_recursive_absolute(dir)
	await RenderingServer.frame_post_draw
	var img := get_viewport().get_texture().get_image()
	img.save_png("%s/%s-%s.png" % [dir, "host" if is_host else "client", tag])
