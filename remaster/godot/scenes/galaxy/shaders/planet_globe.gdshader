// A planet at system zoom, lit by its own star (star_pos), not by scene lights, so Forward+ and Compatibility match.
// Textured from the lane L7 planet sets when present (equirectangular albedo, optional clouds and night lights),
// procedural otherwise (rocky worlds from 3D noise with sea, land, highland and ice; gas giants as bands and storms).
// Atmosphere rim: fresnel power 3, tinted per class (art bible section 8).
shader_type spatial;
render_mode unshaded, fog_disabled, shadows_disabled;
#include "res://scenes/galaxy/shaders/galaxy_noise.gdshaderinc"

uniform int kind = 0; // 0 rocky, 1 gas giant, 2 lava (class Y), 3 airless
uniform vec3 c_low : source_color = vec3(0.1, 0.25, 0.55);
uniform vec3 c_mid : source_color = vec3(0.3, 0.5, 0.25);
uniform vec3 c_high : source_color = vec3(0.6, 0.55, 0.45);
uniform vec3 atmosphere : source_color = vec3(0.45, 0.65, 1.0);
uniform float atmo_strength = 1.0;
uniform float sea_level = 0.48;
uniform float ice = 0.8;
uniform float cloud_amount = 0.5;
uniform float seed = 0.0;
uniform float colonised = 0.0;
uniform vec3 star_pos = vec3(0.0);
uniform vec3 star_color : source_color = vec3(1.0, 0.95, 0.88);
uniform bool has_albedo = false;
uniform bool has_clouds = false;
uniform bool has_night = false;
uniform sampler2D albedo_tex : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D clouds_tex : filter_linear_mipmap, repeat_enable;
uniform sampler2D night_tex : source_color, filter_linear_mipmap, repeat_enable;

varying vec3 v_local;
varying vec3 v_world;

void vertex() {
	v_local = normalize(VERTEX);
	v_world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

// Equirectangular lookup without the seam line: take derivatives from whichever wrap of u is continuous here.
vec4 equirect(sampler2D tex, vec3 n, float spin) {
	float u = atan(n.z, n.x) / 6.2831853 + 0.5 + spin;
	float v = acos(clamp(n.y, -1.0, 1.0)) / 3.1415927;
	vec2 uv = vec2(u, v);
	vec2 uv2 = vec2(fract(u + 0.5) - 0.5, v);
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);
	vec2 dx2 = dFdx(uv2);
	vec2 dy2 = dFdy(uv2);
	if (abs(dx2.x) + abs(dy2.x) < abs(dx.x) + abs(dy.x)) {
		dx = dx2;
		dy = dy2;
	}
	return textureGrad(tex, uv, dx, dy);
}

void fragment() {
	vec3 n = v_local;
	vec3 o = vec3(fract(seed * 0.173), fract(seed * 0.311), fract(seed * 0.419)) * 50.0;
	vec3 albedo;
	float spec_mask = 0.0;
	vec3 emit = vec3(0.0);
	float hf = 0.0;   // the height field's fbm, reused by the city lights
	if (has_albedo) {
		albedo = equirect(albedo_tex, n, 0.0).rgb;
	} else if (kind == 1) {
		float warp = g_fbm3(n * 3.0 + o);
		float lat = n.y * 9.0 + warp * 1.8;
		float bands = 0.5 + 0.35 * sin(lat * 2.2) + 0.3 * sin(lat * 5.3 + 1.0);
		albedo = mix(c_low, c_mid, smoothstep(0.2, 0.8, bands));
		albedo = mix(albedo, c_high, smoothstep(0.7, 0.95, g_fbm3(n * vec3(3.0, 14.0, 3.0) + o + warp)));
	} else {
		hf = g_fbm3(n * 2.6 + o);
		float h = hf + 0.12 * g_noise3(n * 18.0 + o);
		if (kind == 0) {
			float land = smoothstep(sea_level - 0.01, sea_level + 0.02, h);
			vec3 sea = mix(c_low * 0.6, c_low, smoothstep(sea_level - 0.2, sea_level, h));
			vec3 ground = mix(c_mid, c_high, smoothstep(sea_level + 0.05, sea_level + 0.3, h));
			albedo = mix(sea, ground, land);
			spec_mask = 1.0 - land;
		} else {
			albedo = mix(c_low, c_mid, smoothstep(0.3, 0.6, h));
			albedo = mix(albedo, c_high, smoothstep(0.62, 0.8, h));
			float crater = smoothstep(0.03, 0.0, abs(g_noise3(n * 11.0 + o) - 0.5)) * 0.25;
			albedo *= 1.0 - crater;
			if (kind == 2) {
				float crack = 1.0 - smoothstep(0.0, 0.035, abs(g_fbm3(n * 5.0 + o) - 0.5));
				emit = vec3(1.0, 0.35, 0.08) * crack * 1.6;
			}
		}
		float pole = smoothstep(ice, ice + 0.06, abs(n.y) + 0.06 * g_noise3(n * 9.0 + o));
		albedo = mix(albedo, vec3(0.92, 0.95, 1.0), pole);
	}
	vec3 nw = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	vec3 l = normalize(star_pos - v_world);
	float nl = dot(nw, l);
	float day = clamp((nl + 0.06) / 1.06, 0.0, 1.0);
	vec3 col = albedo * day * star_color * 1.35 + albedo * 0.012;
	// Clouds
	float cl = 0.0;
	if (has_clouds) {
		cl = equirect(clouds_tex, n, TIME * 0.004).r;
	} else if (kind == 0 && cloud_amount > 0.0) {
		cl = smoothstep(0.55 - cloud_amount * 0.2, 0.85, g_fbm3(n * 4.0 + o + vec3(TIME * 0.01, 0.0, 0.0))) * cloud_amount;
	}
	col = mix(col, vec3(1.0) * day * star_color * 1.2 + 0.01, cl * 0.85);
	// Specular glint on water.
	vec3 vw = normalize((INV_VIEW_MATRIX * vec4(VIEW, 0.0)).xyz);
	vec3 hv = normalize(l + vw);
	col += star_color * pow(max(dot(nw, hv), 0.0), 60.0) * spec_mask * 0.6 * day * (1.0 - cl);
	// Night side: city lights on colonised worlds, lava glow on class Y.
	float night = 1.0 - smoothstep(-0.12, 0.08, nl);
	if (has_night && colonised > 0.0) {
		col += equirect(night_tex, n, 0.0).rgb * night * colonised * 1.4 * (1.0 - cl * 0.7);
	} else if (colonised > 0.0 && kind == 0) {
		float city = smoothstep(0.72, 0.9, g_noise3(n * 40.0 + o)) * step(sea_level + 0.02, hf);
		col += vec3(1.0, 0.75, 0.4) * city * night * colonised * 0.9;
	}
	col += emit * (0.35 + 0.65 * night);
	// Atmosphere rim.
	float fres = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 3.0);
	col += atmosphere * fres * smoothstep(-0.25, 0.45, nl) * atmo_strength * 0.9;
	ALBEDO = col;
}
