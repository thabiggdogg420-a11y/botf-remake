// Shared value noise for the 3D galaxy shaders. Sin-free hash (stable on GL/WebGL, see nebula.gdshader notes),
// quintic fade, rotated octaves. Loops are constant and capped at 4 (art bible 12.2).
float g_hash13(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}
float g_hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}
float g_noise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	return mix(mix(mix(g_hash13(i), g_hash13(i + vec3(1, 0, 0)), u.x), mix(g_hash13(i + vec3(0, 1, 0)), g_hash13(i + vec3(1, 1, 0)), u.x), u.y),
		mix(mix(g_hash13(i + vec3(0, 0, 1)), g_hash13(i + vec3(1, 0, 1)), u.x), mix(g_hash13(i + vec3(0, 1, 1)), g_hash13(i + vec3(1, 1, 1)), u.x), u.y), u.z);
}
float g_noise2(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	return mix(mix(g_hash12(i), g_hash12(i + vec2(1.0, 0.0)), u.x), mix(g_hash12(i + vec2(0.0, 1.0)), g_hash12(i + vec2(1.0, 1.0)), u.x), u.y);
}
float g_fbm3(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * g_noise3(p);
		p = vec3(p.y * 0.8 + p.z * 0.6, p.z * 0.8 - p.y * 0.6, p.x) * 2.03 + vec3(3.1, 1.3, 7.7);
		a *= 0.5;
	}
	return v;
}
float g_fbm2(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		v += a * g_noise2(p);
		p = mat2(vec2(0.8, 0.6), vec2(-0.6, 0.8)) * p * 2.03 + vec2(3.1, 1.3);
		a *= 0.5;
	}
	return v;
}
