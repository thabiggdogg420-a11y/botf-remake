// Torpedo head (M1 battleplay; Simon, 9 Nov 2026: "the torpedoes still never got fixed"). A camera-facing additive
// sprite: a white-hot core that fills about a third of the quad, a glow in the weapon colour, and a four-point flare
// whose long horizontal rays shimmer, as photon and quantum torpedoes read in the shows. The R3.5 head used flare.gdshader,
// whose core exp(-30 r^2) is about a fifth of the quad, so at the tactical camera's floor size it drew a 4 px dot behind
// a 60 px trail: every torpedo read as a thin bolt. Standard render modes only: same pass in Forward+ and Compatibility.
// Plain uniforms on a material per effect node (BattleFx._fxp), not instance uniforms: see there.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec4 torp_color : source_color = vec4(1.0, 0.25, 0.08, 1.0);
uniform float torp_intensity = 1.0;
uniform float torp_phase = 0.0;   // per torpedo, drives the flicker and the flare spin
uniform float torp_core = 0.3;    // core radius as a share of the half-quad

void vertex() {
	// Billboard, keeping the node's scale.
	vec3 scale = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), length(MODEL_MATRIX[2].xyz));
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0] * scale.x, INV_VIEW_MATRIX[1] * scale.y,
		INV_VIEW_MATRIX[2] * scale.z, MODEL_MATRIX[3]);
	MODELVIEW_NORMAL_MATRIX = mat3(MODELVIEW_MATRIX);
}

void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	float r = length(p);
	float flicker = 0.85 + 0.15 * sin(TIME * 37.0 + torp_phase * 5.3) * sin(TIME * 23.0 + torp_phase);
	// White-hot core with a soft rim, then the coloured glow around it.
	float core = 1.0 - smoothstep(torp_core * 0.55, torp_core, r);
	float glow = exp(-r * 3.4) * (1.0 - smoothstep(0.75, 1.0, r));
	// Four-point flare: the horizontal pair long and bright, the vertical pair shorter, slowly turning.
	float a = TIME * 0.6 + torp_phase;
	vec2 q = mat2(vec2(cos(a), -sin(a)), vec2(sin(a), cos(a))) * p;
	float rays = exp(-abs(q.y) * 38.0) * exp(-abs(q.x) * 2.4) + 0.55 * exp(-abs(q.x) * 38.0) * exp(-abs(q.y) * 3.6);
	rays *= (1.0 - smoothstep(0.8, 1.0, r)) * flicker;
	vec3 tint = torp_color.rgb;
	vec3 col = tint * (glow * 2.2 + rays * 2.0) + mix(tint, vec3(1.0, 0.97, 0.92), 0.75) * core * 3.0;
	float near_fade = smoothstep(1.0, 8.0, -VERTEX.z); // a torpedo passing the lens does not blind it
	ALBEDO = col * torp_intensity * flicker * near_fade;
	ALPHA = clamp((glow + core + rays) * torp_intensity * near_fade, 0.0, 1.0);
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility (the web build) adds this glow with no HDR lift, so dim parts all but vanish on black: lift
	// what is added (ALBEDO x ALPHA under blend_add) with an sRGB-like curve, as in shield.gdshader (M1 battleplay).
	ALBEDO = pow(ALBEDO * ALPHA, vec3(1.0 / 2.2));
	ALPHA = 1.0;
#endif
}
