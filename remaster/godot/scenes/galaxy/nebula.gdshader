shader_type canvas_item;
// Animated cloud for galaxy features (Nebula, IonStorm, SubspaceRift, Wormhole, BlackHole). Drawn on a quad per feature;
// `tint` sets the colour, `seed` varies the shape, `swirl` is for rifts/wormholes, `crackle` adds ion lightning.
//
// M0 L6 (8 Nov 2026) fix for the blocky ion storm. Three causes, three fixes:
//  1. hash() used fract(sin(dot())), which loses precision on mobile/GL GPUs once seed*7.13 pushes the lattice
//     coordinate high, so whole lattice cells took the same value (visible squares). It is now a sin-free integer-style hash.
//  2. The lightning was smoothstep(0.96, 1.0, sin(n2*40 + TIME*6)): a razor-thin band of a fast sine, aliased into
//     rectangular flecks. It is now a thin filament of a ridged field with a width that follows the screen derivative.
//  3. Every octave sampled the same axis-aligned lattice, so the cell grid reinforced itself across octaves. Each octave
//     is now rotated, and the lattice uses a quintic fade.
// Compatibility (WebGL2) safe: canvas_item only, constant loop bounds, no textures, no derivatives beyond fwidth().

uniform vec4 tint : source_color = vec4(0.55, 0.35, 0.9, 1.0);
uniform float seed = 0.0;
uniform float intensity = 0.8;
uniform float swirl = 0.0;
uniform float crackle = 0.0;

float hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}
const mat2 ROT = mat2(vec2(0.8, 0.6), vec2(-0.6, 0.8));
float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 5; i++) {
		v += a * noise(p);
		p = ROT * p * 2.03 + vec2(3.1, 1.3);
		a *= 0.5;
	}
	return v;
}

void fragment() {
	vec2 c = UV - 0.5;
	float r = length(c);
	if (swirl > 0.0) {
		float ang = atan(c.y, c.x);
		ang += swirl * (0.6 - r) * 6.0 + TIME * swirl * 0.8;
		c = vec2(cos(ang), sin(ang)) * r;
	}
	// Keep the seed offset small and fractional so the lattice never runs out of float precision.
	vec2 p = c * 3.2 + vec2(fract(seed * 0.137) * 40.0, fract(seed * 0.291) * 40.0);
	float n = fbm(p + vec2(TIME * 0.03, -TIME * 0.02));
	float n2 = fbm(p * 2.0 - vec2(TIME * 0.05));
	float mask = smoothstep(0.5, 0.12, r);
	float a = smoothstep(0.30, 0.90, n + 0.25 * n2) * mask * intensity;
	vec3 col = mix(tint.rgb * 0.7, tint.rgb * 1.4 + 0.15, n2);
	if (crackle > 0.0) {
		// Ridged field: |n - 0.5| is zero along a continuous curve; a narrow window around zero is a filament.
		float ridge = abs(fbm(p * 1.6 + vec2(TIME * 0.35, -TIME * 0.2)) - 0.5);
		float w = max(fwidth(ridge) * 1.5, 0.012);
		float bolt = (1.0 - smoothstep(0.0, w * 2.0, ridge)) * crackle * mask;
		bolt *= 0.55 + 0.45 * sin(TIME * 9.0 + n2 * 12.0); // flicker, but the shape stays smooth
		col += vec3(0.8, 0.9, 1.0) * bolt;
		a += bolt * 0.6;
	}
	COLOR = vec4(col, clamp(a, 0.0, 1.0));
}
