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
	if backend == "native":
		_opening_empires()
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
