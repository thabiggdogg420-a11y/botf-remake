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
## E6 (22 Nov 2026): human-planned battles, AI takeover and rejoin.
var roster: Array = []          ## the humans playing the turn being resolved (a player who dropped is not in it; the AI runs their power)
var resolving := false          ## host: a turn is being resolved (a rejoin waits for the next turn start)
var in_battle := false          ## a battle with human commanders is being fought round by round
var battle_idx := 0             ## which battle of this turn (1, 2, ...)
var battle_round := 0           ## the round awaiting orders
var battle_parts: Array = []    ## the humans commanding in the current battle
var battle_sent := false        ## this seat's orders for the round are in
var _battle_orders := {}        ## host: "turn:battle:round" -> {empire: [orders]}
var _battle_buffer := {}        ## "turn:battle:round" -> a bround that arrived before this peer reached that round
var _battle_deadline := 0.0

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
	roster = []
	resolving = false
	in_battle = false
	battle_sent = false
	_battle_orders = {}
	_battle_buffer = {}
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
	if not in_game or sent or in_battle:
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
	if resolving:
		return
	for h in humans:
		if not orders.has(int(h)) and not dropped.has(int(h)) and not force:
			return
	# E6: a player who left is not in this turn's roster: their power is run by the AI (unless their orders did arrive first).
	var seats := []
	var present := []
	for h in humans:
		if orders.has(int(h)) or not dropped.has(int(h)):
			seats.append({"empire": int(h), "journal": orders.get(int(h), [])})
			present.append(int(h))
	if present.is_empty():
		_status("Every player has left; the turn waits.")
		return
	orders = {}
	prints = {}
	resolving = true
	var msg := {"t": "turn", "turn": turn, "seats": seats, "roster": present}
	_send_all(msg)
	_apply_turn(msg)

## Every peer: replays the turn on the canonical state and reports its fingerprint.
func _apply_turn(m: Dictionary) -> void:
	var core := app.session.core
	var r := core.invoke("load", {"data": canonical.get("data", ""), "scenario": canonical.get("scenario", "campaign")})
	if not r["ok"]:
		_status("Cannot reload the turn: %s" % r["error"])
		return
	roster = []
	for h in m.get("roster", humans):
		roster.append(int(h))
	var rr := core.invoke("lanRoster", {"humans": roster})   # E6: a dropped player's power goes to the AI, a rejoined one's comes back
	if not rr["ok"]:
		_status("Cannot set the turn's players: %s" % rr["error"])
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
	battle_idx = 0
	_resume_turn(core.invoke("endTurn", {}))

## Runs the turn on until it is done or a battle with a human commander needs orders. A battle between AI fleets (or one whose
## humans have all left) is fought by the tactical AI, the same on every peer.
func _resume_turn(first: Dictionary) -> void:
	var core := app.session.core
	var res := first
	var guard := 0
	while res["ok"] and str(res["result"].get("status", "")) == "battle" and guard < 256:
		if _begin_battle():
			return
		core.invoke("tacticalAuto", {})
		res = core.invoke("endTurn", {})
		guard += 1
	_finish_turn(res)

func _finish_turn(res: Dictionary) -> void:
	var core := app.session.core
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

# ---------------------------------------------------------------------------------------------
# Human-planned battles (E6, 22 Nov 2026). Every peer reaches the same battle with the same state (the turn is deterministic), so
# a round is lockstep on orders: each human commanding in it plans on their own seat (the core journals their tacticalOrder
# calls), sends them to the host with "borders", and the host answers every peer with "bround" holding every seat's orders. Each
# peer then applies the other seats' orders, resolves the round and (a commander) plays it. A seat that left sends nothing and its
# ships keep their last orders; its power goes to the AI at the next turn start. The end-of-turn fingerprint covers the battle.
# ---------------------------------------------------------------------------------------------
func _battle_key(rnd: int = -1) -> String:
	return "%d:%d:%d" % [turn, battle_idx, battle_round if rnd < 0 else rnd]

func _is_commander() -> bool:
	return battle_parts.has(my_empire)

## True when the pending battle has a human commander in this turn's roster (the battle then waits for orders).
func _begin_battle() -> bool:
	app.session.refresh()
	app.view = app.session.view
	var parts: Array = []
	for h in app.battle_humans():
		if roster.has(int(h)):
			parts.append(int(h))
	if parts.is_empty():
		return false
	battle_idx += 1
	battle_round = 0
	battle_parts = parts
	in_battle = true
	battle_sent = false
	var core := app.session.core
	if _is_commander():
		core.invoke("battleSeat", {"empire": my_empire})
		core.invoke("battleJournal", {})
		app.session.fetch_encounter()
		app.lan_open_battle()
		_status("Battle %d: plan your ships, then resolve the round. The other commanders resolve with you." % battle_idx)
	else:
		_status("Battle %d: the other players are commanding it." % battle_idx)
	_start_battle_round()
	return true

func _start_battle_round() -> void:
	battle_sent = false
	_battle_deadline = (Time.get_ticks_msec() / 1000.0 + timer_secs) if timer_secs > 0 else 0.0
	if not auto.is_empty() and _is_commander():
		_auto_battle_round.call_deferred()
	if is_host:
		_battle_check()
	var key := _battle_key()
	if _battle_buffer.has(key):
		var m: Dictionary = _battle_buffer[key]
		_battle_buffer.erase(key)
		_on_bround(1, m)

## A commander presses Resolve: their orders go to the host.
func battle_resolve_pressed() -> void:
	if not in_battle or battle_sent or not _is_commander():
		return
	var j: Dictionary = app.session.core.invoke("battleJournal", {})
	var list: Array = j["result"] if j["ok"] and j["result"] is Array else []
	battle_sent = true
	_battle_deadline = 0.0
	var msg := {"t": "borders", "turn": turn, "battle": battle_idx, "round": battle_round, "empire": my_empire, "orders": list}
	if is_host:
		_on_borders(1, msg)
	else:
		_send(1, msg)
	_status("Battle orders sent (%d). Waiting for the other commanders." % list.size())

func _on_borders(from: int, m: Dictionary) -> void:
	if not is_host or not _from_seat(from, m) or int(m.get("turn", -1)) != turn:
		return
	var key := "%d:%d:%d" % [turn, int(m.get("battle", 0)), int(m.get("round", 0))]
	if not _battle_orders.has(key):
		_battle_orders[key] = {}
	_battle_orders[key][int(m["empire"])] = m.get("orders", [])
	_battle_check()

## Host: with every commander's orders for the current round in (a seat that left counts as in), the round goes to everyone.
func _battle_check() -> void:
	if not is_host or not in_battle:
		return
	var key := _battle_key()
	var got: Dictionary = _battle_orders.get(key, {})
	var seats := []
	for h in battle_parts:
		if not got.has(int(h)) and not dropped.has(int(h)):
			return
		seats.append({"empire": int(h), "orders": got.get(int(h), [])})
	_battle_orders.erase(key)
	var msg := {"t": "bround", "turn": turn, "battle": battle_idx, "round": battle_round, "seats": seats}
	_send_all(msg)
	_on_bround(1, msg)

func _on_bround(_from: int, m: Dictionary) -> void:
	if int(m.get("turn", -1)) != turn:
		return
	var key := "%d:%d:%d" % [turn, int(m.get("battle", 0)), int(m.get("round", 0))]
	if not in_battle or key != _battle_key():
		_battle_buffer[key] = m   # this peer has not reached that round yet
		return
	var core := app.session.core
	for seat in m.get("seats", []):
		var e := int(seat["empire"])
		if e == my_empire and _is_commander():
			continue   # this peer's own orders went in while it planned
		if not battle_parts.has(e):
			continue
		if not core.invoke("battleSeat", {"empire": e})["ok"]:
			continue
		for o in seat.get("orders", []):
			core.invoke("tacticalOrder", o.get("args", {}))
	if _is_commander():
		core.invoke("battleSeat", {"empire": my_empire})
	var r := core.invoke("tacticalRound", {})
	if not r["ok"]:
		_status("The battle round did not resolve: %s" % r["error"])
		return
	app.session.encounter = r["result"]
	battle_round += 1
	var finished := bool(r["result"].get("finished", false))
	if _is_commander():
		app.session.refresh()
		app.view = app.session.view
		app.lan_battle_round_played()
		if finished:
			_status("The battle is decided. Continue the turn when ready.")
			battle_sent = true
			if not auto.is_empty():
				get_tree().create_timer(1.5).timeout.connect(battle_continue)
	elif finished:
		battle_continue()
	if not finished:
		_start_battle_round()

## After a decided battle: the turn goes on (the next battle, or the end of the turn).
func battle_continue() -> void:
	if not in_battle:
		return
	in_battle = false
	battle_parts = []
	_resume_turn(app.session.core.invoke("endTurn", {}))

func _auto_battle_round() -> void:
	if auto.is_empty() or not in_battle or battle_sent or not _is_commander():
		return
	await get_tree().create_timer(float(auto.get("think", 0.4))).timeout
	if not in_battle or battle_sent:
		return
	app.session.core.invoke("tacticalOrder", {"all": true, "maneuver": "Engage"})
	battle_resolve_pressed()

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
	resolving = false
	in_battle = false
	battle_parts = []
	_battle_buffer.clear()
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
	if in_battle and not battle_sent and _battle_deadline > 0.0 and Time.get_ticks_msec() / 1000.0 >= _battle_deadline and _is_commander():
		_status("Time is up: the round resolves with the orders given.")
		battle_resolve_pressed()
	if is_host and in_game and _round_deadline > 0.0 and Time.get_ticks_msec() / 1000.0 >= _round_deadline and not orders.is_empty():
		_round_deadline = 0.0
		_status("A player's orders did not arrive in time: their seat plans nothing this turn.")
		_try_resolve(true)


func _on_message(from: int, m: Dictionary) -> void:
	match str(m.get("t", "")):
		"hello": _on_hello(from, m)
		"lobby":
			if in_game:
				return   # a rejoin: the seating is not the lobby's any more
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
		"borders": _on_borders(from, m)
		"bround": _on_bround(from, m)
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
		_rejoin(from, int(m.get("faction", 0)))
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

## E6: a player who dropped comes back to their power. They get the canonical state of the turn in progress and plan it like anyone
## else; until then their power was run by the AI. Refused while a turn is resolving or a battle is being fought (try again shortly).
func _rejoin(from: int, want: int) -> void:
	if not dropped.has(want):
		_send(from, {"t": "refused", "reason": "the game has started"})
		return
	if resolving or in_battle:
		_send(from, {"t": "refused", "reason": "the turn is being resolved; try again in a moment"})
		return
	for id in lobby.keys():
		if int(lobby[id]["empire"]) == want:
			lobby.erase(id)
	lobby[from] = {"empire": want, "name": app.session.empire_name(want)}
	dropped.erase(want)
	_send(from, {"t": "start", "blob": canonical, "humans": humans, "lobby": _lobby_wire(), "timer": timer_secs, "rejoin": true})
	_status("%s rejoined; they plan this turn." % app.session.empire_name(want))

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
	_status("%s left the game; their power is run by the AI until they rejoin." % app.session.empire_name(e))
	if in_game:
		if in_battle:
			_battle_check()
		else:
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
