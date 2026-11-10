// Compatibility fallback for the battle's volumetric nebula (nebula_fog.gdshader on two FogVolumes, Forward+ only).
// A stack of camera-facing slices from near the eye out to the fog length (Forward+ volumetric_fog_length), each a
// full-view quad placed in view space by the vertex shader; the fragment samples both fog ellipsoids at its world point
// with the same shape (ellipsoid edge falloff, warped fbm density) and blends in the light that one slab of that thickness
// scatters toward the eye, as Godot's froxel fog does (see compat_nebula.gd). Depth-tested against the scene, so ships in
// front of the fog stay clear and fog in front of a ship veils it. No screen or depth texture reads: WebGL2-safe.
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled, shadows_disabled;

uniform sampler3D noise_tex : filter_linear, repeat_enable;
uniform float lattice = 32.0;          // noise_tex size: the value-noise lattice period, in cells
uniform float scale = 0.012;           // nebula_fog.gdshader `scale`
uniform vec3 c0 = vec3(0.0);
uniform vec3 h0 = vec3(1.0);
uniform vec3 albedo0 = vec3(0.1, 0.05, 0.26);
uniform float dens0 = 0.02;
uniform vec3 c1 = vec3(0.0);
uniform vec3 h1 = vec3(1.0);
uniform vec3 albedo1 = vec3(0.05, 0.1, 0.32);
uniform float dens1 = 0.012;
// The scene's directional lights: direction toward the light and colour x energy x PI (linear). Unused ones are black.
uniform vec3 l0_dir = vec3(0.0, 1.0, 0.0);
uniform vec3 l0_col = vec3(0.0);
uniform vec3 l1_dir = vec3(0.0, 1.0, 0.0);
uniform vec3 l1_col = vec3(0.0);
uniform vec3 l2_dir = vec3(0.0, 1.0, 0.0);
uniform vec3 l2_col = vec3(0.0);
uniform float anisotropy = 0.5;        // Environment.volumetric_fog_anisotropy
uniform float fog_length = 480.0;

varying vec3 v_world;
varying float v_thick;
varying float v_d;

void vertex() {
	// UV2 = (slice distance, slice thickness); VERTEX.xy = the quad corner in NDC, a little past the edge.
	float d = UV2.x;
	vec3 vp = vec3(VERTEX.x * d / PROJECTION_MATRIX[0][0], VERTEX.y * d / PROJECTION_MATRIX[1][1], -d);
	v_world = (INV_VIEW_MATRIX * vec4(vp, 1.0)).xyz;
	v_thick = UV2.y;
	v_d = d;
	POSITION = PROJECTION_MATRIX * vec4(vp, 1.0);
}

// nebula_fog.gdshader's n3(): value noise on the integer lattice with smoothstep interpolation. The fraction is remapped
// before the fetch, so the hardware's trilinear blend between lattice texels is the cubic blend.
float n3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return texture(noise_tex, (i + f + 0.5) / lattice).r;
}

float fbm(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) { v += a * n3(p); p *= 2.1; a *= 0.5; }
	return v;
}

// Henyey-Greenstein, as volumetric_fog_process.glsl.
float hg(float cos_t) {
	float g = anisotropy;
	return 0.0795774715459 * (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * cos_t, 1e-4), 1.5);
}

void fragment() {
	vec3 w = v_world;
	float r0 = length((w - c0) / h0);
	float r1 = length((w - c1) / h1);
	if (r0 >= 1.0 && r1 >= 1.0) {
		discard;
	}
	vec3 p = w * scale + vec3(0.0, 0.0, TIME * 0.004);
	float shape = smoothstep(0.42, 0.78, fbm(p + fbm(p * 1.7) * 1.5));
	float d0 = r0 < 1.0 ? shape * (1.0 - smoothstep(0.55, 1.0, r0)) : 0.0;
	float d1 = r1 < 1.0 ? shape * (1.0 - smoothstep(0.55, 1.0, r1)) : 0.0;
	float sigma = d0 * dens0 + d1 * dens1;
	if (sigma <= 1e-5) {
		discard;
	}
	vec3 ray = normalize(w - INV_VIEW_MATRIX[3].xyz);
	vec3 light = l0_col * hg(dot(ray, l0_dir)) + l1_col * hg(dot(ray, l1_dir)) + l2_col * hg(dot(ray, l2_dir));
	vec3 alb = (albedo0 * d0 * dens0 + albedo1 * d1 * dens1) / sigma;
	float fade = 1.0 - smoothstep(fog_length * 0.8, fog_length, v_d);
	ALBEDO = alb * light;
	ALPHA = (1.0 - exp(-sigma * v_thick)) * fade;
}
