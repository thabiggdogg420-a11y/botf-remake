shader_type canvas_item;
// Animated nebula cloud for galaxy features (Nebula, IonStorm, SubspaceRift). Drawn on a quad
// per feature; `tint` sets the colour, `seed` varies the shape, `swirl` is for rifts/wormholes.

uniform vec4 tint : source_color = vec4(0.55, 0.35, 0.9, 1.0);
uniform float seed = 0.0;
uniform float intensity = 0.8;
uniform float swirl = 0.0;
uniform float crackle = 0.0;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
	float v = 0.0; float a = 0.5;
	for (int i = 0; i < 5; i++) { v += a * noise(p); p = p * 2.1 + vec2(3.1, 1.3); a *= 0.5; }
	return v;
}

void fragment() {
	vec2 c = UV - 0.5;
	float r = length(c);
	float ang = atan(c.y, c.x);
	if (swirl > 0.0) {
		ang += swirl * (0.6 - r) * 6.0 + TIME * swirl * 0.8;
		c = vec2(cos(ang), sin(ang)) * r;
	}
	vec2 p = c * 3.2 + seed * 7.13;
	float n = fbm(p + vec2(TIME * 0.03, -TIME * 0.02));
	float n2 = fbm(p * 2.0 - vec2(TIME * 0.05));
	float mask = smoothstep(0.5, 0.12, r);
	float a = smoothstep(0.35, 0.85, n + 0.25 * n2) * mask * intensity;
	vec3 col = mix(tint.rgb * 0.7, tint.rgb * 1.4 + 0.15, n2);
	if (crackle > 0.0) {
		float bolt = smoothstep(0.96, 1.0, sin((n2 * 40.0 + TIME * 6.0)) * 0.5 + 0.5) * crackle * mask;
		col += vec3(0.8, 0.9, 1.0) * bolt;
		a += bolt * 0.8;
	}
	COLOR = vec4(col, clamp(a, 0.0, 1.0));
}
