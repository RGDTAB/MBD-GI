#version 430 core
out vec4 frag_color;

in vec3 vert_norm;
in vec4 vert_color;
in vec4 light_pos;
in vec4 world_pos;

layout (location = 3) uniform vec3 light_dir;
layout (binding = 0) uniform sampler2DShadow shadow_map;

float
shadow_test()
{
    vec3 proj_coords = light_pos.xyz / light_pos.w;
    proj_coords = fma(proj_coords, vec3(0.5), vec3(0.5));

    float current_depth = proj_coords.z;

    vec2 texel_size = 1.0 / textureSize(shadow_map, 0);
    texel_size *= 1.2;
    
    float pcf_shadow = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            pcf_shadow += texture(shadow_map, vec3(proj_coords.xy + vec2(x, y) * texel_size, current_depth));
        }
    }
    pcf_shadow /= 25.0;

    return pcf_shadow;
}

void
main()
{
    vec3 normal = normalize(vert_norm);
    float shadow = shadow_test();
    float diffuse = 1.0 * (1.0 - shadow) * max(0.0, dot(light_dir, normal));
    frag_color = vec4(vert_color.xyz * diffuse, 1.0);
}
