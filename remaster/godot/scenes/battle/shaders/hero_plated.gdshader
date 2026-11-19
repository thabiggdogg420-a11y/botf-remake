// Plated hero hull (an untextured local model, hullhd_build.py bake_plating): the baked vertex colour "Col" (faction
// hull colour, underside accent, mottling, AO; linear) x a fine, ship-scale plating drawn here.
// F4 round 2 (20 Nov 2026): the old tiling T_HullPanels read as brick or cobblestone on these hulls, so the plating is
// procedural, in metres, projected from the model's own axes (the glb is metres, nose +X, up +Y): small low-contrast
// plates, larger panel bands that lean toward the power's accent colour, deck rows of lit windows on the sides, and
// every detail faded to its average once it is smaller than a pixel (no shimmer at battle range).
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;

uniform bool use_vertex_color = true;
uniform vec3 tint = vec3(0.46, 0.48, 0.52);   // linear; the flat hull colour when there is no vertex bake
uniform float color_gain = 1.0;
uniform float roughness_min = 0.32;
uniform float metallic = 0.15;
// Ship-scale plating (hero_look.gd FINE_PLATING sets these per power from the hull's canon length).
uniform float plate_m = 5.0;          // small plate width in metres (plates are twice as long as wide)
uniform float band_m = 24.0;          // panel band size in metres
uniform float plate_contrast = 0.05;  // +- tone of a plate
uniform float seam_dark = 0.12;       // how much a seam line darkens
uniform vec3 accent = vec3(1.0);      // multiplier a band leans toward (the power's second colour)
uniform float accent_amt = 0.25;
uniform float deck_m = 3.6;           // window row spacing
uniform float window_density = 0.35;  // share of side bands that carry window rows
uniform vec3 window_color = vec3(1.0, 0.85, 0.6);
uniform float window_gain = 1.6;

varying vec3 v_pos;
varying vec3 v_nrm;

float h21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

// One projected plane: returns tone (x), seam (y), roughness (z), window light (w).
vec4 plating(vec2 m, bool side) {
	// Small plates, rows offset like hull plating.
	vec2 q = m / vec2(plate_m * 2.0, plate_m);
	q.x += h21(vec2(floor(q.y), 9.1)) * 4.0;               // each row its own offset: no brick bond
	vec2 id = floor(q);
	vec2 fq = fract(q);
	// Mixed plate sizes: some plates split in two lengthwise, some across.
	float sp = h21(id + 11.3);
	if (sp > 0.55) { fq.x = fract(fq.x * 2.0); id.x += step(0.5, fract(q.x)) * 0.37; }
	else if (sp < 0.2) { fq.y = fract(fq.y * 2.0); id.y += step(0.5, fract(q.y)) * 0.53; }
	vec2 f = abs(fq - 0.5);
	vec2 w = fwidth(q);
	float fade = clamp(2.0 - 4.0 * max(w.x, w.y), 0.0, 1.0);   // plates smaller than ~4 px fade to their mean
	float edge = 0.5 - max(f.x, f.y);
	float line = (1.0 - smoothstep(0.0, max(w.x, w.y) * 1.2 + 0.02, edge)) * fade;
	float t = (h21(id) - 0.5) * fade;
	// Bands: larger panel groups.
	vec2 b = floor(m / band_m);
	float bh = h21(b + 17.0);
	float rough = mix(0.5, 0.38 + 0.25 * h21(id + 3.7), fade);
	// Windows: deck rows on the sides of some bands, one light every deck_m along.
	float win = 0.0;
	if (side && bh < window_density) {
		vec2 g = m / vec2(deck_m * 1.4, deck_m);
		vec2 gi = floor(g);
		vec2 gf = abs(fract(g) - 0.5);
		vec2 gw = fwidth(g);
		// Single deck rows in runs, not blocks: a third of the decks in a band are lit, each in runs of windows.
		float row = step(h21(vec2(gi.y, b.x * 3.1 + b.y)), 0.3);
		float run = step(h21(vec2(floor(gi.x / 7.0), gi.y + b.y)), 0.55);
		float on = row * run * step(0.15, h21(gi + b));        // a few dark windows
		float cell = (1.0 - smoothstep(0.16, 0.16 + gw.x, gf.x)) * (1.0 - smoothstep(0.11, 0.11 + gw.y, gf.y));
		float wf = clamp(1.5 - 2.0 * max(gw.x, gw.y), 0.0, 1.0);
		win = on * mix(0.07, cell, wf);                  // sub-pixel windows: their average glow
	}
	return vec4(t, line, rough, win);
}

void vertex() {
	v_pos = VERTEX;
	v_nrm = NORMAL;
}

void fragment() {
	vec3 n = abs(normalize(v_nrm));
	vec3 wgt = pow(n, vec3(6.0));
	wgt /= (wgt.x + wgt.y + wgt.z);
	vec4 px = plating(v_pos.zy, false);     // bow / stern faces
	vec4 py = plating(v_pos.xz, false);     // top / bottom
	vec4 pz = plating(v_pos.xy, true);      // port / starboard: window rows run fore-aft
	vec4 p = px * wgt.x + py * wgt.y + pz * wgt.z;
	vec2 band = floor(v_pos.xz / band_m) + floor(v_pos.y / band_m) * 7.0;
	float bt = h21(band + 5.0);
	vec3 base = use_vertex_color ? COLOR.rgb : tint;
	vec3 lean = mix(vec3(1.0), accent, accent_amt * bt);
	float shade = (1.0 + p.x * 2.0 * plate_contrast + (bt - 0.5) * plate_contrast) * (1.0 - seam_dark * p.y);
	ALBEDO = base * lean * shade * color_gain;
	ROUGHNESS = max(p.z, roughness_min);
	METALLIC = metallic;
	EMISSION = window_color * window_gain * p.w;
}
