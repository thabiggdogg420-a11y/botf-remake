extends SceneTree
## M4-TREKIFY proof frames: every non-battle screen in one interface look, full 1920x1080 (a real GPU window, always --audio-driver Dummy).
##   godot --path remaster/godot --resolution 1920x1080 --audio-driver Dummy -s res://scenes/ui/tools/trekify_capture.gd -- <out_dir> <look> <tag> [shot ...]
## look: tos movie tng voy late. tag prefixes the file names (before / after). Faction 1 (Federation) unless SHOT_FACTION is set.

var out_dir := ""
var look := "tng"
var tag := "x"
var shots: Array = []
var app: App

const ALL := ["title", "new", "new-preset", "galaxy", "galaxy-whole", "galaxy-menu", "station", "system-installed", "system-structures", "system-ships",
	"research", "fleets", "empire-colonies", "empire-fleets", "empire-trade", "empire-governors", "empire-templates", "empire-stations", "empire-members",
	"empire-programs", "diplomacy", "diplomacy-propose", "diplomacy-history", "diplomacy-ledger", "diplomacy-news", "intel", "db-ships", "db-tech",
	"db-structures", "db-races", "db-minors", "events", "victory", "briefing", "end", "settings", "saves", "menu", "help", "packs", "palette"]

func _initialize() -> void:
	var a := OS.get_cmdline_user_args()
	out_dir = a[0]
	look = a[1]
	tag = a[2]
	shots = Array(a.slice(3)) if a.size() > 3 else ALL
	DirAccess.make_dir_recursive_absolute(out_dir)
	_run.call_deferred()

func _wait(n: int = 6) -> void:
	for i in n:
		await process_frame

func _save(name_: String) -> void:
	await _wait(8)
	for t in app.get_node("Toasts").get_children():
		t.queue_free()
	await _wait(3)
	var img := root.get_viewport().get_texture().get_image()
	var p := out_dir.path_join("%s-%s-%s.png" % [tag, look, name_])
	img.save_png(p)
	print("saved ", p, " ", img.get_size())

func _campaign(faction: int) -> void:
	var opts := {"faction": faction, "seed": 424242, "era": 3, "ruleset": "reference", "width": 22, "height": 16, "systems": 48, "preset": "remastered", "events": true, "aiAggression": true}
	app.start_campaign(opts)
	if app.core.is_native():
		for n in 12:
			app.end_turn()
			app.close_all_modals()
	app.close_all_modals()

func _run() -> void:
	OS.set_environment("BOTF_TITLE_EMPIRE", "1")
	app = load("res://scenes/ui/app.tscn").instantiate()
	app.settings["uiEra"] = look
	root.add_child(app)
	await _wait(6)
	app.settings["uiEra"] = look
	UiTokens.era = look
	app._theme_empire = -1
	var faction := int(OS.get_environment("SHOT_FACTION")) if OS.get_environment("SHOT_FACTION") != "" else 1
	var started := false
	for s in shots:
		var needs_campaign: bool = not (s in ["title", "new", "new-preset", "saves", "packs"])
		if needs_campaign and not started:
			_campaign(faction)
			started = true
		await _prepare(s, faction)
		await _save(s)
		app.close_all_modals()
	app.queue_free()
	quit(0)

func _screen_tab(id: String, tab: String) -> void:
	app.goto(id)
	await _wait(3)
	var sc = app.current_screen()
	sc.tab = tab
	sc.refresh()
	await _wait(4)

func _prepare(s: String, faction: int) -> void:
	match s:
		"title":
			var keep: Dictionary = app.view
			app.view = {}
			app.goto("title")
			await _wait(16)
			app.view = keep
		"new":
			app.view = {}
			app.goto("new")
			await _wait(8)
		"new-preset":
			app.view = {}
			app.goto("new")
			await _wait(2)
			var picker = app.current_screen()
			picker.vm.pick_preset("dominion-war")
			picker.refresh()
			await _wait(6)
		"saves":
			app.goto("saves")
			await _wait(6)
		"packs":
			app.goto("packs")
			await _wait(8)
		"galaxy":
			app.goto("galaxy")
			var cols: Array = app.view.get("fleets", [])
			if cols.size() > 0:
				app.select_fleet(int(cols[0]["id"]))
			await _wait(10)
		"galaxy-whole":
			app.goto("galaxy")
			await _wait(4)
			var gm: GalaxyMap = app.current_screen().map
			var fl: Array = app.view.get("fleets", [])
			gm.focus_cell(int(fl[0]["x"]) if fl.size() > 0 else 11, int(fl[0]["y"]) if fl.size() > 0 else 8, 0.7, false)
			await _wait(6)
		"galaxy-menu":
			app.goto("galaxy")
			await _wait(3)
			var scr := app.current_screen()
			var f: Dictionary = app.view["fleets"][0]
			app.select_fleet(int(f["id"]))
			if scr.has_method("open_context_menu_at_cell"):
				scr.open_context_menu_at_cell(int(f["x"]) + 2, int(f["y"]) + 1)
			await _wait(6)
		"station":
			app.goto("galaxy")
			await _wait(3)
			for q in app.view.get("systems", []):
				if int(q.get("owner", 0)) == faction:
					app.select_cell(int(q["y"]) * app.width() + int(q["x"]))
					break
			await _wait(8)
		"system-installed":
			await _screen_tab("system", "installed")
		"system-structures":
			await _screen_tab("system", "structures")
		"system-ships":
			await _screen_tab("system", "ships")
			var t0 := Time.get_ticks_msec()
			while Time.get_ticks_msec() - t0 < 2000:
				await _wait(5)
		"research":
			app.goto("research")
		"fleets":
			app.goto("fleets")
		"empire-colonies", "empire-fleets", "empire-trade", "empire-governors", "empire-templates", "empire-stations", "empire-members", "empire-programs":
			await _screen_tab("empire", s.trim_prefix("empire-"))
		"diplomacy":
			app.goto("diplomacy")
		"diplomacy-propose", "diplomacy-history":
			app.goto("diplomacy")
			await _wait(3)
			var ds = app.current_screen()
			ds.pane = "dossier"
			ds.dossier_tab = "propose" if s == "diplomacy-propose" else "history"
			ds.refresh()
		"diplomacy-ledger", "diplomacy-news":
			app.goto("diplomacy")
			await _wait(3)
			var ds2 = app.current_screen()
			ds2.pane = s.trim_prefix("diplomacy-")
			ds2.refresh()
		"intel":
			app.goto("intel")
		"db-ships", "db-tech", "db-structures", "db-races", "db-minors":
			app.goto("database")
			await _wait(3)
			var dbs = app.current_screen()
			dbs.vm.pick_tab({"db-ships": "ships", "db-tech": "technologies", "db-structures": "structures", "db-races": "races", "db-minors": "minors"}[s])
			dbs.refresh()
		"events", "victory", "end", "settings", "menu", "help":
			app.goto("galaxy")
			await _wait(3)
			if s == "end":
				Overlays.end_screen(app, "victory")
			else:
				app.open_overlay(s)
		"briefing":
			app.goto("galaxy")
			await _wait(3)
			app.briefing = app.core.request("briefing")
			app.open_overlay("briefing")
		"palette":
			app.goto("galaxy")
			await _wait(3)
			app.open_palette()
	await _wait(8)
