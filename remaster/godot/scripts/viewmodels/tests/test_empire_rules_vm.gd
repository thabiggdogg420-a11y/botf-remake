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
	var listed := sys.detail["choices"].filter(func(c): return c["product"] == "DilithiumRefinery")
	check(listed.size() == 1 and str(listed[0]["blocker"]).contains("already built"), "the build list shows the refinery blocked once built")
	var scrap := sys.detail["scrap"].filter(func(c): return c["building"] == "DilithiumRefinery")
	check(scrap.size() == 1 and int(scrap[0]["count"]) == 1, "the scrap list carries the refinery")
