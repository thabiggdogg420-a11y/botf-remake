// Procedural deep-space sky: three layers of stars with colour temperature, a dusty galactic band and a
// domain-warped nebula. The sun disc and halo follow the first directional light.
shader_type sky;

uniform vec3 nebula_a : source_color = vec3(0.42, 0.10, 0.55);
uniform vec3 nebula_b : source_color = vec3(0.04, 0.22, 0.52);
uniform vec3 nebula_c : source_color = vec3(0.95, 0.35, 0.15);
uniform float nebula_strength = 0.9;
uniform float star_strength = 1.0;
uniform float band_strength = 0.6;
uniform vec3 band_normal = vec3(0.25, 0.9, 0.35);
uniform float sun_size = 0.012;

float hash13(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}
vec3 hash33(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}
float noise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash13(i), hash13(i + vec3(1, 0, 0)), f.x),
			mix(hash13(i + vec3(0, 1, 0)), hash13(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash13(i + vec3(0, 0, 1)), hash13(i + vec3(1, 0, 1)), f.x),
			mix(hash13(i + vec3(0, 1, 1)), hash13(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 5; i++) {
		v += a * noise3(p);
		p = p * 2.03 + vec3(1.7, 9.2, 3.1);
		a *= 0.5;
	}
	return v;
}

vec3 star_color(float t) {
	// Cool red through white to blue.
	return mix(mix(vec3(1.0, 0.62, 0.4), vec3(1.0, 0.96, 0.9), smoothstep(0.0, 0.5, t)),
		vec3(0.65, 0.78, 1.0), smoothstep(0.5, 1.0, t));
}

vec3 star_layer(vec3 dir, float scale, float threshold, float size) {
	vec3 p = dir * scale;
	vec3 cell = floor(p);
	vec3 col = vec3(0.0);
	for (int x = -1; x <= 1; x++)
	for (int y = -1; y <= 1; y++)
	for (int z = -1; z <= 1; z++) {
		vec3 c = cell + vec3(float(x), float(y), float(z));
		vec3 r = hash33(c);
		if (r.x < threshold) continue;
		vec3 sp = c + hash33(c + 17.0);
		float d = length(p - sp);
		float b = pow((r.x - threshold) / (1.0 - threshold), 3.0);
		float glow = exp(-d * d / (size * size));
		col += star_color(r.y) * glow * (0.15 + 3.0 * b);
	}
	return col;
}

void sky() {
	vec3 dir = normalize(EYEDIR);
	vec3 col = vec3(0.0015, 0.002, 0.004);

	// Galactic band: a dusty ribbon of faint stars.
	float band_d = dot(dir, normalize(band_normal));
	float band = exp(-band_d * band_d / 0.035);
	float dust = fbm(dir * 6.0);
	col += band * band_strength * vec3(0.14, 0.12, 0.16) * smoothstep(0.3, 0.8, dust);
	col -= band * band_strength * 0.05 * smoothstep(0.55, 0.75, fbm(dir * 11.0 + 4.0));

	// Nebula: domain-warped fbm, three hues.
	vec3 na = nebula_a;
	vec3 nb = nebula_b;
	vec3 nc = nebula_c;
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Forward+ hands a source_color uniform to the shader in linear; Compatibility's sky pass hands it over as written
	// (sRGB). Decode, or the web's nebula comes out pale cyan instead of deep blue.
	na = mix(pow((na + 0.055) / 1.055, vec3(2.4)), na / 12.92, lessThan(na, vec3(0.04045)));
	nb = mix(pow((nb + 0.055) / 1.055, vec3(2.4)), nb / 12.92, lessThan(nb, vec3(0.04045)));
	nc = mix(pow((nc + 0.055) / 1.055, vec3(2.4)), nc / 12.92, lessThan(nc, vec3(0.04045)));
#endif
	vec3 q = dir * 1.6;
	vec3 warp = vec3(fbm(q + 1.3), fbm(q + 7.7), fbm(q + 3.1));
	float n = fbm(q * 1.4 + warp * 1.8);
	float m = fbm(q * 2.6 - warp * 1.2 + 11.0);
	float mask = smoothstep(0.42, 0.85, n) * (0.35 + 0.65 * smoothstep(-0.6, 0.6, dot(dir, normalize(vec3(-0.6, 0.2, -0.75)))));
	vec3 neb = mix(nb, na, smoothstep(0.3, 0.7, m));
	neb = mix(neb, nc, smoothstep(0.62, 0.8, m) * smoothstep(0.55, 0.8, n));
	col += neb * mask * nebula_strength * 0.55;
	col += na * pow(max(n - 0.35, 0.0), 2.0) * nebula_strength * 0.35;

	// Stars (fewer inside dense nebula).
	vec3 stars = star_layer(dir, 260.0, 0.975, 0.16) * 0.6 + star_layer(dir, 120.0, 0.99, 0.08)
		+ star_layer(dir, 50.0, 0.996, 0.045) * 2.5;
	stars *= 1.0 + band * 1.5;
	col += stars * star_strength * (1.0 - 0.6 * mask);

	// Sun.
	if (LIGHT0_ENABLED) {
		float d = dot(dir, LIGHT0_DIRECTION);
		float disc = smoothstep(cos(sun_size), cos(sun_size * 0.8), d);
		vec3 sun = LIGHT0_COLOR * LIGHT0_ENERGY;
		col += sun * disc * 30.0;
		col += sun * (pow(max(d, 0.0), 900.0) * 3.0 + pow(max(d, 0.0), 60.0) * 0.25 + pow(max(d, 0.0), 6.0) * 0.03);
	}
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility's sky pass reads COLOR as sRGB and converts it to linear before tonemapping (Godot 4.5
	// drivers/gles3/shaders/sky.glsl); Forward+ reads it as linear. Encode it, so the web draws the desktop's sky (without
	// this every dim value was squared toward black: the nebula and the galactic band all but vanished on the web).
	col = mix(1.055 * pow(max(col, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055, col * 12.92, lessThan(col, vec3(0.0031308)));
#endif
	COLOR = col;
}
