extends SceneTree
## Hotseat battle and shared-win frame proof (MULTIPLAYER.md, 21 Nov 2026), through the real screens:
##   godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 --position -10000,-10000 -s res://scenes/ui/tools/hotseat_battle_capture.gd -- <out_dir>
## The tactical drill (Federation against the Klingons at Sol) as a two-human hotseat game. The Klingon player plans last, so
## the battle opens with the Klingons in the chair: the Federation's human gets the battle behind the hand-over card instead
## of the tactical AI fighting it. Each human plans the round in turn behind the card; the second Resolve ends the round.
## Then the end screen of a shared diplomatic win names the allied humans (a preview built from the view, the core's
## "winners" list filled as the bridge fills it). Prints "HSB ..." lines; exit code 1 on a failed check.

var app: App
var out_dir := "user://hotseat_battle"
var code := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out_dir = str(args[0])
	DirAccess.make_dir_recursive_absolute(out_dir)
	AppSaves.sandbox("user://capture_sandbox/hotseat_battle/%d" % int(Time.get_unix_time_from_system()))
	root.set_flag(Window.FLAG_NO_FOCUS, true)
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()


func _wait(n: int) -> void:
	for i in n:
		await process_frame


func _shot(name: String) -> void:
	await create_timer(1.2).timeout
	await _wait(2)
	var img := root.get_viewport().get_texture().get_image()
	img.resize(960, 540, Image.INTERPOLATE_LANCZOS)
	img.save_png("%s/%s.png" % [out_dir, name])


func _check(ok: bool, what: String) -> void:
	print("HSB %s %s" % ["ok  " if ok else "FAIL", what])
	if not ok:
		code = 1


func _card_seat() -> int:
	var card := app.get_node_or_null("HotseatCard")
	return int(card.get_meta("battle", 0)) if card != null else 0


func _run() -> void:
	await _wait(10)
	app.cinematics_live = false
	app.start_drill("newBattleDrill", "Tactical drill")
	await _wait(20)
	var r: Dictionary = app.session.core.invoke("setHotseat", {"humans": [1, 2]})
	_check(r["ok"], "the drill becomes a two-human hotseat game")
	app.session.refresh()
	app.view = app.session.view
	app.close_all_modals()
	var st := app.end_turn()   # the Federation has planned: the chair passes to the Klingons
	await _wait(20)
	_check(st == "handover" and int(app.view["meta"]["player"]) == 2, "the chair passes to the Klingon player")
	app._hotseat_take(app.get_node("HotseatCard"))
	await _wait(10)
	app.close_all_modals()
	st = app.end_turn()   # the last human ends the turn: the battle at Sol involves both humans
	await _wait(40)
	_check(st == "battle", "ending the turn opens the battle")
	var bh: Array = app.view.get("session", {}).get("battleHumans", [])
	_check(bh == [1, 2], "both humans are in the battle (%s)" % str(bh))
	_check(app.has_node("HotseatCard") and _card_seat() == 1, "the battle opens behind the card for the Federation player")
	await _shot("1-battle-card-federation")
	app._hotseat_take(app.get_node("HotseatCard"))
	await _wait(30)
	_check(int(app.encounter.get("foe", 0)) == 2 and _mine(1), "the Federation player commands the Federation ships")
	await _shot("2-battle-federation-plans")
	var cs = app.current_screen()
	cs._round()   # the Federation's Resolve hands the round to the Klingon player
	await _wait(20)
	_check(app.has_node("HotseatCard") and _card_seat() == 2 and int(app.encounter.get("round", -1)) == 0, "the Klingon player plans the same round behind the card")
	await _shot("3-battle-card-klingon")
	app._hotseat_take(app.get_node("HotseatCard"))
	await _wait(30)
	_check(_mine(2), "the Klingon player commands the Klingon ships")
	await _shot("4-battle-klingon-plans")
	cs = app.current_screen()
	cs._round()   # the second Resolve ends the round
	await create_timer(6.0).timeout
	_check(int(app.encounter.get("round", 0)) >= 1, "the round resolved after both humans planned (round %d)" % int(app.encounter.get("round", 0)))
	await _shot("5-after-round")
	# The rest of the battle by the tactical AI, then the turn completes.
	if app.has_node("HotseatCard"):
		app._hotseat_take(app.get_node("HotseatCard"))
	cs = app.current_screen()
	if cs != null and cs.has_method("_auto"):
		cs._auto()
	await _wait(20)
	st = app.end_turn()
	await _wait(40)
	_check(st == "complete", "the turn completes after the battle (%s)" % st)
	print("HSB turn now %d, chair %d" % [int(app.view["meta"]["turn"]), int(app.view["meta"]["player"])])
	# Shared diplomatic win: the end screen for a hotseat game names the allied humans.
	if app.has_node("HotseatCard"):
		app._hotseat_take(app.get_node("HotseatCard"))
	app.close_all_modals()
	var v: Dictionary = app.view.duplicate(true)
	v["meta"]["hotseat"] = [1, 2]
	v["meta"]["winners"] = [1, 2]
	v["meta"]["winner"] = 1
	v["meta"]["outcome"] = "victory"
	app.view = v
	app.session.view = v
	Overlays.end_screen(app, "victory")
	await _wait(10)
	var allies := app.find_child("EndAllies", true, false)
	_check(allies != null, "the end screen names the allied humans")
	if allies != null:
		for n in allies.find_children("*", "Label", true, false):
			print("HSB end screen: ", (n as Label).text)
	await _shot("6-end-shared-win")
	print("HSB done, code ", code)
	quit(code)


func _mine(owner: int) -> bool:
	var e = app.encounter
	if not (e is Dictionary):
		return false
	for s in e.get("ships", []):
		if s.get("mine", false) and int(s.get("owner", 0)) != owner:
			return false
	for s in e.get("ships", []):
		if s.get("mine", false):
			return true
	return false
