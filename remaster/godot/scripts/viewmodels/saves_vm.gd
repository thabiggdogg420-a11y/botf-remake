class_name SavesVM
extends ViewModel
## Saves model: the slot list and its display lines.

var saves: Array = []

func fetch() -> void:
	saves = AppSaves.list()

static func describe(s: Dictionary) -> String:
	return "%s · turn %d · %s · %s save · %s" % [s.get("playerName", ""), int(s.get("turn", 0)), s.get("scenario", "campaign"), s.get("kind", "game"), s.get("when", "")]
