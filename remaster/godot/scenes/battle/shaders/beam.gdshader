// Energy beam drawn on a cylinder (axis = local Y): a white-hot core inside a faction-coloured halo, with energy
// pulses running from emitter to target. HDR output feeds the glow/bloom pass.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

instance uniform vec4 beam_color : source_color = vec4(1.0, 0.45, 0.05, 1.0);
instance uniform float beam_intensity = 1.0;
instance uniform float beam_phase = 0.0;
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
	vec3 col = beam_color.rgb * halo * 1.4 * pulse + vec3(1.0, 0.97, 0.9) * core * 3.0;
	float dim = mix(1.0, 0.2, smoothstep(0.75, 1.0, end_on));
	ALBEDO = col * beam_intensity * flicker * dim;
	ALPHA = clamp(halo * beam_intensity, 0.0, 1.0);
}
