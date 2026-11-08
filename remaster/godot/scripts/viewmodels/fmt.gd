class_name Fmt
extends RefCounted
## Number formatting shared by view models and widgets (Lcars.num/signed/fmt delegate here).

static func num(value: Variant) -> String:
	if value is float and is_equal_approx(value, round(value)):
		return str(int(value))
	return str(value)

static func signed(n: float) -> String:
	return ("+%d" % n) if n > 0 else ("%d" % n)

## Thousands-separated integer: 12,345.
static func fmt(n: float) -> String:
	var i := int(round(n))
	var s := str(abs(i))
	var out := ""
	while s.length() > 3:
		out = "," + s.substr(s.length() - 3) + out
		s = s.substr(0, s.length() - 3)
	return ("-" if i < 0 else "") + s + out
