class_name LoadTrace
## Loading-hitch trace (web smoothness pass, 19 Nov 2026). On the web build, and on desktop with the user argument
## --load-trace, any traced loading step that takes 20 ms or more prints one line:
##   LOADT <label> <ms> ms (frame <n>)
## so a frame over budget can be pinned on the step inside it from the browser console. Costs one clock read per step.

static var on: bool = OS.has_feature("web") or OS.get_cmdline_user_args().has("--load-trace")


## Microseconds now: pass the result to done().
static func start() -> int:
	return Time.get_ticks_usec()


## Prints the step when it took at least `min_ms`. Returns the clock, so steps can be chained.
static func done(label: String, t0: int, min_ms: int = 20) -> int:
	var t := Time.get_ticks_usec()
	if on and t - t0 >= min_ms * 1000:
		print("LOADT %s %d ms (frame %d)" % [label, (t - t0) / 1000, Engine.get_process_frames()])
	return t
