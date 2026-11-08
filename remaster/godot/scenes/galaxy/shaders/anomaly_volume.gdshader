// Desktop (Forward+) anomaly: a ray-marched cloud inside a unit sphere. Same parameters as nebula.gdshader:
// tint, seed, intensity, swirl (rift, wormhole, black hole) and crackle (ion storm lightning). `disc` flattens the
// density into an accretion disc (black hole, wormhole) and `core` adds a black event horizon.
// Web uses anomaly_billboard.gdshader (the declared fallback: the Compatibility budget allows 4 loop iterations).
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_front, fog_disabled, shadows_disabled;
#include "res://scenes/galaxy/shaders/galaxy_noise.gdshaderinc"

uniform vec4 tint : source_color = vec4(0.55, 0.35, 0.9, 1.0);
uniform float seed = 0.0;
uniform float intensity = 0.8;
uniform float swirl = 0.0;
uniform float crackle = 0.0;
uniform float disc = 0.0;
uniform float core = 0.0;
uniform float opacity = 1.0;

varying vec3 v_cam;
varying vec3 v_local;

void vertex() {
	v_local = VERTEX;
	v_cam = (inverse(MODEL_MATRIX) * vec4(CAMERA_POSITION_WORLD, 1.0)).xyz;
}

float density(vec3 p, out float bolt) {
	float r = length(p);
	vec3 q = p;
	if (swirl > 0.0) {
		float ang = swirl * (1.0 - r) * 3.0 + TIME * swirl * 0.5;
		float cs = cos(ang);
		float sn = sin(ang);
		q.xz = mat2(vec2(cs, sn), vec2(-sn, cs)) * q.xz;
	}
	vec3 o = vec3(fract(seed * 0.137), fract(seed * 0.291), fract(seed * 0.373)) * 40.0;
	float n = g_fbm3(q * 2.2 + o + vec3(TIME * 0.03, 0.0, -TIME * 0.02));
	float shell = 1.0 - smoothstep(0.35, 1.0, r);
	float flat_ = mix(1.0, exp(-p.y * p.y * 60.0) * smoothstep(0.12, 0.3, r), disc);
	float d = smoothstep(0.38, 0.85, n + 0.15) * shell * flat_;
	bolt = 0.0;
	if (crackle > 0.0) {
		float ridge = abs(g_fbm3(q * 3.0 + o + vec3(TIME * 0.35, 0.0, -TIME * 0.2)) - 0.5);
		bolt = (1.0 - smoothstep(0.0, 0.025, ridge)) * crackle * shell * (0.55 + 0.45 * sin(TIME * 9.0 + n * 12.0));
	}
	return d;
}

void fragment() {
	vec3 ro = v_cam;
	vec3 rd = normalize(v_local - ro);
	float b = dot(ro, rd);
	float c = dot(ro, ro) - 1.0;
	float h = b * b - c;
	if (h < 0.0) {
		discard;
	}
	h = sqrt(h);
	float t0 = max(-b - h, 0.0);
	float t1 = -b + h;
	const int STEPS = 22;
	float dt = (t1 - t0) / float(STEPS);
	float jitter = g_hash12(FRAGCOORD.xy);
	float t = t0 + dt * jitter;
	vec3 acc = vec3(0.0);
	float alpha = 0.0;
	for (int i = 0; i < STEPS; i++) {
		vec3 p = ro + rd * t;
		if (core > 0.0 && length(p) < 0.11 * core) {
			alpha = 1.0;
			break;
		}
		float bolt;
		float d = density(p, bolt) * intensity;
		float r = length(p);
		vec3 col = mix(tint.rgb * 0.55, tint.rgb * 1.5 + 0.12, smoothstep(0.9, 0.2, r));
		col *= 1.0 + 2.5 * disc * smoothstep(0.5, 0.15, r);
		float da = clamp(d * dt * 3.2, 0.0, 1.0);
		acc += (1.0 - alpha) * (col * da + vec3(0.8, 0.9, 1.0) * bolt * dt * 6.0);
		alpha += (1.0 - alpha) * clamp(da + bolt * dt * 2.0, 0.0, 1.0);
		if (alpha > 0.97) {
			break;
		}
		t += dt;
	}
	alpha = clamp(alpha * opacity, 0.0, 1.0);
	ALBEDO = acc / max(alpha, 0.001) * opacity;
	ALPHA = alpha;
}
