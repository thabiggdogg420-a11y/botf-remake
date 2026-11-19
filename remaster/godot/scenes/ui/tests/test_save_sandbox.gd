extends SceneTree
## The player's saves are off limits to tests and tools (F6 round: a test overwrote Simon's Autosave 2 and 3).
## A scripted run with no sandbox of its own is moved to user://capture_sandbox/auto/... on first use, and AppLoader
## refuses to write or delete the real user://saves and user://botf_settings.json from any scripted run.
##   godot --headless --path remaster/godot -s res://scenes/ui/tests/test_save_sandbox.gd

var failures := 0


func check(cond: bool, what: String) -> void:
	if cond:
		print("  ok  ", what)
	else:
		failures += 1
		printerr("  FAIL ", what)


func _initialize() -> void:
	print("Save sandbox tests")
	check(AppSaves.scripted_run(), "a -s run is seen as scripted")
	AppSaves.ensure()
	check(AppSaves.DIR.begins_with(AppSaves.AUTO_SANDBOX), "saves moved to the auto sandbox (%s)" % AppSaves.DIR)
	check(AppSaves.SETTINGS_PATH.begins_with(AppSaves.AUTO_SANDBOX), "settings moved to the auto sandbox")
	check(UserPacks.dir.begins_with(AppSaves.AUTO_SANDBOX), "user packs moved to the auto sandbox")
	check(AppSaves.write({"turn": 1}, "SandboxProbe", "probe") == "", "a save writes into the sandbox")
	check(FileAccess.file_exists(AppSaves.DIR + "/SandboxProbe.json"), "the sandboxed slot is on disk")
	check(not FileAccess.file_exists(AppSaves.REAL_DIR + "/SandboxProbe.json"), "nothing landed in the real saves")
	var before := FileAccess.get_file_as_string(AppSaves.REAL_SETTINGS) if FileAccess.file_exists(AppSaves.REAL_SETTINGS) else "<none>"
	check(AppLoader.write_json(AppSaves.REAL_DIR + "/Autosave 2.json", {"x": 1}) != "", "writing the real Autosave 2 is refused")
	check(AppLoader.write_text(AppSaves.REAL_SETTINGS, "{}") != "", "writing the real settings file is refused")
	var after := FileAccess.get_file_as_string(AppSaves.REAL_SETTINGS) if FileAccess.file_exists(AppSaves.REAL_SETTINGS) else "<none>"
	check(before == after, "the real settings file is unchanged")
	AppSaves.delete("SandboxProbe.json")
	check(not FileAccess.file_exists(AppSaves.DIR + "/SandboxProbe.json"), "the probe slot is removed from the sandbox")
	print("save sandbox: %d failures" % failures)
	quit(1 if failures > 0 else 0)
