extends SceneTree
## Pack editor tests (lane MOD-10, docs/design/MOD-TOOLS.md): the editor view model's pure helpers, then, with the native core, the
## whole route a user pack takes: edit, core validation errors, save to a scratch data folder, the preset picker, a new game on the
## pack, 20 turns, save, reload, and the refusals when the pack's files change or go missing. Without BotfCore the live part reports
## SKIP and passes (the same rule as the empire-rules suite).
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scripts/viewmodels/tests/test_pack_editor_vm.gd

var failures := 0
var checks := 0
const SCRATCH := "user://mods-test"

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
	_pure()
	if not Engine.has_singleton("BotfCore"):
		print("SKIP: BotfCore is not loaded; the live checks need the native core")
	else:
		_live()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

func _pure() -> void:
	print("== pure helpers")
	check(PackEditorVM.parse_field("int", " 42 ") == {"ok": true, "value": 42}, "an int field reads a whole number")
	check(not PackEditorVM.parse_field("int", "4.5")["ok"] and not PackEditorVM.parse_field("int", "abc")["ok"], "an int field refuses 4.5 and text")
	check(PackEditorVM.parse_field("dec", "2.5")["value"] == 2.5 and PackEditorVM.parse_field("dec", "3")["value"] == 3, "a decimal field reads 2.5 and 3")
	check(PackEditorVM.parse_field("bool", "true")["value"] == true and not PackEditorVM.parse_field("bool", "maybe")["ok"], "a bool field reads true/false only")
	check(PackEditorVM._norm({"a": 5.0, "b": [1.0, 2.5], "c": "x"}) == {"a": 5, "b": [1, 2.5], "c": "x"}, "whole floats go back as ints")
	check(PackEditorVM._nest({"major.traitEffects.researchPct": 8, "name": "X"}) == {"major": {"traitEffects": {"researchPct": 8}}, "name": "X"}, "dotted keys nest for the overlay")
	check(PackEditorVM._flatten({"major": {"traitEffects": {"morale": 3}}}) == {"major.traitEffects.morale": 3}, "nested patches flatten for the editor")
	check(PackEditorVM.flag_default("stations", "classic") and not PackEditorVM.flag_default("wegoCombat", "classic") and PackEditorVM.flag_default("wegoCombat", "remastered"), "flag defaults follow the rule base")
	check(PackEditorVM.flag_default("interceptOrders", "classic") and not PackEditorVM.flag_default("eras", "remastered"), "economy flags and eras default as in the core")
	check(PackEditorVM._tab_of("content: overlay x.roster.patch[0]: unknown field \"armour\"") == "roster", "an error naming roster points at the roster tab")
	check(PackEditorVM._tab_of("content: pack.balance.weaponOutputPercent: 5 is outside 10..1000") == "balance", "a balance error points at balance")
	check(PackEditorVM._tab_of("content: preset x: must list the pack's overlay") == "preset", "a preset error points at the preset tab")
	check(UserPacks.friendly_load_error("content: save: needs overlay my-pack, which is not installed").contains("\"my-pack\", which is not installed"), "a missing pack is said plainly")
	check(UserPacks.friendly_load_error("content: save: overlay my-pack changed since the save (aa then, bb now)").contains("earlier version of the pack \"my-pack\""), "a changed pack is said plainly")
	check(UserPacks.friendly_load_error("something else") == "something else", "other errors pass through")

func _wipe() -> void:
	for id in UserPacks.ids():
		UserPacks.remove(id)

func _end_turns(s: AppSession, n: int) -> void:
	for i in n:
		var r := s.core.invoke("endTurn", {})
		var guard := 0
		while r.get("ok", false) and r["result"]["status"] == "battle" and guard < 20:
			s.core.invoke("tacticalAuto", {})
			r = s.core.invoke("endTurn", {})
			guard += 1
		if not r.get("ok", false):
			check(false, "turn %d ended: %s" % [i, r.get("error", "")])
			return

func _live() -> void:
	print("== live core")
	UserPacks.dir = SCRATCH
	_wipe()
	var s := AppSession.create("auto")
	var vm := PackEditorVM.new(s)
	check(vm.supported(), "the native core takes user packs")
	var shipped: int = s.meta.get("presets", []).size()
	vm.new_pack()
	check(vm.error != "" and vm.error_tab == "identity" and not vm.error_core, "an empty draft asks for an id first (the editor's own note, not a core refusal)")
	vm.set_identity("ut-pack", "Test pack", "A pack made by the editor test.")
	check(vm.error == "" and str(vm.status.get("stackId", "")) == "botf-base+ut-pack", "a named draft is valid: %s" % vm.error)
	# Ship class edit.
	var rows := vm.rows("roster")
	check(rows.size() >= 98 and str(rows[0]["name"]) == "NX", "the roster lists %d classes starting with NX" % rows.size())
	check(vm.set_field("roster", 1000, "industryCost", "1500") == "" and vm.error == "", "editing a class cost validates")
	check(vm.field_value("roster", 1000, "industryCost") == 1500 and vm.is_changed("roster", 1000, "industryCost") and vm.base_value("roster", 1000, "industryCost") == 1830, "the patch is held and the base value kept")
	check(vm.set_field("roster", 1000, "industryCost", "abc") != "", "a non-number is refused by the editor before the core")
	# A core refusal, inline.
	check(vm.set_field("roster", 1000, "hull", "-5") == "" and vm.error != "" and vm.error.contains("outside") and vm.error_tab == "roster" and vm.error_core, "the core's range error comes back on the roster tab: %s" % vm.error)
	check(vm.save() != "" and UserPacks.ids().is_empty(), "an invalid pack is not saved")
	check(vm.set_field("roster", 1000, "hull", "140") == "" and vm.error == "", "fixing the value clears the error")
	# Other sections.
	check(vm.set_field("buildings", 0, "industryCost", "350") == "" and vm.set_field("technologies", 13, "cost", "220") == "" and vm.set_field("races", 1, "major.traitEffects.researchPct", "8") == "" and vm.set_field("minors", 1, "name", "Acamar Clans") == "" and vm.error == "", "structure, tech, race trait and minor edits validate: %s" % vm.error)
	check(vm.set_balance("weaponOutputPercent", "90") == "" and vm.set_balance("researchCostPercent", "5") == "" and vm.error.contains("balance") and vm.error_tab == "balance", "balance edits validate and a bad one points at balance: %s" % vm.error)
	check(vm.set_balance("researchCostPercent", "100") == "" and not vm.balance.has("researchCostPercent") and vm.error == "", "a value back at its default drops out")
	var added := vm.add_record("roster", 1000, "Test Cutter")
	check(added == 1098 or added > 1000, "a class is added by copying one (id %d)" % added)
	check(vm.error == "" and str(vm.field_value("roster", added, "className")) == "Test Cutter", "the added class validates: %s" % vm.error)
	check(vm.set_field("roster", added, "attack", "999") == "" and vm.error == "" and not vm.patches.get("roster", {}).has(added), "an added class is edited in place, not patched")
	var text_before := vm.overlay_text()
	check(JSON.parse_string(text_before) is Dictionary, "the overlay file is JSON")
	# Lineup and rules.
	vm.toggle_empire("Dominion", true)
	check(vm.lineup.has("Dominion") and vm.error == "", "the lineup takes the Dominion")
	vm.toggle_empire("Dominion", false)
	for e in ["Romulan", "Cardassian", "Ferengi"]:
		vm.toggle_empire(e, false)
	vm.toggle_empire("Klingon", false)
	check(vm.error != "" and vm.error_tab == "preset", "a one-empire lineup is refused on the preset tab: %s" % vm.error)
	vm.toggle_empire("Klingon", true)
	check(vm.error == "", "two empires are enough")
	vm.set_flag("wegoCombat", false)
	vm.set_rules_base("classic")
	check(vm.error != "" and vm.error_tab == "preset", "Classic with the remaster layers on is refused: %s" % vm.error)
	vm.set_rules_base("remastered")
	check(vm.error == "", "back on Remastered it is valid: %s" % vm.error)
	# Eras.
	vm.set_timeline(true)
	check(vm.has_timeline() and vm.flag_value("eras") and vm.error == "", "a timeline switches the eras rule on: %s" % vm.error)
	check(vm.set_era("years", 1, "2285") == "" and vm.set_era_window(1000, "2250", "0") == "" and vm.error == "", "era years and a class window validate: %s" % vm.error)
	check(vm.set_era("years", 2, "2200") == "" and vm.error.to_lower().contains("ascend"), "years must ascend: %s" % vm.error)
	vm.set_era("years", 2, "2320")
	# Save and the picker.
	check(vm.save() == "" and UserPacks.ids() == ["ut-pack"], "the pack saves: %s" % vm.error)
	var files := UserPacks.read("ut-pack")
	check(str(files["overlay"]) == vm.overlay_text() and str(files["preset"]) == vm.preset_text(), "the files hold exactly the bytes the core was given")
	var presets: Array = s.meta.get("presets", [])
	check(presets.size() == shipped + 1 and presets.any(func(p): return p["id"] == "ut-pack" and p.get("user", false)), "the preset picker lists the user pack, marked as user")
	check(not presets.filter(func(p): return p["id"] != "ut-pack").any(func(p): return p.has("user")), "shipped presets carry no user mark")
	var info: Array = s.core.request("userPacks")
	check(info.size() == 1 and info[0]["id"] == "ut-pack" and str(info[0]["checksum"]).length() == 16, "the core lists the user pack with its checksum")
	var nc := NewCampaignVM.new(s)
	check(nc.preset_text("ut-pack").contains("Your own pack") and nc.pick_preset("ut-pack"), "the new-game picker offers it")
	# Reload from disk.
	var vm2 := PackEditorVM.new(s)
	check(vm2.load_pack("ut-pack") and vm2.pack_id == "ut-pack", "a saved pack opens again")
	check(vm2.field_value("roster", 1000, "industryCost") == 1500 and vm2.balance.get("weaponOutputPercent", 0) == 90 and vm2.has_timeline(), "its edits are all there")
	check(vm2.error == "" and vm2.revision == vm.revision, "and it still validates: %s" % vm2.error)
	check(vm2.overlay_text() == files["overlay"], "reloading and rewriting gives the same bytes")
	# Refuse a shipped id.
	var vm3 := PackEditorVM.new(s)
	vm3.new_pack()
	vm3.set_identity("ultimate", "Mine", "")
	check(vm3.error.contains("shipped") and vm3.save() != "", "a shipped pack id is refused: %s" % vm3.error)
	vm3.new_pack("ages")
	vm3.set_identity("ut-on-ages", "On Ages", "")
	check(vm3.under == ["ages"] and vm3.timeline_inherited() and vm3.error == "", "building on Ages puts it under the pack: %s" % vm3.error)
	check(vm3.set_field("roster", 1000, "buildable", true) == "" and vm3.error == "", "a boolean field edits")
	# A game on the pack: 20 turns, save, reload.
	var opts := AppSession.campaign_options({"faction": 1, "empires": 3, "seed": 777, "era": 1, "ruleset": "reference", "preset": "ut-pack", "size": "small"})
	var v := s.start("newCampaign", opts)
	check(not v.is_empty() and str(v["meta"]["rules"]["preset"]) == "ut-pack", "a new game starts on the user pack")
	check(str(s.core.request("contentInfo")["id"]) == "botf-base+ut-pack", "its stack is the active content")
	_end_turns(s, 20)
	var blob := s.save_blob()
	var fp: Variant = s.core.request("fingerprint")
	check(not blob.is_empty() and str(blob["data"]).contains("PACKS ut-pack 1 ut-pack "), "the save records the pack and its checksum")
	s.core.invoke("loadBaseContent", {})
	var lv := s.start("load", {"data": blob["data"], "scenario": "campaign"})
	check(not lv.is_empty() and s.core.request("fingerprint") == fp and int(lv["meta"]["turn"]) == 20, "the save reloads at turn 20 with its fingerprint")
	_end_turns(s, 2)
	# Tampered pack: its files change, the app restarts (register_all), the save is refused with a clear message.
	var tampered := str(files["overlay"]).replace("\"industryCost\":1500", "\"industryCost\":1501")
	check(tampered != str(files["overlay"]), "the tamper changes the overlay text")
	AppLoader.write_text(UserPacks.overlay_path("ut-pack"), tampered)
	var reg := UserPacks.register_all(s.core)
	check(reg["loaded"].has("ut-pack"), "the tampered pack still registers (it is valid, only different)")
	var failed_msgs := []
	s.failed.connect(func(m): failed_msgs.append(m))
	s.core.invoke("loadBaseContent", {})
	check(s.start("load", {"data": blob["data"], "scenario": "campaign"}).is_empty(), "a save made on the original bytes refuses to load")
	check(failed_msgs.size() == 1 and str(failed_msgs[0]).contains("earlier version of the pack \"ut-pack\""), "with a clear message: %s" % str(failed_msgs))
	# Restore the original bytes: it loads again.
	AppLoader.write_text(UserPacks.overlay_path("ut-pack"), files["overlay"])
	UserPacks.register_all(s.core)
	s.core.invoke("loadBaseContent", {})
	check(not s.start("load", {"data": blob["data"], "scenario": "campaign"}).is_empty(), "restoring the exact bytes restores the save")
	# Missing pack.
	s.core.invoke("removeUserPack", {"id": "ut-pack"})
	failed_msgs.clear()
	s.core.invoke("loadBaseContent", {})
	check(s.start("load", {"data": blob["data"], "scenario": "campaign"}).is_empty(), "a save whose pack is gone refuses to load")
	check(failed_msgs.size() == 1 and str(failed_msgs[0]).contains("\"ut-pack\", which is not installed"), "with a clear message: %s" % str(failed_msgs))
	# Delete through the editor; Classic and Remastered games are unaffected.
	vm2.delete_pack()
	check(UserPacks.ids().is_empty() and s.meta.get("presets", []).size() == shipped, "deleting removes the files and the preset")
	var classic: Variant = s.core.request("newCampaign", AppSession.campaign_options({"faction": 1, "empires": 2, "seed": 5, "preset": "classic", "size": "small"}))
	check(classic is Dictionary and str(classic["meta"]["rules"]["preset"]) == "classic", "a Classic game starts normally")
	_wipe()
	DirAccess.remove_absolute(SCRATCH)
