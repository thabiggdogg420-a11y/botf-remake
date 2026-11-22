extends SceneTree
## LAN session host-side rules (R8, Gemini audit 22 Nov): seat assignment reaches all eight powers, a peer's orders and
## fingerprint count only for the seat the host gave that peer, and the opening campaign has the empires the lobby seated.
##   godot --headless --path remaster/godot --audio-driver Dummy -s res://scenes/ui/tests/test_lan_session.gd
## Exit code 0 on success, 1 on any failure. No sockets: with no peer open, every send is a no-op.

var failures := 0
var checks := 0
var app: App

func _initialize() -> void:
	_run.call_deferred()

func check(cond: bool, what: String) -> void:
	checks += 1
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func _run() -> void:
	var saved := {}
	AppSaves.load_settings(saved)
	var backend := "native" if Engine.has_singleton("BotfCore") else "fixture"
	App.forced_backend = "fixture" if backend == "fixture" else ""
	app = load("res://scenes/ui/app.tscn").instantiate()
	app.size = Vector2(1920, 1080)
	root.add_child(app)
	for k in 2:
		await process_frame
	_seating()
	_sender_checks()
	_rejoin_checks()
	_battle_checks()
	if backend == "native":
		_opening_empires()
		_takeover_turn()
	AppSaves.store_settings(saved)
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

func _host(taken: Array) -> LanSession:
	var lan := LanSession.new(app)
	lan.is_host = true
	lan.lobby = {}
	var id := 1
	for e in taken:
		lan.lobby[id] = {"empire": int(e), "name": "P%d" % int(e)}
		id += 1
	return lan

func _pack() -> Dictionary:
	var ci = app.session.core.request("contentInfo")
	return ci if ci is Dictionary else {}

func _seating() -> void:
	# Five seats taken, a sixth player asks for no particular power: they get the sixth, not a refusal.
	var lan := _host([1, 2, 3, 4, 5])
	lan._on_hello(9, {"protocol": LanSession.PROTOCOL, "faction": 0, "pack": _pack()})
	check(lan.lobby.has(9) and int(lan.lobby[9]["empire"]) == 6, "a sixth player with no preference is seated as empire 6")
	lan._on_hello(10, {"protocol": LanSession.PROTOCOL, "faction": 6, "pack": _pack()})
	check(lan.lobby.has(10) and int(lan.lobby[10]["empire"]) == 7, "a taken power gives the next free one (7)")
	lan._on_hello(11, {"protocol": LanSession.PROTOCOL, "faction": 0, "pack": _pack()})
	check(lan.lobby.has(11) and int(lan.lobby[11]["empire"]) == 8, "the eighth power can be seated")
	lan._on_hello(12, {"protocol": LanSession.PROTOCOL, "faction": 0, "pack": _pack()})
	check(not lan.lobby.has(12), "a ninth player is refused: every power is taken")
	lan.free()

func _sender_checks() -> void:
	var lan := _host([1, 2])   # peer 1 = empire 1 (the host), peer 2 = empire 2
	lan.in_game = true
	lan.my_empire = 1
	lan.humans = [1, 2]
	lan.turn = 5
	lan._on_orders(2, {"turn": 5, "empire": 1, "journal": [{"x": 1}]})   # peer 2 claims to be empire 1
	check(not lan.orders.has(1), "orders from peer 2 claiming empire 1 are ignored")
	lan._on_orders(77, {"turn": 5, "empire": 2, "journal": []})            # a peer that never joined
	check(not lan.orders.has(2), "orders from an unseated peer are ignored")
	lan._on_orders(2, {"turn": 5, "empire": 2, "journal": [{"x": 2}]})
	check(lan.orders.has(2), "peer 2's orders for its own seat are taken")
	lan._on_fp(2, {"turn": 5, "empire": 1, "fp": "forged"})
	check(not lan.prints.has(1), "a fingerprint from peer 2 claiming empire 1 is ignored")
	lan.free()

func _opening_empires() -> void:
	# A power above the default five needs the campaign to have it: the host's start passes the empire count.
	var lan := _host([1, 7])
	lan._host_faction = 1
	var err := lan.start_game()
	check(err == "", "a host starts a LAN game with a player on power 7 (%s)" % err)
	check(lan.humans.has(7), "the campaign seats empire 7 as a human (humans %s)" % str(lan.humans))
	lan.free()

## E6: a player who dropped comes back to their own power (and only theirs), never while a turn is resolving.
func _rejoin_checks() -> void:
	var lan := _host([1, 2])
	lan.in_game = true
	lan.my_empire = 1
	lan.humans = [1, 2]
	lan.turn = 5
	lan._on_hello(9, {"protocol": LanSession.PROTOCOL, "faction": 2, "pack": _pack()})
	check(not lan.lobby.has(9), "a newcomer asking for a power whose player is still here is refused (the game has started)")
	lan.peer_left(2)
	check(lan.dropped.has(2), "a peer that leaves is marked dropped")
	lan._on_hello(9, {"protocol": LanSession.PROTOCOL, "faction": 1, "pack": _pack()})
	check(not lan.lobby.has(9), "a rejoin asking for another seat than a dropped one is refused")
	lan.resolving = true
	lan._on_hello(9, {"protocol": LanSession.PROTOCOL, "faction": 2, "pack": _pack()})
	check(not lan.lobby.has(9) and lan.dropped.has(2), "a rejoin is refused while the turn is being resolved")
	lan.resolving = false
	lan._on_hello(9, {"protocol": LanSession.PROTOCOL, "faction": 2, "pack": _pack()})
	check(lan.lobby.has(9) and int(lan.lobby[9]["empire"]) == 2 and not lan.lobby.has(2) and not lan.dropped.has(2), "the dropped player rejoins their own power: new peer seated, old seat cleared, no longer dropped")
	lan._on_orders(9, {"turn": 5, "empire": 2, "journal": []})
	check(lan.orders.has(2), "the rejoined player's orders are taken from their new peer")
	lan.free()

## E6: a battle round goes to everyone once every commander (that is still here) has sent orders; orders count only from the seat's peer.
func _battle_checks() -> void:
	var lan := _host([1, 2])
	lan.in_game = true
	lan.in_battle = true
	lan.my_empire = 1
	lan.humans = [1, 2]
	lan.battle_parts = [1, 2]
	lan.turn = 5
	lan.battle_idx = 1
	lan.battle_round = 0
	var o := [{"seat": 1, "args": {"maneuver": "Engage", "all": true}}]
	lan._on_borders(2, {"turn": 5, "battle": 1, "round": 0, "empire": 1, "orders": o})
	check(lan._battle_orders.is_empty(), "battle orders from peer 2 claiming empire 1 are ignored")
	lan._on_borders(1, {"turn": 5, "battle": 1, "round": 0, "empire": 1, "orders": o})
	check(lan._battle_orders.get("5:1:0", {}).has(1) and lan.in_battle, "one commander's orders do not release the round")
	lan._on_borders(2, {"turn": 4, "battle": 1, "round": 0, "empire": 2, "orders": o})
	check(not lan._battle_orders.get("5:1:0", {}).has(2), "orders for another turn are ignored")
	lan._on_borders(2, {"turn": 5, "battle": 1, "round": 0, "empire": 2, "orders": o})
	check(not lan._battle_orders.has("5:1:0"), "with both commanders' orders in the round is released to everyone")
	lan._on_bround(1, {"turn": 5, "battle": 1, "round": 3, "seats": []})
	check(lan._battle_buffer.has("5:1:3"), "a round for a later point is held until this peer reaches it")
	# A commander who left counts as in: the other's orders alone release the round.
	var solo := _host([1, 2])
	solo.in_game = true
	solo.in_battle = true
	solo.my_empire = 1
	solo.humans = [1, 2]
	solo.battle_parts = [1, 2]
	solo.turn = 5
	solo.battle_idx = 1
	solo.dropped[2] = true
	solo._on_borders(1, {"turn": 5, "battle": 1, "round": 0, "empire": 1, "orders": o})
	check(not solo._battle_orders.has("5:1:0"), "a commander who left does not hold the round up")
	solo.free()
	lan.free()

## E6, on the real core: a turn resolves with one human gone (their power under the AI), the roster narrows to the human still here,
## and a rejoin restores the second human on the next turn.
func _takeover_turn() -> void:
	var lan := _host([1, 7])
	lan._host_faction = 1
	lan.start_game()
	var t0 := lan.turn
	lan.peer_left(2)   # the player of empire 7 left (their peer id in the lobby is 2)
	check(lan.dropped.has(7), "the player of empire 7 is marked dropped")
	lan.end_turn_pressed()
	check(lan.roster == [1], "the turn's roster is the human still playing (%s)" % str(lan.roster))
	check(lan.turn == t0 + 1 and not lan.history.is_empty() and str(lan.history.back()[2]) == "ok", "the turn resolved with the dropped player's power under the AI (turn %d, %s)" % [lan.turn, str(lan.history)])
	check(app.hotseat_humans().is_empty(), "with one human left there is no hotseat list")
	# Rejoin on the next turn: the roster has both again.
	lan._on_hello(9, {"protocol": LanSession.PROTOCOL, "faction": 7, "pack": _pack()})
	check(lan.lobby.has(9) and not lan.dropped.has(7), "the dropped player rejoins at the next turn start")
	lan.end_turn_pressed()
	lan._on_orders(9, {"turn": lan.turn, "empire": 7, "journal": []})
	check(lan.roster == [1, 7], "the rejoined player is in the roster again (%s)" % str(lan.roster))
	check(app.hotseat_humans() == [1, 7], "both humans are seated again")
	lan.free()
