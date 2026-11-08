// Cloud shell over a textured planet: the cloud map's red channel is the coverage, tinted per class, lit by the same sun
// as the surface. Slightly larger sphere, alpha blended. Compatibility-safe.
shader_type spatial;
render_mode unshaded, fog_disabled, shadows_disabled, cull_back, depth_draw_never;
#include "res://scenes/galaxy/shaders/planet_light.gdshaderinc"

uniform sampler2D clouds_tex : filter_linear_mipmap, repeat_enable;
uniform vec3 tint : source_color = vec3(1.0);
uniform float alpha = 0.8;
uniform float drift = 0.003;

void vertex() {
	v_world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float c = texture(clouds_tex, vec2(UV.x + TIME * drift, UV.y)).r;
	vec3 nw = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	float day = planet_day(dot(nw, planet_sun(v_world)));
	ALBEDO = tint * (day * sun_color * sun_energy + ambient);
	ALPHA = clamp(c * alpha, 0.0, 1.0);
}
