#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 norm;
layout (location = 2) in vec3 color;

layout (location = 0) uniform mat4 model;
layout (location = 1) uniform mat4 view_proj;

void
main()
{
    gl_Position = view_proj * model * vec4(aPos, 1.0);
}
