#version 430 core

layout(binding = 0) uniform sampler2D hdr_buffer;

in vec2 uv;
out vec4 frag_color;

const float exposure = 1.0;

vec3
tonemap(vec3 col)
{
    //vec3 m = vec3(1.0) - exp(-col * exposure);
    vec3 m = col;
    m = pow(m, vec3(1.0 / 2.2));
    return m;
}

void
main()
{
    vec4 col = texture(hdr_buffer, uv);
    vec3 mapped = tonemap(col.rgb);
    frag_color = vec4(mapped, 1.0);
    //frag_color = vec4(uv, 1.0, 1.0);
}
