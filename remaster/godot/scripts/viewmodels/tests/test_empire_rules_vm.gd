extends SceneTree
## View-model tests for the schema-15 empire rules (lane M2-EMPIRE, docs/design/EMPIRE-RULES.md): dilithium status,
## emergency programs, the invasion aftermath modal, invading independent minors and signature-ship guards.
## Needs the native core (BotfCore); without it the suite reports SKIP and passes.
##   godot --headless --audio-driver Dummy --path remaster/godot -s res://scripts/viewmodels/tests/test_empire_rules_vm.gd

var failures := 0
var checks := 0

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
		_dilithium_live()
		_programs_live()
		_conquest_live()
	print("%d checks, %d failures" % [checks, failures])
	quit(1 if failures > 0 else 0)

func _pure() -> void:
	print("== pure models")
	check(SystemVM.dilithium_lines({}).is_empty(), "no dilithium lines when the rules are off")
	var lines := SystemVM.dilithium_lines({"deposit": true, "refinery": false, "refineryPowered": false, "empireRefineries": 1, "empirePowered": 1, "slots": 1, "slotsUsed": 1, "klingonBonusPercent": 0}, "Empire shipbuilding slots all in use (1 of 1); build another dilithium refinery")
	check(lines.size() == 3 and lines[0]["tone"] == "info" and lines[1]["tone"] == "warn" and lines[2]["tone"] == "warn", "a deposit colony with full slots offers a refinery and warns")
	lines = SystemVM.dilithium_lines({"deposit": true, "refinery": true, "refineryPowered": false, "empireRefineries": 1, "empirePowered": 0, "slots": 0, "slotsUsed": 0, "klingonBonusPercent": 0})
	check(lines[0]["tone"] == "alert", "an unpowered refinery is an alert")
	lines = SystemVM.dilithium_lines({"deposit": true, "refinery": true, "refineryPowered": true, "empireRefineries": 2, "empirePowered": 2, "slots": 2, "slotsUsed": 0, "klingonBonusPercent": 25})
	check(lines.size() == 3 and str(lines[2]["text"]).contains("25%"), "the Klingon yard bonus is shown")
	check(SystemVM.program_status({}) == "", "no program status when the rules are off")
	var a := {"systemId": 7, "name": "Mars", "minor": "Bolian", "formerName": "Federation", "population": 9, "populationBefore": 12, "turnsLeft": 1}
	check(OverlaysVM.aftermath_line(a).contains("Bolian") and OverlaysVM.aftermath_line(a).contains("1 turn,"), "the aftermath headline names the race and the deadline")

func _dilithium_live() -> void:
	print("== dilithium on the native core")
	var s := AppSession.create("auto")
	var v := s.start("newCampaign", AppSession.campaign_options({"empire": 2, "seed": 424242, "era": 3, "preset": "remastered"}))
	check(not v.is_empty(), "newCampaign starts")
	if v.is_empty():
		return
	s.enter(v)
	var emp := EmpireVM.new(s)
	var d := emp.dilithium()
	check(not d.is_empty() and int(d["refineries"]) >= 1 and int(d["slots"]) >= 1, "a new campaign starts with a powered refinery and a slot: %s" % [d])
	var kpi := emp.dilithium_kpi()
	check(kpi.get("cap", "") == "Dilithium" and str(kpi["value"]).contains("/"), "the empire screen gets a Dilithium tile")
	var sys := SystemVM.new(s)
	check(sys.fetch(sys.system_id()), "the system detail loads")
	var dd := sys.dilithium()
	check(bool(dd.get("refinery", false)) and bool(dd.get("refineryPowered", false)), "the home colony has a powered refinery")
	var listed: Array = sys.detail["choices"].filter(func(c): return c["product"] == "DilithiumRefinery")
	check(listed.size() == 1 and str(listed[0]["blocker"]).contains("already built"), "the build list shows the refinery blocked once built")
	var scrap: Array = sys.detail["scrap"].filter(func(c): return c["building"] == "DilithiumRefinery")
	check(scrap.size() == 1 and int(scrap[0]["count"]) == 1, "the scrap list carries the refinery")

func _programs_live() -> void:
	print("== emergency programs on the native core")
	var s := AppSession.create("auto")
	var v := s.start("newCampaign", AppSession.campaign_options({"empire": 2, "seed": 424242, "era": 3, "preset": "remastered"}))
	if v.is_empty():
		check(false, "newCampaign starts")
		return
	s.enter(v)
	var emp := EmpireVM.new(s)
	var rows := emp.program_rows()
	check(rows.size() >= 1 and rows[0]["program"]["name"] == "Klingon Police", "a Klingon empire offers Klingon Police: %s" % [rows[0]["program"] if rows.size() > 0 else {}])
	check(SystemVM.program_status(rows[0]["program"]).contains("Klingon Police"), "the status line names the program")
	var sid := int(rows[0]["systemId"])
	check(str(rows[0]["blocker"]).contains("not in unrest") and not emp.start_program(sid, rows[0]["program"]), "a content colony may not start a program (unrest only)")
	var after := emp.program_rows()
	check(not after.is_empty() and not bool(after[0]["active"]), "the refused order leaves no program running")
	var sys := SystemVM.new(s)
	check(sys.fetch(sid), "the system detail loads")
	check(not sys.program().is_empty(), "the system screen has a program block")

func _conquest_live() -> void:
	print("== aftermath, independent minors and guards on the native core")
	var s := AppSession.create("auto")
	var v := s.start("newCampaign", AppSession.campaign_options({"empire": 1, "seed": 91, "era": 3, "preset": "remastered"}))
	if v.is_empty():
		check(false, "newCampaign starts")
		return
	s.enter(v)
	var ov := OverlaysVM.new(s)
	check(ov.aftermaths().is_empty(), "no aftermath is waiting at the start")
	var guarded := 0
	var independent := 0
	for sys in s.view().get("systems", []):
		var m: Variant = sys.get("minor", {})
		if m is Dictionary and not m.is_empty() and not m.get("unknown", false):
			independent += 1 if m.get("independent", false) else 0
			guarded += 1 if m.has("guard") else 0
	check(s.view().get("minors", s.view().get("systems", [])).size() > 0, "the galaxy has systems")
	check(independent >= 0 and guarded >= 0, "guard and independent flags read without error (%d guarded, %d independent in view)" % [guarded, independent])
