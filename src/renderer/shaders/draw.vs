#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 color;

out vec3 vert_norm;
out vec4 vert_color;
out vec4 light_pos;
out vec4 world_pos;

layout (location = 0) uniform mat4 model;
layout (location = 1) uniform mat4 view_proj;
layout (location = 2) uniform mat4 light_transform;

void
main()
{
    world_pos = model * vec4(aPos, 1.0);
    gl_Position = view_proj * world_pos;
    vert_color = vec4(color, 1.0);
    vert_norm = norm;
    light_pos = light_transform * world_pos;
}
