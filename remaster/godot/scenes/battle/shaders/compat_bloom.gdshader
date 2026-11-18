// CompatBloom (web build): composites the bloom chain over the 3D view as the container draws it. Each level is read
// through a 9-tap tent (bilinear upsample, so the glow is smooth at any size) and screen-blended, as Forward+'s glow
// (GLOW_BLEND_MODE_SCREEN). TEXTURE is the 3D view's own frame.
shader_type canvas_item;

uniform sampler2D bloom_a : filter_linear, repeat_disable;   // 1/4 size, bright pass
uniform sampler2D bloom_b : filter_linear, repeat_disable;   // 1/16 size
uniform vec2 a_px = vec2(0.0021, 0.0037);
uniform vec2 b_px = vec2(0.0083, 0.0148);
uniform float intensity_a = 0.55;
uniform float intensity_b = 0.45;

vec3 tent(sampler2D t, vec2 uv, vec2 px) {
	vec3 c = texture(t, uv).rgb * 4.0;
	c += (texture(t, uv + vec2(px.x, 0.0)).rgb + texture(t, uv - vec2(px.x, 0.0)).rgb
		+ texture(t, uv + vec2(0.0, px.y)).rgb + texture(t, uv - vec2(0.0, px.y)).rgb) * 2.0;
	c += texture(t, uv + px).rgb + texture(t, uv - px).rgb + texture(t, uv + vec2(px.x, -px.y)).rgb + texture(t, uv + vec2(-px.x, px.y)).rgb;
	return c / 16.0;
}

void fragment() {
	vec4 base = texture(TEXTURE, UV);
	vec3 g = tent(bloom_a, UV, a_px) * intensity_a + (tent(bloom_b, UV, b_px) + tent(bloom_b, UV, b_px * 2.5) + tent(bloom_b, UV, b_px * 5.0)) * 0.42 * intensity_b;
	g = clamp(g, 0.0, 1.0);
	COLOR = vec4(1.0 - (1.0 - base.rgb) * (1.0 - g), 1.0); // the 3D view is opaque; its alpha channel holds transparent passes' coverage, not see-through
}
