shader_type canvas_item;
// Lit planet sphere for the UI (docs/UI-DESIGN-SYSTEM.md §5, ART-BIBLE §8). Compatibility-safe: no screen or depth
// textures, at most 4 noise octaves. With use_tex the equirectangular albedo (lane L7) is wrapped on the sphere;
// otherwise a procedural surface by class style. Light comes from the upper left, as the battle sun (ART-BIBLE §4).

uniform vec4 c_deep : source_color = vec4(0.2, 0.35, 0.6, 1.0);
uniform vec4 c_land : source_color = vec4(0.3, 0.6, 0.35, 1.0);
uniform vec4 c_high : source_color = vec4(0.85, 0.85, 0.8, 1.0);
uniform vec4 c_rim : source_color = vec4(0.5, 0.7, 1.0, 1.0);
uniform int style = 0;          // 0 terran, 1 ocean, 2 gas, 3 barren, 4 ice, 5 lava, 6 desert, 7 jungle
uniform float seed = 1.0;
uniform float spin = 0.0;       // longitude offset, turns
uniform float clouds = 0.5;
uniform float atmosphere = 0.6;
uniform bool use_tex = false;
uniform sampler2D albedo : source_color, filter_linear_mipmap, repeat_enable;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3) * seed);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int k = 0; k < 4; k++) {
		v += a * noise(p);
		p = p * 2.03 + vec3(1.7, 9.2, 3.1);
		a *= 0.5;
	}
	return v;
}

void fragment() {
	vec2 q = UV * 2.0 - 1.0;
	float r = 0.82;
	float d = length(q) / r;
	vec3 L = normalize(vec3(-0.55, -0.45, 0.7));
	if (d > 1.0) {
		// atmosphere halo outside the disc
		float halo = smoothstep(1.22, 1.0, d) * atmosphere;
		float lit = clamp(dot(normalize(vec3(q / r, 0.0)), L) * 0.8 + 0.35, 0.0, 1.0);
		COLOR = vec4(c_rim.rgb, halo * 0.55 * lit);
	} else {
		vec2 s = q / r;
		vec3 n = vec3(s, sqrt(max(0.0, 1.0 - dot(s, s))));
		// rotate around the Y axis for spin
		float ang = spin * 6.2831853;
		vec3 w = vec3(n.x * cos(ang) + n.z * sin(ang), n.y, -n.x * sin(ang) + n.z * cos(ang));
		float lon = atan(w.x, w.z) / 6.2831853 + 0.5;
		float lat = asin(clamp(w.y, -1.0, 1.0)) / 3.14159265 + 0.5;
		vec3 col;
		if (use_tex) {
			col = texture(albedo, vec2(lon, lat)).rgb;
		} else {
			vec3 p = w * 2.2 + vec3(seed * 3.1, seed * 1.3, seed * 2.7);
			float h = fbm(p);
			if (style == 2) {
				float band = sin(w.y * 14.0 + fbm(p * 1.5) * 4.0);
				col = mix(c_deep.rgb, c_land.rgb, band * 0.5 + 0.5);
				col = mix(col, c_high.rgb, smoothstep(0.62, 0.8, fbm(p * 2.0 + band)));
			} else if (style == 3) {
				col = mix(c_deep.rgb, c_land.rgb, h);
				float cr = fbm(p * 3.0);
				col *= 0.75 + 0.5 * smoothstep(0.45, 0.55, cr);
			} else if (style == 5) {
				col = mix(c_deep.rgb, c_land.rgb, smoothstep(0.35, 0.65, h));
				float crack = 1.0 - smoothstep(0.0, 0.05, abs(fbm(p * 2.5) - 0.5));
				col = mix(col, c_high.rgb, crack);
			} else {
				float sea = style == 1 ? 0.62 : (style == 6 ? 0.25 : (style == 4 ? 0.3 : 0.5));
				col = h < sea ? mix(c_deep.rgb * 0.8, c_deep.rgb, h / sea) : mix(c_land.rgb, c_land.rgb * 0.7, (h - sea) / (1.0 - sea));
				float ice = smoothstep(style == 4 ? 0.35 : 0.78, style == 4 ? 0.55 : 0.92, abs(w.y) + h * 0.15);
				col = mix(col, c_high.rgb, ice);
				float cl = smoothstep(0.55, 0.75, fbm(p * 1.6 + vec3(0.0, 0.0, spin * 2.0)));
				col = mix(col, vec3(0.95), cl * clouds);
			}
		}
		float diff = clamp(dot(n, L), 0.0, 1.0);
		float term = smoothstep(-0.05, 0.25, dot(n, L));
		vec3 lit = col * (0.06 + 1.05 * diff) * (0.25 + 0.75 * term);
		float fres = pow(1.0 - n.z, 3.0);
		lit += c_rim.rgb * fres * atmosphere * (0.25 + 0.75 * term);
		float edge = smoothstep(1.0, 0.985, d);
		COLOR = vec4(lit, edge);
	}
}
