#version 430 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0) uniform sampler2D color_cubemap;

struct SH2 {
    vec3 sh[9];
};

layout(std430, binding = 0) buffer sh_buffer {
    SH2 sh_data[];
};

layout(location = 0) uniform int face;
layout(location = 1) uniform uint max_probe;

const int cubemap_res = 64;

const float PI = 3.14159265;

shared vec3 local_sh[9 * 64];

const float c[] = {
	0.282095,
	0.488603,
	1.092548,
	0.315392,
	0.546274
};

vec3
dir_from_coords(in vec2 coords, in int face)
{
    float inv_res = 1.0 / float(cubemap_res);
    float u = (2.0 * (float(coords.x) + 0.5) * inv_res) - 1.0f;
    float v = (2.0 * (float(coords.y) + 0.5) * inv_res) - 1.0f;

    vec3 dir;
    switch (face) {
        case 0: dir = vec3(1.0, -v, -u);
                break;
        case 1: dir = vec3(-1.0, -v, u);
                break;
        case 2: dir = vec3(u, 1.0, v);
                break;
        case 3: dir = vec3(u, -1.0, -v);
                break;
        case 4: dir = vec3(u, -v, 1.0);
                break;
        case 5: dir = vec3(-u, -v, -1.0);
                break;
    }
    return normalize(dir);
}

float area_element(float x, float y)
{
    return atan(x * y, sqrt(x * x + y * y + 1.0));
}

float texel_solid_angle(int x, int y)
{
    float inv_res = 1.0 / float(cubemap_res);
    float u = (2.0 * (float(x) + 0.5) * inv_res) - 1.0f;
    float v = (2.0 * (float(y) + 0.5) * inv_res) - 1.0f;

    float x0 = u - inv_res;
    float y0 = v - inv_res;
    float x1 = u + inv_res;
    float y1 = v + inv_res;
    float angle = area_element(x0, y0) - area_element(x0, y1) - area_element(x1, y0) + area_element(x1, y1);

    return angle;
}

void
main()
{
    uint probe_index = gl_WorkGroupID.x + gl_WorkGroupID.y * gl_NumWorkGroups.x;
    if (probe_index >= max_probe) {
        return;
    }

    vec3 sh[9];
    for (int i = 0; i < 9; i++) {
        sh[i] = vec3(0.0);
    }

    ivec2 offset = ivec2(gl_WorkGroupID.x * 64, gl_WorkGroupID.y * 64);
    ivec2 pos = ivec2(gl_LocalInvocationID.x, 0);
    for (int i = 0; i < cubemap_res; i++) {
        ivec2 offset_pos = pos + offset;
        vec3 col = texelFetch(color_cubemap, offset_pos, 0).rgb;
        vec3 dir = dir_from_coords(vec2(pos), face);
        float weight = texel_solid_angle(pos.x, pos.y);

        sh[0] += c[0] * col * weight;
        sh[1] += c[1] * col * dir.y * weight;
        sh[2] += c[1] * col * dir.z * weight;
        sh[3] += c[1] * col * dir.x * weight;
        sh[4] += c[2] * col * dir.y * dir.x * weight;
        sh[5] += c[2] * col * dir.y * dir.z * weight;
        sh[6] += c[3] * col * (3.0 * dir.z * dir.z - 1.0) * weight;
        sh[7] += c[2] * col * dir.x * dir.z * weight;
        sh[8] += c[4] * col * (dir.x * dir.x - dir.y * dir.y) * weight;

        pos += ivec2(0, 1);
    }

    uint ind = gl_LocalInvocationIndex * 9;
    for (uint i = 0; i < 9; i++) {
        local_sh[ind + i] = sh[i];
    }

    barrier();

    if (gl_LocalInvocationIndex == 0) {
        for (int i = 1; i < 64; i++) {
            sh[0] += local_sh[i * 9 + 0];
            sh[1] += local_sh[i * 9 + 1];
            sh[2] += local_sh[i * 9 + 2];
            sh[3] += local_sh[i * 9 + 3];
            sh[4] += local_sh[i * 9 + 4];
            sh[5] += local_sh[i * 9 + 5];
            sh[6] += local_sh[i * 9 + 6];
            sh[7] += local_sh[i * 9 + 7];
            sh[8] += local_sh[i * 9 + 8];
        }

        for (int i = 0; i < 9; i++) {
            sh_data[probe_index].sh[i] += sh[i];
        }
    }
}
