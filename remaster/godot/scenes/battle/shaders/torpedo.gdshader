// Torpedo head (M1 weaponfx; docs/WEAPON-FX.md; Simon, 9 Nov 2026: "torpedoes still havent been done properly"). A
// camera-facing additive sprite, `aspect` times wider than tall so a screen-horizontal lens streak fits: a white-hot
// core, a glow in the weapon colour pulsing at about 6 Hz, a ring of thin twinkling spikes of uneven length, and the
// streak, as photon torpedoes read in the DS9 battles. Styles: 0 photon, 1 quantum (blue-white, ring halo, 6 sharper
// spikes), 2 plasma (diffuse churning ball, 4 faint spikes, no streak).
// Standard render modes only: the same pass in Forward+ and Compatibility.
// Plain uniforms on a material per effect node (BattleFx._fxp), not instance uniforms: see there.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled;

uniform vec4 torp_color : source_color = vec4(1.0, 0.29, 0.078, 1.0);
uniform vec4 torp_core : source_color = vec4(1.0, 0.886, 0.722, 1.0);
uniform float torp_intensity = 1.0;
uniform float torp_phase = 0.0;   // per torpedo: pulse, twinkle and spin
uniform int torp_style = 0;
uniform float aspect = 3.0;       // node scale x / scale y (BattleFx sets both)

float hash1(float n) { return fract(sin(n) * 43758.5453); }
float noise1(float x) {
	float i = floor(x);
	float f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(hash1(i), hash1(i + 1.0), f);
}

void vertex() {
	// Billboard, keeping the node's scale.
	vec3 scale = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), length(MODEL_MATRIX[2].xyz));
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0] * scale.x, INV_VIEW_MATRIX[1] * scale.y,
		INV_VIEW_MATRIX[2] * scale.z, MODEL_MATRIX[3]);
	MODELVIEW_NORMAL_MATRIX = mat3(MODELVIEW_MATRIX);
}

void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	p.x *= aspect;                 // round units: |p| = 1 at the top edge
	float r = length(p);
	float t = TIME;
	float ph = torp_phase;
	float pulse = 1.0 + 0.2 * sin(t * 37.7 + ph * 5.0);
	float rim = 1.0 - smoothstep(0.8, 1.0, r);
	float core_r = torp_style == 1 ? 0.16 : (torp_style == 2 ? 0.17 : 0.13);
	float core = 1.0 - smoothstep(core_r * 0.55, core_r, r);
	// Two-tone glow: a warm inner glow round the core, then the weapon colour further out.
	float glow = exp(-r * 6.0 / pulse) * rim; // footage WotW 4891: a tight halo, the spikes carry the size
	float inner = exp(-r * 11.0 / pulse);
	vec3 tint = torp_color.rgb;
	vec3 warm = mix(tint, vec3(1.0, 0.82, 0.55), torp_style == 1 ? 0.3 : 0.55);
	if (torp_style == 2) {
		// Plasma: a wider, churning ball.
		float a0 = atan(p.y, p.x);
		float churn = 0.7 + 0.45 * noise1(a0 * 2.5 + t * 4.0 + ph * 7.0) * noise1(r * 6.0 - t * 3.0 + ph);
		glow = exp(-r * 2.2 / pulse) * rim * churn;
	}
	// Spikes: n of them, uneven length, twinkling, turning slowly. Their width is a perpendicular distance, so they
	// stay thin all the way out.
	float n = torp_style == 1 ? 6.0 : 4.0; // footage WotW 4891 s: a photon is a 4-point star
	float ang = atan(p.y, p.x) + 0.35 + 0.15 * sin(ph) + t * 0.08; // near-fixed, slightly off the axes
	float k = ang * n / 6.2831853;
	float fi = floor(k + 0.5);
	float off = (k - fi) * 6.2831853 / n;
	float h = hash1(fi + ph * 13.0);
	float slen = torp_style == 0 ? 0.85 + 0.15 * h : 0.55 + 0.45 * h; // long glints to the sprite edge
	float twinkle = 0.75 + 0.25 * sin(t * (9.0 + 6.0 * h) + fi * 1.7 + ph);
	float thin = torp_style == 1 ? 0.02 : 0.034;
	float spike = exp(-abs(r * sin(off)) / thin) * (1.0 - smoothstep(0.0, slen, r)) * twinkle * rim;
	spike *= torp_style == 2 ? 0.35 : 1.0;
	// The lens streak: screen-horizontal (it does not turn), the full width of the sprite.
	float streak = torp_style != 1 ? 0.0 : 0.4 * exp(-abs(p.y) / 0.035) * exp(-abs(p.x) / (aspect * 0.42)) * (0.8 + 0.2 * pulse);
	float ring = torp_style == 1 ? exp(-pow((r - 0.45) / 0.035, 2.0)) * 0.6 * rim : 0.0;
	vec3 streak_col = torp_style == 1 ? mix(tint, vec3(0.9, 0.95, 1.0), 0.5) : tint;
	vec3 col = tint * (glow * 2.0 + spike * 3.0 + ring * 2.0) + warm * inner * 2.2 + streak_col * streak * 1.8
		+ torp_core.rgb * core * 2.4;
	float near_fade = smoothstep(1.0, 8.0, -VERTEX.z); // a torpedo passing the lens does not blind it
	ALBEDO = col * torp_intensity * near_fade;
	ALPHA = 1.0;
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility (the web build) adds this glow with no HDR lift, so dim parts all but vanish on black: lift
	// what is added with an sRGB-like curve, as the other effect shaders do (M1 battleplay).
	ALBEDO = pow(max(ALBEDO, vec3(0.0)), vec3(1.0 / 2.2));
#endif
}
