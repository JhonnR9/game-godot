shader_type spatial;
render_mode cull_back;

uniform sampler2D albedo_texture : source_color, filter_nearest, repeat_enable;

void fragment() {
	vec4 texel = texture(albedo_texture, UV);
	ALBEDO = texel.rgb * COLOR.rgb;
	ROUGHNESS = 0.9;
}
