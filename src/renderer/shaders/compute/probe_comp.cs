#version 430 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout (binding = 0) uniform sampler2DArray color_texture;

struct SH2 {
    vec3 sh[9];
};


layout(std430, binding = 0) restrict readonly buffer sh_buffer {
    SH2 sh_data[256];
};

struct comp_SH2 {
    uvec3 bits[2];
    vec3 dc;
};

layout(std430, binding = 1) restrict writeonly buffer comp_buffer {
    ivec3 probe_dim;
    vec3 probe_extents;
    vec3 probe_offset;
    comp_SH2 comp_sh[];
};

layout(location = 0) uniform uint max_probe;
layout(location = 1) uniform uint offset;

const float irrad[] = {
	1.0,
	0.666667,
	0.25,
};

uint pack4(in vec4 v, in float bv)
{
    v = vec4(bv + 0.5f) + v * bv;
    return uint(v.x) | (uint(v.y) << 8) | (uint(v.z) << 16) | (uint(v.w) << 24);
}

void compress_sh(in float sh[9], out uint bo[2])
{
    float bv = float(1 << 7) - 1.0f;
    float lc = (1.0f / (sh[0] * 1.73205)) * irrad[1];
    float qz = (1.0f / (sh[0] * 2.23606)) * irrad[2];
    float nz = (1.0f / (sh[0] * 1.93649)) * irrad[2];

    bo[0] = pack4(vec4(sh[1], sh[2], sh[3], sh[4]) * vec4(lc, lc, lc, nz), bv);
    bo[1] = pack4(vec4(sh[5], sh[6], sh[7], sh[8]) * vec4(nz, qz, nz, nz), bv);
}

void
main()
{
    vec3 sh[9];

    if (gl_GlobalInvocationID.x >= max_probe) {
        return;
    }

    for (int i = 0; i < 9; i++) {
        sh[i] = sh_data[gl_GlobalInvocationID.x].sh[i];
    }

    uint probe_index = gl_GlobalInvocationID.x + offset;

    /*comp_sh[probe_index].sh[0] = sh[0];
    comp_sh[probe_index].sh[1] = sh[1] * irrad[1];
    comp_sh[probe_index].sh[2] = sh[2] * irrad[1];
    comp_sh[probe_index].sh[3] = sh[3] * irrad[1];
    comp_sh[probe_index].sh[4] = sh[4] * irrad[2];
    comp_sh[probe_index].sh[5] = sh[5] * irrad[2];
    comp_sh[probe_index].sh[6] = sh[6] * irrad[2];
    comp_sh[probe_index].sh[7] = sh[7] * irrad[2];
    comp_sh[probe_index].sh[8] = sh[8] * irrad[2];*/
    comp_sh[probe_index].dc = sh[0];

    float channel_sh[9];
    uint channel_bits[2];

    for (int i = 0; i < 9; i++) {
        channel_sh[i] = sh[i].r;
    }
    compress_sh(channel_sh, channel_bits);
    comp_sh[probe_index].bits[0].r = channel_bits[0];
    comp_sh[probe_index].bits[1].r = channel_bits[1];

    for (int i = 0; i < 9; i++) {
        channel_sh[i] = sh[i].g;
    }
    compress_sh(channel_sh, channel_bits);
    comp_sh[probe_index].bits[0].g = channel_bits[0];
    comp_sh[probe_index].bits[1].g = channel_bits[1];

    for (int i = 0; i < 9; i++) {
        channel_sh[i] = sh[i].b;
    }
    compress_sh(channel_sh, channel_bits);
    comp_sh[probe_index].bits[0].b = channel_bits[0];
    comp_sh[probe_index].bits[1].b = channel_bits[1];
}
