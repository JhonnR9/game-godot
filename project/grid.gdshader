shader_type spatial;
render_mode cull_back;
varying vec3 world_position;
void vertex() {
    world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}
void fragment() {
    vec2 p = world_position.xz / 2.0;
    vec2 distance_to_line = abs(fract(p - 0.5) - 0.5) / max(fwidth(p), vec2(0.0001));
    float line = 1.0 - smoothstep(0.6, 1.5, min(distance_to_line.x, distance_to_line.y));
    float fade = 1.0 - smoothstep(10.0, 40.0, length(world_position.xz));
    ALBEDO = mix(vec3(0.24, 0.25, 0.27), vec3(0.55, 0.57, 0.6), line * fade);
    ROUGHNESS = 0.9;
}
