extends SceneTree
## Cold first visit hail proof (18 Nov): starts a campaign and opens the first hail at once, before the portrait pack has landed,
## then prints what the viewscreen actually draws ("HAILSTATE <ms> <static|picture|bust>") each time it changes, for 40 s.
## Run in a fresh browser context (empty cache, no service worker), throttled so the pack takes a few seconds:
##   web_driver.py --dir <web build> --work <dir> --throttle 20000,5000,40 --args "-s res://scenes/ui/tools/hail_cold_capture.gd"
##   then `shot <name>` commands while the HAILSTATE lines scroll (`log 40`).

var app: App

func _initialize() -> void:
	app = load("res://scenes/ui/app.tscn").instantiate()
	root.add_child(app)
	_run.call_deferred()

func _wait(frames: int = 8) -> void:
	for n in frames:
		await process_frame

func _run() -> void:
	await _wait(6)
	app.settings["tutorial"] = -1
	app.start_campaign({"faction": 1, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48,
		"preset": "remastered", "events": true, "aiAggression": true})
	app.close_all_modals()
	app.goto("diplomacy")
	await _wait(4)
	var ds := app.current_screen()
	var others: Array = app.view.get("empires", []).filter(func(e): return int(e.get("empire", 0)) != app.player())
	var tid := int(others[0]["empire"]) if not others.is_empty() else 2
	var t0 := Time.get_ticks_msec()
	ds.open_hail(app.empire_name(tid), tid, false, "At peace")
	var last := ""
	print("HAIL OPEN power ", app.empire_name(tid), " pack_pending ", AppLoader.portrait_pack_pending())
	while Time.get_ticks_msec() - t0 < 40000:
		await _wait(3)
		var hv: HailViewscreen = app.find_child("HailViewscreen", true, false) as HailViewscreen
		var st := "none" if hv == null else hv.drawn()
		if st != last:
			last = st
			print("HAILSTATE ", Time.get_ticks_msec() - t0, " ", st, " keys ", hv.portrait_keys() if hv else [])
	print("HAILDONE ", last)
