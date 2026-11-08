class_name TitleVM
extends ViewModel
## Title model: the latest save (for Continue), the power the live backdrop films, and the build footer.

const EMPIRE_NAMES := {1: "federation", 2: "klingon", 3: "romulan", 4: "cardassian", 5: "ferengi"}

var saves: Array = []

func fetch() -> void:
	saves = AppSaves.list()

func latest() -> Dictionary:
	return saves[0] if saves.size() > 0 else {}

## The power the title films: $BOTF_TITLE_EMPIRE (screenshots), the campaign's, `kept` (a refresh keeps it), the
## latest save's, else a random one.
func title_empire(pinned: int, kept: int) -> int:
	if pinned >= 1 and pinned <= 5:
		return pinned
	if session != null and session.in_game() and session.player() >= 1 and session.player() <= 5:
		return session.player()
	if kept > 0:
		return kept
	if saves.size() > 0:
		var who := str(saves[0].get("playerName", "")).to_lower()
		for id in EMPIRE_NAMES:
			if who.find(EMPIRE_NAMES[id]) >= 0:
				return id
	return randi_range(1, 5)

func footer() -> String:
	var m := meta()
	return "Simulation core %s · save schema %s · backend: %s" % [m.get("contentVersion", "…"), str(m.get("saveVersion", "…")), session.core.backend]
