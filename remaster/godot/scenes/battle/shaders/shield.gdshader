// Deflector shield bubble: invisible until struck, then a bright flash at the impact point with a hexagonal
// lattice and a ripple ring that runs across the shell. Up to 6 simultaneous impacts.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

// xyz = impact direction in the bubble's local space (unit), w = age 0..1 (<0 = inactive).
uniform vec4 hits[6];
uniform vec4 hit_colors[6];
// Tactical rules 4: xyz = the struck facing's axis in local space (fore +x, aft -x, port -z, starboard +z, dorsal +y,
// ventral -y; zero = no facing), w = that facing's remaining strength 0..1. The whole facing segment lights up with a
// bright rim at its border, so the player reads which of the six shields took the hit and how much it has left.
uniform vec4 hit_facing[6];
uniform float energy = 0.9;

varying vec3 v_local;

void vertex() {
	v_local = normalize(VERTEX);
}

float hex_edge(vec2 p) {
	p *= vec2(1.0, 1.1547);
	vec2 r = vec2(1.0, 1.7320508);
	vec2 h = r * 0.5;
	vec2 a = mod(p, r) - h;
	vec2 b = mod(p - h, r) - h;
	vec2 g = dot(a, a) < dot(b, b) ? a : b;
	vec2 q = abs(g);
	float d = max(dot(q, normalize(vec2(1.0, 1.7320508))), q.x);
	return smoothstep(0.38, 0.5, d);
}

void fragment() {
	vec3 n = normalize(v_local);
	float fres = pow(1.0 - abs(dot(normalize(NORMAL), VIEW)), 2.5);
	vec2 uv = vec2(atan(n.z, n.x) * 3.0, asin(clamp(n.y, -1.0, 1.0)) * 3.0) * 1.6;
	float hex = hex_edge(uv);
	vec3 col = vec3(0.0);
	for (int i = 0; i < 6; i++) {
		float age = hits[i].w;
		if (age < 0.0 || age > 1.0) continue;
		float d = acos(clamp(dot(n, normalize(hits[i].xyz)), -1.0, 1.0));
		float fade = pow(1.0 - age, 1.6);
		float flash = exp(-d * d / (0.03 + age * 0.12)) * (0.6 + 1.5 * exp(-age * 12.0));
		float ring_r = 0.15 + age * 1.3;
		float ring = exp(-pow((d - ring_r) / 0.09, 2.0)) * 0.7;
		float lattice = hex * exp(-d * d / (0.08 + age * 0.35)) * 0.8;
		float glow = (flash + ring + lattice) * fade + fres * 0.25 * fade;
		if (dot(hit_facing[i].xyz, hit_facing[i].xyz) > 0.5) {
			float c = dot(n, normalize(hit_facing[i].xyz));
			float sector = smoothstep(0.55, 0.68, c);                    // about 50 degrees round the facing axis
			float rim = exp(-pow((c - 0.62) / 0.035, 2.0));              // the facing's border
			float weak = 1.0 - clamp(hit_facing[i].w, 0.0, 1.0);
			float seg = (sector * (0.18 + 0.55 * hex) + rim * 0.9) * pow(1.0 - age, 1.2) * (0.7 + 0.6 * weak);
			glow += seg;
		}
		col += hit_colors[i].rgb * glow;
	}
	// R3.5: a close camera sat inside a flashing shell and the frame went white; fade it near the lens and cap it.
	ALBEDO = min(col * energy * 0.35, vec3(1.2)) * smoothstep(2.0, 18.0, -VERTEX.z);
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility (the web build) adds this low-energy glow with no HDR lift or glow pass: on black the bubble's peak
	// was 0.013 against Forward+'s 0.214 (M1 battleplay probe) and it vanished in battle. An sRGB-like curve restores it.
	ALBEDO = pow(ALBEDO, vec3(1.0 / 2.2));
#endif
	ALPHA = 1.0;
}
