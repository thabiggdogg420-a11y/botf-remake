extends SceneTree
## The idle prefetch order (AppLoader.idle_queue): what the web build fetches quietly on the title and the galaxy.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scenes/ui/tests/test_idle_prefetch.gd
## Exit code 0 on success, 1 on any failure.

var failures := 0

func check(cond: bool, what: String) -> void:
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)

func _initialize() -> void:
	var files := {}
	for n in ["ships_base_web.pck", "planets_hi_web.pck", "planets_web.pck", "art_web.pck", "audio_fed_web.pck", "audio_kli_web.pck",
			"hull_ship_fed_galaxy_web.pck", "hull_ship_fed_defiant_web.pck", "hull_ship_kli_bird_web.pck", "hull_ship_rom_warbird_web.pck", "hull_ship_borg_cube_web.pck"]:
		files[n] = n.replace(".pck", ".0123456789ab.pck")
	AppLoader._web_manifest = {"files": files, "bytes": {}}
	AppLoader._mounted.clear()

	var title := AppLoader.idle_queue("", ["fed", "kli", "rom"])
	check(title.size() == 2, "title: only the shared packs (%d)" % title.size())
	check(title[0] == "ships_base_web.pck" and title[1] == "planets_hi_web.pck", "title: hull manifests, then the planet tier")
	check(not title.has("audio_fed_web.pck"), "title: no empire audio before an empire is chosen")
	check(not title.any(func(n): return n.begins_with("hull_")), "title: no hulls before an empire is chosen")

	var fed := AppLoader.idle_queue("fed", ["kli", "rom"])
	check(fed[0] == "ships_base_web.pck" and fed[1] == "planets_hi_web.pck" and fed[2] == "audio_fed_web.pck", "galaxy: shared packs, then the player's audio")
	check(fed.slice(3, 5).all(func(n): return n.begins_with("hull_ship_fed_")), "galaxy: the player's hulls come before the rivals'")
	check(fed.find("hull_ship_kli_bird_web.pck") > fed.find("hull_ship_fed_defiant_web.pck"), "galaxy: rival hulls after the player's hulls")
	check(not fed.has("audio_kli_web.pck"), "galaxy: only the player's empire audio")

	AppLoader._mounted["ships_base_web.pck"] = true
	AppLoader._mounted["audio_fed_web.pck"] = true
	var after := AppLoader.idle_queue("fed", ["kli", "rom"])
	check(not after.has("ships_base_web.pck") and not after.has("audio_fed_web.pck"), "a mounted pack is left out")
	check(after[0] == "planets_hi_web.pck", "the next pack moves up")

	AppLoader._web_manifest = {}
	check(AppLoader.idle_queue("fed", ["kli"]).is_empty(), "no manifest, nothing queued")
	AppLoader._web_manifest = null
	AppLoader._mounted.clear()
	print("%d failures" % failures)
	quit(1 if failures > 0 else 0)
