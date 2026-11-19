class_name TitleFleet
extends Node3D
## The live backdrop of the title menu (Unreal: ABotfCombatScene::ConfigureTitleScene): one power's hero fleet, a
## lead ship with two escorts trailing high and low, flying right of centre past a gas giant under the nebula sky,
## lit by a three-quarter front key. It reuses the battle's environment (BattleEnvironment) and ships (BattleShip:
## a local hero model when LocalAssets has one, else the procedural faction hull) with no timeline playback.

## Lead ship first; the escorts fly off its quarters. [asset key, class name, canon length in metres]
const HEROES := {
	1: [["ship.fed.sovereign", "Sovereign", 685.0], ["ship.fed.galaxy", "Galaxy", 642.0], ["ship.fed.defiant", "Defiant", 170.0]],
	2: [["ship.kli.neghvar", "Negh'Var", 682.0], ["ship.kli.vorcha", "Vor'cha", 481.0], ["ship.kli.bop", "Bird-of-Prey", 110.0]],
	3: [["ship.rom.dderidex", "D'deridex", 1041.0], ["ship.rom.valdore", "Valdore", 843.0], ["ship.rom.bop", "Bird-of-Prey", 180.0]],
	4: [["ship.car.keldon", "Keldon", 400.0], ["ship.car.galor", "Galor", 371.0], ["ship.car.hideki", "Hideki", 80.0]],
	5: [["ship.fer.dkora", "D'Kora", 350.0], ["ship.fer.nagus", "Nagus", 520.0], ["ship.fer.pod", "Pod", 45.0]],
}
const HERO_LENGTH := 30.0   ## World units of the lead ship.
## Ship frame (bow +x, up +y, starboard +z), in lead-ship lengths: the escorts trail high and low, deeper than the
## lead, so all three stay in frame against the gas giant (the Unreal formation over its 340-unit hero).
const FORMATION := [Vector3(0, 0, 0), Vector3(-1.06, -0.32, 0.49), Vector3(-1.06, 0.26, 0.88)]
const HEADING_DEG := -138.0 ## The bows turn toward the camera and screen-left: a three-quarter hero view.
const BANK_DEG := [0.0, -8.0, 8.0]

@export var animate := true ## False holds the opening frame (reduced motion, screenshots of a fixed frame).

var empire := 1
var assets: BattleAssets
var ships: Array[BattleShip] = []
var camera: Camera3D
var env := {}
var time := 0.0
var _base: Array[Transform3D] = []


## empire 1-5; p_assets: optional BattleAssets (hero models when LocalAssets has them; null = procedural hulls).
func setup(p_empire: int, p_assets: BattleAssets = null) -> void:
	empire = clampi(p_empire, 1, 5)
	assets = p_assets
	name = "TitleFleet"
	var t := LoadTrace.start()
	env = BattleEnvironment.build(self, Vector2(16, 12) * BattleScene.CELL)
	t = LoadTrace.done("title fleet: environment", t)
	_stage_environment()
	t = LoadTrace.done("title fleet: stage", t)
	_build_ships()
	t = LoadTrace.done("title fleet: ships", t)
	camera = Camera3D.new()
	camera.name = "Camera"
	camera.fov = 40.0
	camera.near = 0.5
	camera.far = 6000.0
	add_child(camera)
	camera.current = true
	_pose(0.0)


## Lengths in world units for the lead and escorts: escorts keep their canon size relative to the lead (a Defiant
## beside a Sovereign is a quarter of its length) between 45/170 and 150/170 of it, as the Unreal title does.
static func lengths_for(canon: Array) -> Array:
	var out := []
	var lead := float(canon[0]) if canon.size() > 0 else 0.0
	for i in canon.size():
		if i == 0:
			out.append(HERO_LENGTH)
		elif lead > 0.0 and float(canon[i]) > 0.0:
			out.append(HERO_LENGTH * clampf(float(canon[i]) / lead, 45.0 / 170.0, 150.0 / 170.0))
		else:
			out.append(HERO_LENGTH * 110.0 / 170.0)
	return out


func _build_ships() -> void:
	var picks: Array = HEROES[empire]
	if assets != null:
		# The three hulls parse in parallel on worker threads (a Galaxy alone is ~2 s), not one after another.
		var keys := PackedStringArray()
		for p in picks:
			keys.append(p[0])
		assets.warm(keys)
	var canon := []
	for p in picks:
		var l := float(p[2])
		if assets != null:
			var h := assets.hero_length(PackedStringArray([p[0]]))
			if h > 0.0:
				l = h
		canon.append(l)
	var lengths := lengths_for(canon)
	var heading := Basis(Vector3.UP, deg_to_rad(HEADING_DEG))
	for i in picks.size():
		var ship := BattleShip.new()
		add_child(ship)
		# maxHull picks the larger procedural variant for the lead; displayLength sets the size outright.
		ship.setup({"id": i + 1, "owner": empire, "name": picks[i][1], "assetKey": picks[i][0],
			"maxHull": 150 if i == 0 else 70, "displayLength": lengths[i]}, assets)
		var at: Vector3 = heading * (FORMATION[i] * HERO_LENGTH)
		var basis := heading * Basis(Vector3.RIGHT, deg_to_rad(BANK_DEG[i]))
		_base.append(Transform3D(basis, at))
		ship.transform = _base[i]
		ships.append(ship)


func _stage_environment() -> void:
	# Sun: a three-quarter front key from the upper left of the view, so the hulls face the light.
	var sun: DirectionalLight3D = env.sun
	sun.rotation_degrees = Vector3(-28.0, -38.0, 0.0)
	sun.light_energy = CompatLook.sun(2.4)
	sun.directional_shadow_max_distance = 400.0
	# Gas giant: large, low on the right, behind the fleet.
	var planet: MeshInstance3D = env.planet
	# True scale (Simon, 8 Nov 2026): the planet is a distant backdrop, never a ball beside the hulls.
	planet.scale = Vector3.ONE * 1500.0
	planet.position = Vector3(2900.0, -1500.0, -4200.0)
	planet.rotation_degrees = Vector3(14.0, 20.0, 12.0)
	# Paler cream-and-umber bands than the battle's: the backdrop sits behind text and must not shout.
	var pm := (planet.material_override as ShaderMaterial).duplicate() as ShaderMaterial
	pm.set_shader_parameter("band_a", Color(0.84, 0.72, 0.56))
	pm.set_shader_parameter("band_b", Color(0.5, 0.36, 0.24))
	pm.set_shader_parameter("band_c", Color(0.94, 0.9, 0.82))
	planet.material_override = pm


## The camera opens straight on the shot: a slow lateral drift and breathing distance keep it alive.
func _pose(t: float) -> void:
	for i in ships.size():
		var bob := Vector3(0.0, sin(t * 0.35 + i * 1.7) * 0.6, cos(t * 0.27 + i * 2.3) * 0.4)
		var roll := Basis(Vector3.RIGHT, sin(t * 0.21 + i) * 0.015)
		ships[i].transform = Transform3D(_base[i].basis * roll, _base[i].origin + bob)
	if camera != null:
		var drift := Vector3(sin(t * 0.05) * 6.0, sin(t * 0.07) * 2.0, cos(t * 0.04) * 4.0)
		camera.position = Vector3(-4.0, 9.0, 88.0) + drift
		camera.look_at(Vector3(-17.0, -3.0, 0.0) + drift * 0.5, Vector3.UP)


func _process(delta: float) -> void:
	if not animate:
		return
	time += delta
	_pose(time)
