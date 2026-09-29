// Expanding shockwave ring on a flat quad (local XZ plane). Scale the node for the radius.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

instance uniform vec4 ring_color : source_color = vec4(1.0, 0.7, 0.35, 1.0);
instance uniform float ring_age = 0.0;

float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	float r = length(p);
	float a = atan(p.y, p.x);
	float wobble = 0.012 * sin(a * 9.0 + ring_age * 5.0) + 0.008 * sin(a * 23.0);
	float ring = exp(-pow((r - 0.8 - wobble) / (0.025 + 0.06 * ring_age), 2.0));
	float inner = exp(-r * 3.0) * 0.3 * (1.0 - ring_age);
	float fade = pow(1.0 - ring_age, 1.5);
	ALBEDO = ring_color.rgb * (ring * 1.4 + inner) * fade;
	ALPHA = clamp((ring + inner) * fade, 0.0, 1.0);
}
