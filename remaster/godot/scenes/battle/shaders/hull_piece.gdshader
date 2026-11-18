// One torn piece of a destroyed hull (DS9 "Sacrifice of Angels", 1161-1163 s: the hull stays inside the fireball, then
// breaks apart and tumbles). BattleFx._hull_pieces copies the ship's model once per piece and puts this material on
// every surface: it keeps the part of the hull between two jagged cut planes (in the model's own space), chars it,
// glows orange along the torn edges, and burns away (dissolve) at the end. No textures beyond the hull's own colour
// map: the same in Forward+ and the web build's Compatibility renderer.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_disabled, diffuse_burley;

uniform sampler2D bc_tex : source_color, hint_default_white, filter_linear_mipmap, repeat_enable;
uniform bool has_tex = false;
uniform bool use_vc = false;
uniform vec3 tint = vec3(0.6);
uniform mat4 mesh_to_root;       // this mesh's transform in the piece root's (the ship model's) space
uniform vec3 cut_n = vec3(1.0, 0.0, 0.0);
uniform float cut_lo = -1e6;     // keep cut_lo < dot(p, cut_n) + jag <= cut_hi
uniform float cut_hi = 1e6;
uniform float jag = 0.0;         // torn-edge amplitude in model units
uniform float noise_scale = 1.0; // 1 / model length: noise cells sized to the hull
uniform float burn = 0.0;        // 0 intact .. 1 charred
uniform float dissolve = 0.0;    // 0 whole .. 1 burnt away

varying vec3 v_root;

void vertex() {
	v_root = (mesh_to_root * vec4(VERTEX, 1.0)).xyz;
}

float hash3(vec3 p) {
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float vnoise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = mix(mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x), mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x), f.y);
	float b = mix(mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x), mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x), f.y);
	return mix(a, b, f.z);
}

void fragment() {
	vec3 np = v_root * noise_scale * 9.0;
	float n = vnoise3(np) * 0.65 + vnoise3(np * 2.7) * 0.35;
	float d = dot(v_root, cut_n) + (n - 0.5) * 2.0 * jag;
	if (d <= cut_lo || d > cut_hi) {
		discard;
	}
	float dn = vnoise3(np * 0.6 + 4.0);
	if (dn < dissolve * 1.15 - 0.1) {
		discard;
	}
	vec3 base = tint;
	if (has_tex) {
		base *= texture(bc_tex, UV).rgb;
	}
	if (use_vc) {
		base *= COLOR.rgb;
	}
	float char_amt = clamp(burn * (0.6 + 0.6 * n), 0.0, 0.92);
	ALBEDO = mix(base, vec3(0.05, 0.04, 0.035), char_amt);
	ROUGHNESS = 0.8;
	METALLIC = 0.2;
	// Torn edges and the burning-away front glow orange.
	float w = jag * 0.6 + 0.001;
	float edge = max(1.0 - smoothstep(0.0, w, d - cut_lo), 1.0 - smoothstep(0.0, w, cut_hi - d));
	float front = 1.0 - smoothstep(0.0, 0.12, dn - (dissolve * 1.15 - 0.1));
	float glow = max(edge * (1.0 - 0.6 * dissolve), front * step(0.001, dissolve));
	EMISSION = vec3(1.0, 0.42, 0.08) * glow * 1.6 + vec3(0.6, 0.18, 0.03) * burn * n * 0.25;
}
