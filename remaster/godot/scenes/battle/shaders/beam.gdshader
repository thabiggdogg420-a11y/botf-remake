// Energy beam drawn on a cylinder (axis = local Y): a white-hot core inside a faction-coloured halo, with energy
// pulses running from emitter to target. HDR output feeds the glow/bloom pass.
// Plain uniforms on a material per effect node (BattleFx._fxp), not instance uniforms: see there.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec4 beam_color : source_color = vec4(1.0, 0.45, 0.05, 1.0);
uniform float beam_intensity = 1.0;
uniform float beam_phase = 0.0;
uniform float beam_halo = 0.0; // 1 = the wide outer glow cylinder: soft colour only, no white core
uniform float core_power = 8.0;

varying float end_on;

void vertex() {
	// How directly the camera looks down the beam: stacked additive layers blow out, so dim end-on views.
	end_on = abs(normalize((MODELVIEW_MATRIX * vec4(0.0, 1.0, 0.0, 0.0)).xyz).z);
}

void fragment() {
	float facing = abs(dot(normalize(NORMAL), VIEW));
	float core = pow(facing, core_power);
	float halo = pow(facing, 1.4);
	float along = UV.y;
	float pulse = 0.75 + 0.25 * sin(along * 60.0 - TIME * 40.0 + beam_phase * 7.0);
	float flicker = 0.9 + 0.1 * sin(TIME * 93.0 + beam_phase * 13.0);
	vec3 col = beam_color.rgb * halo * 1.4 * pulse + vec3(1.0, 0.97, 0.9) * core * 1.6;
	if (beam_halo > 0.5) {
		halo = pow(facing, 3.0);
		col = beam_color.rgb * halo * 0.9 * pulse;
	}
	float dim = mix(1.0, 0.12, smoothstep(0.6, 1.0, end_on));
	// Fade the part of a beam that passes right by the lens: at true scale a close camera sat inside the glow.
	dim *= smoothstep(4.0, 30.0, -VERTEX.z);
	ALBEDO = col * beam_intensity * flicker * dim;
	ALPHA = clamp(halo * beam_intensity, 0.0, 1.0);
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility (the web build) adds this glow with no HDR lift, so dim parts all but vanish on black: lift
	// what is added (ALBEDO x ALPHA under blend_add) with an sRGB-like curve, as in shield.gdshader (M1 battleplay).
	ALBEDO = pow(ALBEDO * ALPHA, vec3(1.0 / 2.2));
	ALPHA = 1.0;
#endif
}
