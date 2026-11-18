class_name CompatBloom
extends Node
## Bloom for the web build (Compatibility renderer). Forward+ blooms engines, beams, fire and suns through the
## Environment's HDR glow; Compatibility's glow barely shows (WEB-PARITY.md, 19 Nov frame pairs: no halo on the system
## view's sun, a kill's fire or a beam's muzzle). This builds the glow from the 3D view's finished frame instead: a bright
## pass downsampled to 1/4 and 1/16 in two small 2D-only SubViewports, composited by the view's SubViewportContainer
## material with a screen blend. Three cheap full-screen passes, no 3D work; under Forward+ attach() does nothing.

const DOWN := preload("res://scenes/battle/shaders/compat_bloom_down.gdshader")
const COMPOSITE := preload("res://scenes/battle/shaders/compat_bloom.gdshader")

var src: SubViewport
var container: SubViewportContainer
var _a: SubViewport
var _b: SubViewport
var _ra: ColorRect
var _rb: ColorRect
var _mat: ShaderMaterial

## Adds the bloom chain to `container` (which shows `viewport`). threshold is on the tonemapped frame (0-1); the
## intensities scale the 1/4 (tight) and 1/16 (wide) levels.
static func attach(container_: SubViewportContainer, viewport: SubViewport, threshold := 0.7, intensity_a := 0.7, intensity_b := 1.1) -> CompatBloom:
	if not CompatLook.active() or container_ == null or viewport == null or CompatLook.flag("BOTF_NO_COMPAT_BLOOM") != "":
		return null
	CompatLook.viewport(viewport) # HDR buffer for views that do not go through DisplaySettings.apply_viewport
	var b := CompatBloom.new()
	b.name = "CompatBloom"
	b.src = viewport
	b.container = container_
	b._a = _level("BloomA")
	b._b = _level("BloomB")
	b._ra = _rect(viewport.get_texture(), threshold, false)
	b._rb = _rect(b._a.get_texture(), 0.0, true)
	b._a.add_child(b._ra)
	b._b.add_child(b._rb)
	b.add_child(b._a)
	b.add_child(b._b)
	b._mat = ShaderMaterial.new()
	b._mat.shader = COMPOSITE
	b._mat.set_shader_parameter("bloom_a", b._a.get_texture())
	b._mat.set_shader_parameter("bloom_b", b._b.get_texture())
	b._mat.set_shader_parameter("intensity_a", intensity_a)
	b._mat.set_shader_parameter("intensity_b", intensity_b)
	container_.material = b._mat
	# A plain Node child of the container: the container draws only its direct SubViewport children, so the levels
	# under it are never drawn on screen. Deferred: the container may be inside its parent's setup.
	container_.add_child.call_deferred(b)
	return b

static func _level(n: String) -> SubViewport:
	var v := SubViewport.new()
	v.name = n
	v.disable_3d = true
	v.transparent_bg = false
	v.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	v.size = Vector2i(4, 4)
	return v

static func _rect(tex: Texture2D, threshold: float, through: bool) -> ColorRect:
	var r := ColorRect.new()
	r.set_anchors_preset(Control.PRESET_FULL_RECT)
	var m := ShaderMaterial.new()
	m.shader = DOWN
	m.set_shader_parameter("src", tex)
	m.set_shader_parameter("threshold", threshold)
	m.set_shader_parameter("pass_through", through)
	r.material = m
	return r

func _ready() -> void:
	_resize()

func _process(_dt: float) -> void:
	_resize()

func _resize() -> void:
	if src == null or not is_instance_valid(src):
		return
	var s := src.size
	var sa := Vector2i(maxi(1, s.x / 4), maxi(1, s.y / 4))
	var sb := Vector2i(maxi(1, s.x / 16), maxi(1, s.y / 16))
	if _a.size != sa:
		_a.size = sa
		_ra.size = Vector2(sa)
		(_ra.material as ShaderMaterial).set_shader_parameter("src_px", Vector2(1.0 / maxf(1.0, s.x), 1.0 / maxf(1.0, s.y)))
		_mat.set_shader_parameter("a_px", Vector2(1.0 / sa.x, 1.0 / sa.y))
	if _b.size != sb:
		_b.size = sb
		_rb.size = Vector2(sb)
		(_rb.material as ShaderMaterial).set_shader_parameter("src_px", Vector2(1.0 / sa.x, 1.0 / sa.y))
		_mat.set_shader_parameter("b_px", Vector2(1.0 / sb.x, 1.0 / sb.y))
