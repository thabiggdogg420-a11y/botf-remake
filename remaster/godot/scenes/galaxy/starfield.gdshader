shader_type canvas_item;
// Three-layer parallax starfield with twinkle, plus a faint accent-tinted nebula haze.
// `offset` is the camera position in map pixels; each layer scrolls at its own depth.

uniform vec2 offset = vec2(0.0);
uniform float zoom = 1.0;
uniform vec4 accent : source_color = vec4(0.43, 0.66, 1.0, 1.0);
uniform float haze = 0.55;
uniform vec2 resolution = vec2(1920.0, 1080.0);

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 hash2(vec2 p) { return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453); }

float noise(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
	float v = 0.0; float a = 0.5;
	for (int i = 0; i < 5; i++) { v += a * noise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
	return v;
}

float star_layer(vec2 uv, float scale, float density, float t) {
	vec2 g = uv * scale;
	vec2 cell = floor(g);
	vec2 f = fract(g);
	float s = 0.0;
	vec2 h = hash2(cell);
	if (h.x < density) {
		vec2 pos = hash2(cell + 13.1) * 0.8 + 0.1;
		float d = length(f - pos);
		float size = mix(0.02, 0.07, h.y);
		float tw = 0.65 + 0.35 * sin(t * (1.0 + h.y * 3.0) + h.x * 40.0);
		s = smoothstep(size, 0.0, d) * tw;
	}
	return s;
}

void fragment() {
	vec2 px = UV * resolution;
	float t = TIME;
	vec3 col = vec3(0.012, 0.02, 0.043);
	// nebula haze
	vec2 q = (px + offset * 0.15) / 900.0;
	float n = fbm(q * 1.6 + vec2(t * 0.004, 0.0));
	float n2 = fbm(q * 3.1 - vec2(0.0, t * 0.003) + 4.0);
	vec3 neb = mix(vec3(0.16, 0.08, 0.30), accent.rgb * 0.55, n2);
	col += neb * smoothstep(0.45, 0.95, n) * 0.35 * haze;
	col += vec3(0.05, 0.1, 0.2) * smoothstep(0.55, 1.0, n2) * 0.25 * haze;
	// star layers
	float s1 = star_layer((px + offset * 0.1) / 1000.0, 90.0, 0.35, t);
	float s2 = star_layer((px + offset * 0.3) / 1000.0, 45.0, 0.25, t * 0.7);
	float s3 = star_layer((px + offset * 0.6) / 1000.0, 20.0, 0.12, t * 0.5);
	col += vec3(0.7, 0.78, 1.0) * s1 * 0.55;
	col += vec3(0.9, 0.92, 1.0) * s2 * 0.8;
	col += mix(vec3(1.0, 0.85, 0.7), vec3(0.75, 0.85, 1.0), hash(floor(px / 50.0))) * s3;
	// vignette
	vec2 c = UV - 0.5;
	col *= 1.0 - dot(c, c) * 0.9;
	COLOR = vec4(col, 1.0);
}
