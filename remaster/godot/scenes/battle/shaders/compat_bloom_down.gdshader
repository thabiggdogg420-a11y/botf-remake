// CompatBloom (web build): bright pass and 4x downsample of a 3D view's final frame. Four bilinear taps cover the 4x4
// source block; a soft knee keeps the edge of the threshold from banding. pass_through=true skips the bright pass (the
// second, 16x level downsamples the first).
shader_type canvas_item;
render_mode unshaded, blend_disabled;

uniform sampler2D src : filter_linear, repeat_disable;
uniform vec2 src_px = vec2(0.00052, 0.00093);
uniform float threshold = 0.72;
uniform float knee = 0.2;
uniform bool pass_through = false;

vec3 bright(vec3 c) {
	if (pass_through) {
		return c;
	}
	float l = max(c.r, max(c.g, c.b));
	float s = clamp(l - threshold + knee, 0.0, 2.0 * knee);
	s = s * s / (4.0 * knee + 1e-4);
	float w = max(s, l - threshold) / (max(l, 1e-4) * max(1.0 - threshold, 0.05)); // white maps to full strength
	return c * w;
}

void fragment() {
	vec2 uv = UV;
	vec3 acc = vec3(0.0);
	acc += bright(texture(src, uv + src_px * vec2(-1.0, -1.0)).rgb);
	acc += bright(texture(src, uv + src_px * vec2(1.0, -1.0)).rgb);
	acc += bright(texture(src, uv + src_px * vec2(-1.0, 1.0)).rgb);
	acc += bright(texture(src, uv + src_px * vec2(1.0, 1.0)).rgb);
	COLOR = vec4(acc * 0.25, 1.0);
}
