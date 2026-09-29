// Banded gas giant with storms and a scattering rim; lit by the scene's sun.
shader_type spatial;
render_mode diffuse_lambert_wrap, specular_disabled;

uniform vec3 band_a : source_color = vec3(0.78, 0.6, 0.42);
uniform vec3 band_b : source_color = vec3(0.45, 0.3, 0.22);
uniform vec3 band_c : source_color = vec3(0.9, 0.85, 0.75);
uniform vec3 atmosphere : source_color = vec3(0.45, 0.65, 1.0);

varying vec3 v_local;

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
	for (int i = 0; i < 6; i++) { v += a * n3(p); p *= 2.07; a *= 0.5; }
	return v;
}

void vertex() {
	v_local = normalize(VERTEX);
}

void fragment() {
	vec3 p = v_local;
	float warp = fbm(p * 4.0);
	float lat = p.y * 9.0 + warp * 1.8;
	float bands = 0.5 + 0.5 * sin(lat * 2.2) * 0.7 + 0.3 * sin(lat * 5.3 + 1.0);
	vec3 col = mix(band_b, band_a, smoothstep(0.2, 0.8, bands));
	col = mix(col, band_c, smoothstep(0.75, 0.95, fbm(p * vec3(3.0, 14.0, 3.0) + warp)));
	// A great storm.
	vec3 storm_c = normalize(vec3(0.6, -0.25, 0.75));
	float sd = length(p - storm_c);
	col = mix(col, vec3(0.75, 0.35, 0.2), smoothstep(0.16, 0.08, sd + (warp - 0.5) * 0.08));
	ALBEDO = col;
	ROUGHNESS = 0.9;
	float fres = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 3.0);
	EMISSION = atmosphere * fres * 0.15;
}

void light() {
	float nl = dot(NORMAL, LIGHT);
	float wrap = clamp((nl + 0.15) / 1.15, 0.0, 1.0);
	float fres = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 4.0);
	DIFFUSE_LIGHT += ALBEDO * wrap * ATTENUATION * LIGHT_COLOR / PI;
	DIFFUSE_LIGHT += atmosphere * fres * smoothstep(-0.2, 0.4, nl) * LIGHT_COLOR * ATTENUATION * 0.6;
}
