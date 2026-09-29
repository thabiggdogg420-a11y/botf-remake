// Camera-facing ribbon (built each frame by BattleFx): vertex colour carries the fade along the trail, UV.y runs
// across the strip for a soft edge.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

void fragment() {
	float across = 1.0 - abs(UV.y * 2.0 - 1.0);
	float soft = pow(across, 1.5);
	ALBEDO = COLOR.rgb * soft * 3.0 + vec3(1.0) * pow(across, 12.0) * COLOR.a * 1.5;
	ALPHA = clamp(COLOR.a * soft, 0.0, 1.0);
}
