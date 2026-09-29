// Cloak shimmer, applied as material_overlay on every hull part while a ship cloaks or decloaks: refracts the scene
// behind the hull through a rippling distortion and lights the silhouette with a cold rim.
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_back, shadows_disabled;

uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;
instance uniform float shimmer = 0.0;
instance uniform vec4 shimmer_color : source_color = vec4(0.55, 0.8, 1.0, 1.0);

varying vec3 v_pos;

void vertex() {
	v_pos = VERTEX;
}

void fragment() {
	float t = TIME;
	vec3 p = v_pos * 6.0;
	vec2 wobble = vec2(sin(p.x * 3.0 + p.y * 2.0 + t * 7.0), cos(p.z * 3.0 - p.x * 1.5 + t * 6.0));
	vec2 uv = SCREEN_UV + wobble * 0.012 * shimmer;
	vec3 bg = textureLod(screen_tex, uv, 1.5 * shimmer).rgb;
	float fres = pow(1.0 - abs(dot(normalize(NORMAL), VIEW)), 3.0);
	float bands = 0.5 + 0.5 * sin(v_pos.x * 40.0 - t * 18.0);
	ALBEDO = bg + shimmer_color.rgb * (fres * 3.0 + bands * 0.3 * fres) * shimmer;
	ALPHA = clamp(shimmer * 1.2, 0.0, 1.0);
}
