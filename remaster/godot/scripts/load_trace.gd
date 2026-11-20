class_name LoadTrace
## Loading-hitch trace (web smoothness pass, 19 Nov 2026). On the web build, and on desktop with the user argument
## --load-trace, any traced loading step that takes 20 ms or more prints one line:
##   LOADT <label> <ms> ms (frame <n>)
## so a frame over budget can be pinned on the step inside it from the browser console. Costs one clock read per step.

static var on: bool = OS.has_feature("web") or OS.get_cmdline_user_args().has("--load-trace")


## Microseconds now: pass the result to done().
static func start() -> int:
	return Time.get_ticks_usec()


## Prints a marker line (LOADT-MARK <label>) when tracing: for work that runs outside GDScript (JS promises, the browser).
static func mark(label: String) -> void:
	if on:
		print("LOADT-MARK %s (frame %d)" % [label, Engine.get_process_frames()])
		_f_last = label


## Prints the step when it took at least `min_ms`. Returns the clock, so steps can be chained.
static var min_override := -1   # tools (open_probe.gd) lower every step's threshold
static func done(label: String, t0: int, min_ms: int = 20) -> int:
	var t := Time.get_ticks_usec()
	if min_override >= 0:
		min_ms = min_override
	if on and t - t0 >= min_ms * 1000:
		print("LOADT %s %d ms (frame %d)" % [label, (t - t0) / 1000, Engine.get_process_frames()])
		_f_last = label
	return t


# ---- Frame split (WebGPU round 9, 19 Nov 2026) ----------------------------------------------------------------------
# A frame over FRAME_MS prints LOADT-FRAME with where its time went: `idle` from the previous frame's draw end to this
# frame's process start (the browser, GC, JS promise callbacks, engine work before scripts), `process` (scripts, signals,
# scene work) and `draw` (rendering: pipeline and shader creation, uploads). The last traced labels ride along.
static var FRAME_MS := 150   # tools lower it (open_probe.gd)
static var _f_proc := 0
static var _f_pre := 0
static var _f_post := 0
static var _f_last := ""

static func split_frames(tree: SceneTree) -> void:
	if not on:
		return
	tree.process_frame.connect(func() -> void:
		var now := Time.get_ticks_usec()
		if _f_post > 0 and now - _f_post >= FRAME_MS * 1000:
			print("LOADT-FRAME gap before process %d ms (frame %d) last: %s" % [(now - _f_post) / 1000, Engine.get_process_frames(), _f_last])
		_f_proc = now)
	RenderingServer.frame_pre_draw.connect(func() -> void: _f_pre = Time.get_ticks_usec())
	RenderingServer.frame_post_draw.connect(func() -> void:
		_f_post = Time.get_ticks_usec()
		if _f_proc > 0 and _f_post - _f_proc >= FRAME_MS * 1000:
			print("LOADT-FRAME %d ms: process %d draw %d (frame %d) last: %s" % [(_f_post - _f_proc) / 1000, (_f_pre - _f_proc) / 1000, (_f_post - _f_pre) / 1000, Engine.get_process_frames(), _f_last]))
