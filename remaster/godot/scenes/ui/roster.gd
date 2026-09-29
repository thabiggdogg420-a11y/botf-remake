class_name Roster
extends RefCounted
## Presentation helpers for the expanded ship roster. Port of remaster/web/src/core/roster.ts.

const ERA_ORDER := ["Enterprise", "Original", "Movie", "NextGeneration", "DominionWar", "Nemesis", "Kelvin", "Future"]
const ERA_LABEL := {"Enterprise": "Enterprise era", "Original": "Original series", "Movie": "Movie era", "NextGeneration": "Next Generation",
	"DominionWar": "Dominion War", "Nemesis": "Nemesis era", "Kelvin": "Kelvin timeline", "Future": "Future"}
const SOURCE_LABEL := {"Canon": "Canon", "ModDerived": "Mod-derived", "Kelvin": "Kelvin"}
const ARC_LABEL := {"Forward": "fore", "ForwardWide": "wide fore", "Broadside": "broadside", "Aft": "aft", "All": "360°"}
const WEAPON_LABEL := {"TricobaltDevice": "Tricobalt device", "Torpedo": "Torpedo launcher"}
const FACTION_LABEL := {"Species8472": "Species 8472", "SonA": "Son'a"}

static func split_camel(s: String) -> String:
	var out := ""
	for i in s.length():
		var ch := s[i]
		if i > 0 and ch == ch.to_upper() and ch != ch.to_lower() and s[i - 1] == s[i - 1].to_lower():
			out += " "
		out += ch
	return out

static func era_label(e: String) -> String:
	return ERA_LABEL.get(e, split_camel(e))

static func source_label(k: String) -> String:
	return SOURCE_LABEL.get(k, k)

static func role_label(r: String) -> String:
	return split_camel(r)

static func faction_label(f: String) -> String:
	return FACTION_LABEL.get(f, f)

static func weapon_line(w: Dictionary) -> String:
	return "%dx %s (%s)" % [int(w["count"]), WEAPON_LABEL.get(w["type"], split_camel(str(w["type"]))), ARC_LABEL.get(w["arc"], w["arc"])]

static func weapon_summary(ws: Variant) -> String:
	if not ws is Array or ws.is_empty():
		return ""
	return ", ".join(ws.map(func(w): return weapon_line(w)))
