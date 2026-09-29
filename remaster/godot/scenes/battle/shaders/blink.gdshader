// Navigation / strobe light: a steady glow with a short bright flash each period.
shader_type spatial;
render_mode unshaded, shadows_disabled;

instance uniform vec4 light_color : source_color = vec4(1.0, 0.1, 0.1, 1.0);
instance uniform float blink_phase = 0.0;
instance uniform float blink_rate = 0.8;

void fragment() {
	float f = fract(TIME * blink_rate + blink_phase);
	float flash = exp(-f * 18.0);
	ALBEDO = light_color.rgb * (1.5 + 14.0 * flash);
}
