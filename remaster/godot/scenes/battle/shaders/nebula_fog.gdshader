// Volumetric nebula for a FogVolume (Forward+ volumetric fog): billowing fbm density with a coloured glow.
shader_type fog;

uniform vec3 albedo : source_color = vec3(0.35, 0.25, 0.55);
uniform vec3 emission : source_color = vec3(0.12, 0.05, 0.2);
uniform vec3 emission_b : source_color = vec3(0.02, 0.1, 0.22);
uniform float density = 0.02;
uniform float scale = 0.012;

float h13(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}
float n3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(h13(i), h13(i + vec3(1, 0, 0)), f.x), mix(h13(i + vec3(0, 1, 0)), h13(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(h13(i + vec3(0, 0, 1)), h13(i + vec3(1, 0, 1)), f.x), mix(h13(i + vec3(0, 1, 1)), h13(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) { v += a * n3(p); p *= 2.1; a *= 0.5; }
	return v;
}

void fog() {
	vec3 p = WORLD_POSITION * scale + vec3(0.0, 0.0, TIME * 0.004);
	float n = fbm(p + fbm(p * 1.7) * 1.5);
	float edge = 1.0 - smoothstep(0.55, 1.0, length(UVW - 0.5) * 2.0);
	float d = smoothstep(0.42, 0.78, n) * edge;
	DENSITY = d * density;
	ALBEDO = albedo;
	EMISSION = mix(emission, emission_b, smoothstep(0.4, 0.7, fbm(p * 2.3 + 5.0))) * d;
}
