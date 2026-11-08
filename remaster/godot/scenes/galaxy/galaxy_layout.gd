class_name GalaxyLayout
extends RefCounted
## Deterministic 3D placement for the galaxy map. The core places systems on a 2D sector grid; the client lifts that
## into a thin disc: each system keeps its sector, is offset inside it, and gets a height above or below the galactic
## plane. Everything is a pure function of (galaxy seed, sector x, sector y), so the same campaign always looks the
## same, on every machine and in both renderers. World units: 1 unit = 1 sector; sector (x, y) spans
## [x, x+1] x [y, y+1] on the XZ plane, Y is up.
##
## Star type: the core's view has no spectral class, so the client derives one from the same hash (weighted like a
## real stellar neighbourhood, scaled toward the visible kinds): the art bible's yellow, red, blue and white stars,
## plus orange.

## Half-thickness of the disc in sectors at the centre of the charted region; it thins to half at the edge.
const DISC_HALF := 1.1
## Largest in-sector offset from the sector centre (keeps every star inside its own sector).
const JITTER := 0.28

const STAR_TYPES := [
	{"id": "yellow", "name": "G yellow dwarf", "color": Color(1.0, 0.86, 0.58), "radius": 0.045, "spike": 1.0, "weight": 30},
	{"id": "orange", "name": "K orange dwarf", "color": Color(1.0, 0.64, 0.36), "radius": 0.04, "spike": 0.8, "weight": 22},
	{"id": "red", "name": "M red dwarf", "color": Color(1.0, 0.42, 0.26), "radius": 0.032, "spike": 0.55, "weight": 22},
	{"id": "white", "name": "F/A white star", "color": Color(0.93, 0.95, 1.0), "radius": 0.05, "spike": 1.2, "weight": 16},
	{"id": "blue", "name": "B blue giant", "color": Color(0.62, 0.74, 1.0), "radius": 0.06, "spike": 1.5, "weight": 10},
]

## A 32-bit integer hash (PCG-style mix) folded to [0, 1). Integer only, so it matches on every platform.
static func hash01(a: int, b: int, c: int, salt: int) -> float:
	var h := (a * 73856093) ^ (b * 19349663) ^ (c * 83492791) ^ (salt * 2654435761)
	h = h & 0xFFFFFFFF
	h = ((h >> 16) ^ h) * 0x45d9f3b
	h = h & 0xFFFFFFFF
	h = ((h >> 16) ^ h) * 0x45d9f3b
	h = h & 0xFFFFFFFF
	h = (h >> 16) ^ h
	return float(h & 0xFFFFFF) / float(0x1000000)

## The 3D position of a star system at sector (x, y) in a galaxy of `grid` sectors made with `seed`.
static func system_position(x: int, y: int, grid: Vector2i, seed: int) -> Vector3:
	var jx := (hash01(x, y, seed, 1) - 0.5) * 2.0 * JITTER
	var jz := (hash01(x, y, seed, 2) - 0.5) * 2.0 * JITTER
	# Height: the sum of two uniforms is a triangle distribution, so most stars sit near the plane.
	var hz := (hash01(x, y, seed, 3) + hash01(x, y, seed, 4) - 1.0)
	var c := Vector2(grid) * 0.5
	var r := (Vector2(x + 0.5, y + 0.5) - c).length() / maxf(c.length(), 1.0)
	var half := DISC_HALF * lerpf(1.0, 0.5, clampf(r, 0.0, 1.0))
	return Vector3(x + 0.5 + jx, hz * half, y + 0.5 + jz)

## Index into STAR_TYPES for the system at (x, y).
static func star_type(x: int, y: int, seed: int) -> int:
	var total := 0
	for t in STAR_TYPES:
		total += int(t["weight"])
	var pick := hash01(x, y, seed, 5) * total
	var acc := 0.0
	for i in STAR_TYPES.size():
		acc += float(STAR_TYPES[i]["weight"])
		if pick < acc:
			return i
	return 0

## Orbit radius (sectors) and phase for planet `index` of `count` around a star of the given type.
static func orbit(index: int, count: int, x: int, y: int, seed: int) -> Vector2:
	var inner := 0.09
	var outer := 0.40
	var t := (index + 0.5) / maxf(float(count), 1.0)
	var r := lerpf(inner, outer, pow(t, 1.15))
	var phase := hash01(x, y, seed, 20 + index) * TAU
	return Vector2(r, phase)

## Orbital angular speed (radians per second of real time; outer planets move slower, Kepler-like).
static func orbit_speed(radius: float) -> float:
	return 0.06 / pow(maxf(radius, 0.05) / 0.1, 1.5)
