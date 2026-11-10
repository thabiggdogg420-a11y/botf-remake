// Weapon ribbon (M1 weaponfx; docs/WEAPON-FX.md). One camera-facing strip draws a beam, a bolt or a phaser charge
// strip. The node's local Y axis runs from the start (y = -0.5) to the end (y = +0.5); BattleFx sets it per frame.
// The vertex shader builds the strip in view space, facing the camera, with its half width the larger of the world
// width and a screen floor (min_px at 1080 lines, per vertex, so the far end of a long beam keeps the floor too).
// Styles: 0 phaser, 1 disruptor beam (pulse bands), 2 Cardassian spiral, 3 polaron, 4 phaser charge strip, 5 bolt.
// Standard render modes only, no screen or depth textures: the same pass in Forward+ and Compatibility (WebGL2).
// Plain uniforms on a material per effect node (BattleFx._fxp), not instance uniforms: see there.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, skip_vertex_transform;

uniform int style = 0;
uniform vec4 col_core : source_color = vec4(1.0, 0.957, 0.839, 1.0);
uniform vec4 col_body : source_color = vec4(1.0, 0.604, 0.18, 1.0);
uniform vec4 col_glow : source_color = vec4(1.0, 0.353, 0.039, 1.0);
uniform float width_world = 1.8;  // full width in world units
uniform float min_px = 16.0;      // full width floor in pixels at 1080 lines
uniform float intensity = 1.0;
uniform float phase = 0.0;        // per shot: noise offset
uniform float charge = 0.0;       // style 4: 0..1, the run of the charge to the emitter
uniform float heavy = 0.0;        // style 5: Romulan plasma wobble
uniform float end_dim = 1.0;      // < 1 when several beams strike one target: their ends share one glare budget
uniform float near_from = 4.0;    // fade the part of a ribbon that passes right by the lens
uniform float near_to = 30.0;

varying float v_s;       // distance along the ribbon from its start, world units
varying float v_len;     // ribbon length, world units
varying float v_hw;      // half width used, world units
varying float v_cap;     // -1 / +1 at the rounded end caps, 0 along the body
varying float v_depth;
varying float v_end_on;

float hash1(float n) { return fract(sin(n) * 43758.5453); }
float noise1(float x) {
	float i = floor(x);
	float f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(hash1(i), hash1(i + 1.0), f);
}

void vertex() {
	vec3 a = (MODELVIEW_MATRIX * vec4(0.0, -0.5, 0.0, 1.0)).xyz;
	vec3 b = (MODELVIEW_MATRIX * vec4(0.0, 0.5, 0.0, 1.0)).xyz;
	vec3 d = b - a;
	float len = max(length(d), 1e-4);
	vec3 dir = d / len;
	float along = clamp(VERTEX.y + 0.5, 0.0, 1.0);
	vec3 p = a + d * along;
	float depth = max(-p.z, 0.05);
	// World size of min_px pixels at 1080 lines (the viewport height cancels: pixels scale with it). abs(): the
	// Vulkan renderers flip the projection's Y, so [1][1] is negative there and positive in Compatibility.
	float floor_hw = min_px * depth / (abs(PROJECTION_MATRIX[1][1]) * 1080.0);
	float hw = max(width_world * 0.5, floor_hw);
	vec3 side = cross(dir, normalize(p));
	float sl = length(side);
	side = sl > 1e-4 ? side / sl : vec3(1.0, 0.0, 0.0);
	float cap = UV2.x;
	p += side * hw * VERTEX.x + dir * hw * cap;
	VERTEX = p;
	NORMAL = vec3(0.0, 0.0, 1.0);
	v_s = along * len;
	v_len = len;
	v_hw = hw;
	v_cap = cap;
	v_depth = depth;
	v_end_on = abs(dir.z);
}

void fragment() {
	float x = UV.x;              // -1..1 across the strip
	float s = v_s;
	float t = TIME;
	vec3 col = vec3(0.0);
	float dcap = length(vec2(x, v_cap));
	if (style == 4) {
		// Phaser charge: the strip glows dim, then two bright points run from its ends to the emitter at its middle.
		float u = clamp(v_s / max(v_len, 1e-3), 0.0, 1.0);
		float body = exp(-pow(dcap / 0.45, 2.0));
		float core = exp(-pow(dcap / 0.2, 2.0));
		float run = 0.5 * clamp(charge, 0.0, 1.0);
		float w = max(v_hw * 2.2 / max(v_len, 1e-3), 0.02);
		float pts = exp(-pow((u - run) / w, 2.0)) + exp(-pow((u - (1.0 - run)) / w, 2.0));
		float lit = smoothstep(0.0, 0.25, charge) * (0.3 + 0.25 * charge);
		col = col_body.rgb * body * lit + (col_core.rgb * core * 2.6 + col_body.rgb * body * 1.2) * pts * (0.4 + 0.6 * charge);
	} else if (style == 5) {
		// Bolt: a capsule, the head (the end) brighter than the tail, pulse bands running along it.
		float wob = heavy * 0.18 * sin(s * 0.9 - t * 22.0 + phase * 5.0);
		float dx = length(vec2(x - wob, v_cap));
		float u = clamp(v_s / max(v_len, 1e-3), 0.0, 1.0);
		// Teardrop (footage SoA 1249, WotW 4916): a bright round head and a tail that tapers away behind it.
		float head = mix(0.15, 1.0, u * u);
		float taper = mix(0.3, 1.0, pow(u, 0.6));
		dx /= taper;
		float band = 0.8 + 0.2 * sin(s * 1.4 - t * 50.0 + phase * 3.0);
		float core = exp(-pow(dx / mix(0.3, 0.36, heavy), 2.0));
		float body = exp(-pow(dcap / (taper * mix(0.62, 0.7, heavy)), 2.0));
		float glow = exp(-dcap * mix(2.4, 1.8, heavy)) * max(0.0, 1.0 - dcap * dcap);
		col = (col_core.rgb * core * 3.4 + col_body.rgb * body * 2.2) * head * band + col_glow.rgb * glow * 0.9 * head;
	} else {
		float jit = 0.0;
		float flick = 1.0;
		float pulse = 0.85 + 0.15 * sin(s * 0.9 - t * 40.0 + phase * 7.0);
		if (style == 0) {
			// Phaser crackle: the core wanders a little across the beam and the brightness flickers.
			jit = (noise1(s * 0.17 - t * 7.0 + phase) - 0.5) * 0.12;
			flick = 0.85 + 0.3 * noise1(t * 25.0 + phase * 3.1 + s * 0.15);
		} else if (style == 1) {
			pulse = 0.55 + 0.45 * smoothstep(0.2, 0.9, sin(s * 0.5 - t * 125.0 + phase));
		} else if (style == 3) {
			jit = (noise1(s * 0.21 - t * 9.0 + phase) - 0.5) * 0.06;
			flick = 0.7 + 0.3 * step(0.35, noise1(t * 18.0 + phase * 2.3));
		}
		float dc = length(vec2(x - jit, v_cap));
		float core = exp(-pow(dc / 0.16, 2.0));
		float body = exp(-pow(dcap / 0.42, 2.0));
		float glow = exp(-dcap * 3.2) * max(0.0, 1.0 - dcap * dcap);
		if (style == 2) {
			// Cardassian (footage: Tears of the Prophets 1712 s, SoA 343 s): a steady yellow beam strung with bright
			// beads that run from the emitter to the target, the "spiral" seen side-on. Bead pitch 4 half widths.
			float pitch = max(2.0, v_hw * 4.0);
			float q = fract(s / pitch - t * 3.0 + phase);
			float bead = exp(-pow((q - 0.5) / 0.2, 2.0)) * exp(-pow(dc / 0.7, 2.0));
			col = col_core.rgb * exp(-pow(dc / 0.13, 2.0)) * 2.2 + col_body.rgb * (bead * 2.6 + body * 0.6)
				+ col_glow.rgb * glow * 0.6;
		} else {
			col = col_core.rgb * core * 2.4 * flick + col_body.rgb * body * 1.6 * pulse * mix(1.0, flick, 0.5)
				+ col_glow.rgb * glow * 0.8;
		}
	}
	if (style <= 3) {
		// Stacked beams converge on one target: dim the last stretch of each so the meeting point does not white out.
		col *= mix(1.0, end_dim, smoothstep(v_len - 12.0 * v_hw, v_len, v_s));
	}
	// End-on views stack the strip into one bright spot: dim them. Fade the part that passes the lens.
	float dim = mix(1.0, 0.2, smoothstep(0.85, 1.0, v_end_on)) * smoothstep(near_from, near_to, v_depth);
	ALBEDO = col * intensity * dim;
	ALPHA = 1.0;
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	// Compatibility (the web build) adds this glow with no HDR lift, so dim parts all but vanish on black: lift
	// what is added with an sRGB-like curve, as the other effect shaders do (M1 battleplay).
	ALBEDO = pow(max(ALBEDO, vec3(0.0)), vec3(1.0 / 2.2));
#endif
}
