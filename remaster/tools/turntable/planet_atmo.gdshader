// L9 atmosphere shell: Rayleigh-style rim scattering. A slightly larger sphere, additive, fresnel-weighted, strongest on
// the sunlit limb, with a warm twilight band at the terminator. Compatibility-safe (blend_add, unshaded).
shader_type spatial;
render_mode blend_add, unshaded, cull_back, depth_draw_never;

uniform vec3 tint = vec3(0.35, 0.6, 1.0);
uniform vec3 twilight = vec3(1.0, 0.45, 0.2);
uniform float power = 4.0;
uniform float strength = 1.2;
uniform vec3 sun_dir = vec3(0.5, 0.3, 0.8);

void fragment() {
	vec3 n = normalize(NORMAL);
	float f = pow(1.0 - max(dot(n, normalize(VIEW)), 0.0), power);
	vec3 wn = normalize((INV_VIEW_MATRIX * vec4(n, 0.0)).xyz);
	float ndl = dot(wn, normalize(sun_dir));
	float lit = smoothstep(-0.2, 0.55, ndl);
	float tw = exp(-pow(ndl / 0.22, 2.0));
	ALBEDO = mix(tint, twilight, tw * 0.55) * f * (lit + 0.04) * strength;
}
