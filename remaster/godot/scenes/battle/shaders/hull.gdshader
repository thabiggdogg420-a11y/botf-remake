// PBR starship hull: plated panels with per-panel tint and roughness, recessed panel seams, rows of lit windows and
// a subtle aztec pattern. Coordinates are the part's local vertex position scaled to world units, so panels keep
// their size whatever the ship class.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

uniform vec3 base_color : source_color = vec3(0.62, 0.64, 0.68);
uniform vec3 accent_color : source_color = vec3(0.48, 0.5, 0.56);
uniform float metallic : hint_range(0.0, 1.0) = 0.55;
uniform float roughness : hint_range(0.0, 1.0) = 0.42;
uniform float panel_size = 0.35;
uniform vec3 window_color : source_color = vec3(1.0, 0.86, 0.6);
uniform float window_energy = 4.0;
uniform float window_density : hint_range(0.0, 1.0) = 0.35;
uniform float window_rows = 5.0;
uniform float aztec = 0.5;

varying vec3 v_pos;
varying vec3 v_nrm;

float h21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}
float h31(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

void vertex() {
	vec3 s = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), length(MODEL_MATRIX[2].xyz));
	v_pos = VERTEX * s;
	v_nrm = NORMAL;
}

// Distance to the nearest panel seam on the two axes of a projection plane, plus the panel id.
vec2 panel(vec2 p, float size, out vec2 id) {
	vec2 q = p / size;
	// Offset every other row, like hull plating.
	q.x += 0.5 * mod(floor(q.y), 2.0);
	id = floor(q);
	vec2 f = abs(fract(q) - 0.5);
	return (0.5 - f) * size;
}

void fragment() {
	vec3 n = normalize(v_nrm);
	vec3 w = abs(n);
	w = pow(w, vec3(4.0));
	w /= (w.x + w.y + w.z);
	vec2 idx; vec2 idy; vec2 idz;
	vec2 ex = panel(v_pos.zy, panel_size, idx);
	vec2 ey = panel(v_pos.xz, panel_size, idy);
	vec2 ez = panel(v_pos.xy, panel_size, idz);
	float seam = w.x * min(ex.x, ex.y) + w.y * min(ey.x, ey.y) + w.z * min(ez.x, ez.y);
	float pid = w.x * h21(idx) + w.y * h21(idy + 3.1) + w.z * h21(idz + 7.7);
	float seam_line = 1.0 - smoothstep(0.004, 0.012, seam);

	// Aztec: larger blocks of slightly different tone.
	vec2 ida; vec2 idb; vec2 idc;
	panel(v_pos.zy, panel_size * 3.0, ida);
	panel(v_pos.xz, panel_size * 3.0, idb);
	panel(v_pos.xy, panel_size * 3.0, idc);
	float az = w.x * h21(ida + 1.0) + w.y * h21(idb + 2.0) + w.z * h21(idc + 5.0);

	vec3 col = mix(base_color, accent_color, step(0.6, az) * aztec);
	col *= 0.9 + 0.2 * pid;
	col *= 1.0 - 0.45 * seam_line;
	ALBEDO = col;
	METALLIC = metallic;
	ROUGHNESS = clamp(roughness + (pid - 0.5) * 0.25 + seam_line * 0.3, 0.05, 1.0);

	// Windows: short rows on the flanks (not on top or bottom faces).
	float side = 1.0 - smoothstep(0.55, 0.8, abs(n.y));
	float rows = window_rows / max(panel_size * 3.0, 0.01);
	float ry = fract(v_pos.y * rows);
	float along = (abs(n.x) > abs(n.z)) ? v_pos.z : v_pos.x;
	float cx = along * rows * 2.2;
	float cell = floor(cx);
	float lit = step(1.0 - window_density, h21(vec2(cell, floor(v_pos.y * rows)) + 0.37));
	float win = smoothstep(0.35, 0.45, ry) * (1.0 - smoothstep(0.55, 0.65, ry))
		* smoothstep(0.15, 0.3, fract(cx)) * (1.0 - smoothstep(0.7, 0.85, fract(cx)));
	float warm = h31(vec3(cell, floor(v_pos.y * rows), 1.0));
	float energy = window_energy;
	// Hull-look lane (25 Nov 2026, Simon: "little white specs all over them, and the starbases in the battles do too"). This
	// window grid is finer than a pixel at battle and title range in every renderer, and its energy (window_energy x the
	// battle's WINDOW_GAIN, 9.6) is far over the glow threshold, so each aliased window cell was a white sparkle that
	// bloomed. The Compatibility build had the fix (below); Forward+ did not. Now both fade the windows out as a cell
	// shrinks under ~3 pixels and cap their energy under the glow threshold.
	float px = max(fwidth(cx), fwidth(v_pos.y * rows));
	energy = min(energy, 2.2) * (1.0 - smoothstep(0.12, 0.35, px));
	ROUGHNESS = max(ROUGHNESS, 0.45 + 0.4 * smoothstep(0.05, 0.3, px));
#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	energy = min(energy, 1.2);   // no HDR range: anything over 1.0 clips to white
	// The 8-bit frame clips the lit plating that Forward+ ACES rolls off: a shade darker reads as the desktop's grey.
	ALBEDO *= 0.8;
#endif
	EMISSION = window_color * (0.7 + 0.6 * warm) * win * lit * side * energy;
}
