extends SceneTree
## Onboarding frames (ONBOARDING-AND-MOBILE-PLAN.md section 1), played with real clicks from the title:
## the title with its emblems and Quick Start, each of the five beats with its spotlight, the first-contact and first-battle
## lines, the side tabs dimmed and then lit with their new marks, and a Custom game's galaxy with no dimming.
##   Godot --path remaster/godot --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/onboarding_capture.gd -- <out_dir>
## Writes ob-01 .. ob-14 PNGs to <out_dir> (default user://onboarding). The run is sandboxed like every scripted run.

var app: App
var out_dir := "user://onboarding"
var shot_n := 0

func _initialize() -> void:
	var args := Array(OS.get_cmdline_user_args())
	if args.size() > 0:
		out_dir = args[0]
	DirAccess.make_dir_recursive_absolute(out_dir)
	OS.set_environment("BOTF_TITLE_EMPIRE", "1")
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _wait(frames: int = 8) -> void:
	for n in frames:
		await process_frame

func _wait_ms(ms: int) -> void:
	var t := Time.get_ticks_msec()
	while Time.get_ticks_msec() - t < ms:
		await _wait(4)

func _shot(label: String) -> void:
	await _wait(8)
	app.advisor_panel.update()
	await _wait(8)
	shot_n += 1
	var name_ := "ob-%02d-%s" % [shot_n, label]
	var img := root.get_viewport().get_texture().get_image()
	img.save_png("%s/%s.png" % [out_dir, name_])
	print("SHOT ", name_, "  guidance: ", str(app.advisor_panel.current.get("text", "")))

func _click_at(at: Vector2) -> void:
	var mm := InputEventMouseMotion.new()
	mm.position = at
	mm.global_position = at
	root.push_input(mm, true)
	await _wait(3)
	for pressed in [true, false]:
		var e := InputEventMouseButton.new()
		e.button_index = MOUSE_BUTTON_LEFT
		e.pressed = pressed
		e.position = at
		e.global_position = at
		e.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed else 0
		root.push_input(e, true)
		await _wait(3)
	await _wait(6)

## Clicks what the spotlight lights.
func _click_spot() -> void:
	app.advisor_panel.update()
	await _wait(4)
	var rects := app.advisor_panel.target_rects()
	if rects.is_empty():
		printerr("nothing spotlit for ", str(app.advisor_panel.current.get("id", "")))
		return
	await _click_at(rects[0].get_center())

func _click_named(n: String) -> void:
	var c := app.find_child(n, true, false) as Control
	if c == null:
		printerr("no control ", n)
		return
	await _click_at(c.get_global_transform_with_canvas() * (c.size * 0.5))

func _run() -> void:
	await _wait(10)
	# 1: the title
	await _wait_ms(2500)
	await _shot("title-quickstart")
	# 2: Quick Start -> the galaxy, beat 1
	var title := app.current_screen()
	title.quick_seed = 31337
	await _click_named("QuickStart")
	await _wait_ms(3500)
	await _shot("beat1-tap-the-scout-tabs-dimmed")
	await _click_spot()          # the scout's button
	await _wait_ms(800)
	await _shot("beat1-tap-a-star")
	await _click_spot()          # the star
	await _wait_ms(800)
	await _shot("beat2-home-system-tab")
	await _click_spot()          # the System tab
	await _wait_ms(4000)
	await _shot("beat2-pick-a-build")
	await _click_spot()          # a build choice
	await _wait_ms(1200)
	await _shot("beat3-end-turn-on-system")
	app.goto("galaxy")
	await _wait_ms(1800)
	await _shot("beat3-end-turn")
	await _click_spot()          # End turn
	await _wait_ms(2500)
	await _shot("beat4-short-log")
	await _click_spot()          # close the log
	await _wait_ms(1200)
	await _shot("beat5-colony-ship")
	await _click_spot()
	await _wait_ms(800)
	await _shot("beat5-colony-star")
	await _click_spot()
	await _wait_ms(800)
	# end turns until first contact
	var adv := app.advisor_panel.advisor
	var n := 0
	while not TabReveal.contact_made(app.view) and n < 16 and app.screen != "combat":
		app.close_all_modals()
		app.end_turn()
		await _wait(8)
		n += 1
	app.close_all_modals()
	await _wait_ms(1500)
	await _shot("first-contact-line-and-new-tabs")
	app.goto("diplomacy")
	await _wait_ms(1500)
	app.goto("galaxy")
	await _wait_ms(1500)
	await _shot("tabs-after-diplomacy-opened")
	# the first battle line
	app.settings["tutorial"] = 0
	app.settings["tutorialBeats"] = 1
	app.start_drill("newBattleDrill", "Tactical drill")
	await _wait_ms(2500)
	app.end_turn()
	for k in 900:
		await _wait(1)
		if app.screen == "combat" and app.advisor_panel.visible:
			break
	await _wait_ms(3000)
	await _shot("first-battle-line")
	# a Custom game: nothing dimmed
	app.view = {}
	app.goto("title", false)
	await _wait_ms(1500)
	await _click_named("NewCampaign")
	await _wait_ms(2500)
	var nc := app.current_screen()
	nc.faction = 1
	nc.seed_value = 31337
	app.settings["tutorial"] = -1   # no spotlight over the frame: it is here to show every tab at full strength
	nc.start()
	await _wait_ms(3500)
	await _shot("custom-game-no-dimming")
	quit(0)
