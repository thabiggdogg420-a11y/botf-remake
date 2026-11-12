class_name UiTokens
extends RefCounted
## Design tokens of the M1 UI (docs/UI-DESIGN-SYSTEM.md §2-§4): era looks, the type scale, spacing and motion.
## The empire accent lives in Lcars.PALETTES; the era sets surface, shape and header style on top of it.

## Era looks (§2.3). radius = panel corner, pill = button corner, header = frame header style.
const ERAS := {
	"tos": {"label": "TOS", "name": "The Original Series", "tint": "#0b0a12", "panel": "#15111d", "radius": 4, "pill": 6, "line": 1, "header": "bar", "stroke": 2.8, "hi": "#f2c14e", "lo": "#d6453d"},
	"movie": {"label": "Movie", "name": "Motion Picture era", "tint": "#070a10", "panel": "#0e131b", "radius": 6, "pill": 8, "line": 1, "header": "bracket", "stroke": 1.5, "hi": "#9fb4c8", "lo": "#c06a4a"},
	"tng": {"label": "TNG / DS9", "name": "Next Generation and Deep Space Nine", "tint": "#04060c", "panel": "#0a1022", "radius": 14, "pill": 18, "line": 1, "header": "cap", "stroke": 2.0, "hi": "#f2b45a", "lo": "#9d8cff"},
	"voy": {"label": "Voyager", "name": "Voyager", "tint": "#050a12", "panel": "#0b1528", "radius": 12, "pill": 16, "line": 1, "header": "cap", "stroke": 2.0, "hi": "#7fb3ff", "lo": "#d9a36a"},
	"late": {"label": "Late", "name": "Post-Nemesis", "tint": "#03050a", "panel": "#0a0f18", "radius": 6, "pill": 10, "line": 1, "header": "hairline", "stroke": 1.5, "hi": "#5fe0ff", "lo": "#ff8a5c"},
}
const ERA_ORDER := ["tos", "movie", "tng", "voy", "late"]

## Type scale (§3): [kind, weight, size].
const TYPE := {
	"display": ["display", 700, 64], "h1": ["display", 600, 40], "h2": ["display", 600, 22], "h3": ["display", 500, 17],
	"kpi": ["display", 560, 30], "stat": ["display", 560, 22], "body": ["ui", 420, 15], "small": ["ui", 420, 13],
	"caption": ["display", 500, 12], "mono": ["mono", 400, 14],
}
## Spacing on a 4 px grid (§3).
const SPACE := {"xs": 4, "sm": 8, "md": 12, "lg": 16, "xl": 24, "xxl": 32}
## Motion in seconds (§4).
const MOTION := {"fast": 0.12, "panel": 0.18, "screen": 0.2, "count": 0.4, "stagger": 0.03}

static var era := "tng"
static var reduced_motion := false

static func era_def(id: String = "") -> Dictionary:
	return ERAS.get(id if id != "" else era, ERAS["tng"])

static func radius() -> int:
	return int(era_def()["radius"])

static func pill_radius() -> int:
	return int(era_def()["pill"])

## Stroke weight of the UI icons on the 24 px grid: heavy for TOS, thin for the movies and Late.
static func icon_stroke() -> float:
	return float(era_def().get("stroke", 2.0))

static func header_style() -> String:
	return str(era_def()["header"])

static func tint() -> Color:
	return Color(era_def()["tint"])

static func panel() -> Color:
	return Color(era_def()["panel"])

static func era_hi() -> Color:
	return Color(era_def()["hi"])

static func era_lo() -> Color:
	return Color(era_def()["lo"])

## Seconds for a motion token; 0 under reduced motion.
static func dur(token: String) -> float:
	return 0.0 if reduced_motion else float(MOTION.get(token, 0.18))

static func font_for(token: String) -> Font:
	var t: Array = TYPE.get(token, TYPE["body"])
	return Lcars.font(t[0], t[1])

static func size_for(token: String) -> int:
	return int(TYPE.get(token, TYPE["body"])[2])
