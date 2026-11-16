class_name PreloadWidget
extends VBoxContainer
## "Preload whole game" (web only; F1, Simon 16 Nov). Shows the total size, downloads every pack with a progress bar, keeps them
## on the device across visits, resumes where it stopped, and ends on "Whole game preloaded". The default stays lazy loading.
## The downloads run on the App node, so closing the panel that holds this widget does not stop them.

var app: App
var _btn: Button
var _label: Label
var _bar: ColorRect
var _fill: ColorRect
var _tick := 0.0

static func make(p_app: App) -> Control:
	if not OS.has_feature("web"):
		return Control.new()
	var w := PreloadWidget.new()
	w.app = p_app
	return w

func _ready() -> void:
	name = "Preload"
	add_theme_constant_override("separation", 6)
	_btn = Lcars.pill("Preload whole game", "tertiary", _on_press, "", "Downloads every ship, planet, sound and picture now and keeps them on this device, so nothing downloads during play. Optional: the game also loads what it needs as you play.")
	_btn.name = "PreloadButton"
	add_child(_btn)
	_bar = ColorRect.new()
	_bar.custom_minimum_size = Vector2(0, 8)
	_bar.color = Color(1, 1, 1, 0.16)
	_fill = ColorRect.new()
	_fill.color = Lcars.accent()
	_fill.size = Vector2(0, 8)
	_bar.add_child(_fill)
	add_child(_bar)
	_label = Lcars.label("", "Mono", 12, Lcars.TEXT3)
	_label.name = "PreloadStatus"
	_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	add_child(_label)
	_label.text = "Checking the size..."
	AppLoader.load_manifest(app)   # fills the pack list; the next refresh shows the size
	_refresh()

func _on_press() -> void:
	if AppLoader.preload_running:
		AppLoader.preload_cancel = true
	elif not AppLoader.fully_cached():
		AppLoader.preload_all(app)
	_refresh()

func _process(delta: float) -> void:
	_tick += delta
	if _tick >= 0.5:
		_tick = 0.0
		_refresh()

func _refresh() -> void:
	var total := AppLoader.total_bytes()
	if total <= 0:
		return
	var have := AppLoader.cached_bytes()
	var mb := 1048576.0
	var live := int(AppLoader.progress.get("bytes", 0)) if AppLoader.preload_running else 0
	var done := mini(have + live, total)
	_fill.size.x = _bar.size.x * clampf(float(done) / float(total), 0.0, 1.0)
	if AppLoader.preload_running:
		_btn.text = "Pause preload"
		_label.text = "Downloading %.0f of %.0f MB  ·  %s" % [done / mb, total / mb, AppLoader.preload_current.trim_suffix("_web.pck")]
	elif AppLoader.fully_cached():
		_btn.text = "Whole game preloaded"
		_btn.disabled = true
		_fill.size.x = _bar.size.x
		_label.text = "Whole game preloaded (%.0f MB kept on this device). Nothing downloads during play." % (total / mb)
	else:
		_btn.disabled = false
		_btn.text = ("Resume preload" if have > 0 else "Preload whole game") + "  (%.0f MB)" % ((total - have) / mb)
		if AppLoader.preload_error != "":
			_label.text = "Preload stopped at %s: check the connection and press again." % AppLoader.preload_error.trim_suffix("_web.pck")
		else:
			_label.text = "%.0f of %.0f MB already on this device. The rest downloads as you play, or all at once with this button." % [have / mb, total / mb]
