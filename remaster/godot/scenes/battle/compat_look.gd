class_name CompatLook
extends RefCounted
## Compatibility-renderer (web build, WebGL2) settings that bring its frame close to the desktop's Forward+ frame
## (M1 webparity; measured on desktop/web frame pairs, see docs/ART-BIBLE.md section 12.2). Nothing here runs under
## Forward+, so the desktop frame does not change.
##
## Measured on the title fleet (same hulls, same lights, 9-10 Nov): with the sun alone a Compatibility hull read 133 against
## Forward+'s 114 (mean 8-bit level), while fill, rim and ambient alone matched within a few levels; the sun is scaled by
## SUN_SCALE. Raising the exposure over scaled-down lights made hulls whiter, not truer, so the exposure is left alone.
## Glow: Compatibility uses a simpler glow (the docs: glow levels, blend mode, strength, mix and normalisation have no
## effect) over a low dynamic range buffer, so Forward+'s 1.8 threshold never fires (no bloom on engines, beams or the
## sun). The threshold, scale and intensity below give the beams and engines their bloom without blooming lit hulls; an
## intensity of 0.55 bloomed a volley's muzzle into a white blob twice the desktop's halo, 0.3 matches its size.

const SUN_SCALE := 0.85
const GLOW_THRESHOLD := 1.0
const GLOW_INTENSITY := 0.3
const GLOW_HDR_SCALE := 1.2

static func active() -> bool:
	return RenderingServer.get_current_rendering_method() == "gl_compatibility"

## Tunes `env`'s glow for Compatibility (no-op under Forward+).
static func tune(env: Environment) -> void:
	if not active() or env == null:
		return
	if hdr():
		return # an HDR 3D buffer (viewport below): the Forward+ glow settings fire as they do on the desktop
	env.glow_hdr_threshold = GLOW_THRESHOLD
	env.glow_hdr_scale = GLOW_HDR_SCALE
	env.glow_intensity = GLOW_INTENSITY

## The energy the key light (a sun) should have under the active renderer.
static func sun(energy: float) -> float:
	return energy * SUN_SCALE if active() else energy

## Whether Compatibility 3D views render to a half-float (HDR) buffer. Off: tried 19 Nov (WEB-PARITY.md). A kill's
## fireball kept its shape, but Godot 4.5 Compatibility skips the Environment's tonemapper on an HDR 2D buffer, so lit
## hulls, the station and the title planet blew out to white. BOTF_COMPAT_HDR=1 (environment or web user argument)
## turns it on for experiments.
static func hdr() -> bool:
	return flag("BOTF_COMPAT_HDR") == "1"

## A 3D viewport under Compatibility (no-op under Forward+).
static func viewport(v: Viewport) -> void:
	if not active() or v == null:
		return
	v.use_hdr_2d = hdr()

## A diagnostics switch from the environment or, on the web (no environment), a user argument NAME=value.
static func flag(name_: String) -> String:
	var v := OS.get_environment(name_)
	if v != "":
		return v
	for a in OS.get_cmdline_user_args():
		if str(a).begins_with(name_ + "="):
			return str(a).substr(name_.length() + 1)
	return ""
