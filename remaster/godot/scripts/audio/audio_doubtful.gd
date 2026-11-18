class_name AudioDoubtful
extends RefCounted
## Trek library clips that did not pass by ear (docs/AUDIO-DOUBTFUL.md, and fail/doubtful rows of docs/AUDIO-LISTEN-SHEET.md).
## The library never plays them, even when an older audio pack still lists them. Names only (pack file stems).
## Regenerate from the two docs when they change (see the F3 commit for the extraction).

const STEMS := {
	"ds9_explosion_explosion_s05e26_1974": true,
	"ds9_explosion_explosion_s06e05_2148": true,
	"ds9_impact_impact_s06e05_1298": true,
	"ds9_voice_fed_ack_s01e05_883": true,
	"ds9_voice_fed_energize_s01e06_2476": true,
	"ds9_voice_fed_hull_breach_s01e01_107": true,
	"ds9_voice_fed_shields_up_s01e12_581": true,
	"ent_explosion_explosion_s02e01_872": true,
	"ent_explosion_explosion_s03e24_1199": true,
	"ent_explosion_explosion_s04e02_1775": true,
	"ent_impact_impact_s02e01_189": true,
	"tng_explosion_explosion_s02e16_1272": true,
	"tng_explosion_explosion_s02e16_1870": true,
	"tng_explosion_explosion_s03e10_1323": true,
	"tng_explosion_explosion_s03e10_2585": true,
	"tng_explosion_explosion_s03e15_1819": true,
	"tng_explosion_explosion_s03e26_1049": true,
	"tng_explosion_explosion_s03e26_1121": true,
	"tng_explosion_explosion_s04e26_1732": true,
	"tng_explosion_explosion_s05e01_2387": true,
	"tng_impact_impact_s03e10_689": true,
	"tng_impact_impact_s03e15_2533": true,
	"tng_impact_impact_s03e26_4221": true,
	"tng_impact_impact_s04e26_1191": true,
	"tng_impact_impact_s04e26_654": true,
	"tng_impact_impact_s05e01_2320": true,
	"tng_launch_launch_s03e26_4190": true,
	"tng_launch_launch_s03e26_4223": true,
	"tng_voice_fed_ack_s02e20_554": true,
	"tng_voice_fed_all_stop_s02e13_2043": true,
	"tng_voice_fed_fire_s02e02_1191": true,
	"tng_voice_fed_fire_s02e16_1605": true,
	"tng_voice_fed_fire_s02e16_2261": true,
	"tng_voice_kli_fire_s02e16_1605": true,
	"tng_voice_kli_fire_s02e16_2261": true,
	"tos_explosion_explosion_s02e06_1496": true,
	"tos_impact_impact_s01e10_1866": true,
	"tos_impact_impact_s02e06_1452": true,
	"tos_voice_fed_ack_s01e14_2226": true,
	"tos_voice_fed_all_stop_s01e16_1470": true,
	"tos_voice_fed_battle_stations_s01e10_1019": true,
	"tos_voice_fed_battle_stations_s01e10_909": true,
	"tos_voice_fed_energize_s01e09_40": true,
	"tos_voice_fed_engage_s01e04_2748": true,
	"tos_voice_fed_engage_s01e12_2703": true,
	"tos_voice_fed_fire_s01e21_1048": true,
	"tos_voice_kli_ack_s01e14_2226": true,
	"tos_voice_kli_fire_s01e21_1048": true,
	"voy_explosion_explosion_s03e26_1301": true,
	"voy_explosion_explosion_s04e09_298": true,
	"voy_impact_impact_s04e01_81": true,
	"voy_launch_launch_s04e01_1893": true,
	"voy_launch_launch_s04e08_464": true,
	"voy_voice_fed_ack_s01e09_2038": true,
	"voy_voice_fed_all_stop_s02e02_1388": true,
	"voy_voice_fed_all_stop_s02e08_1624": true,
	"voy_voice_fed_fire_s01e01_5131": true,
	"voy_voice_fed_shields_down_s01e16_1549": true,
	"voy_voice_kli_fire_s01e01_5131": true,
}

static func blocked(path: String) -> bool:
	if STEMS.has(path.get_file().get_basename()):
		return true
	var parts := path.replace("\\", "/").split("/")   # a library path: .../trek/<era>/<kind>/<name>.wav
	if parts.size() >= 3:
		return STEMS.has("%s_%s_%s" % [parts[parts.size() - 3], parts[parts.size() - 2], parts[parts.size() - 1].get_basename()])
	return false
