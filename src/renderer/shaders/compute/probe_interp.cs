#version 430 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct SH2 {
    uvec3 comp_sh[2];
    vec3 dc;
};

layout(std430, binding = 0) buffer sh_buffer {
    ivec3 probe_dim;
    vec3 probe_extents;
    vec3 probe_offset;
    SH2 sh_data[];
};

layout(std430, binding = 1) buffer invalid_buffer {
    int invalid_len;
    ivec4 invalid_probes[];
};

layout(std430, binding = 2) buffer valid_data {
    bool valid[];
};

uint pack4(in vec4 v, in float bv)
{
    v = vec4(bv + 0.5f) + v * bv;
    return uint(v.x) | (uint(v.y) << 8) | (uint(v.z) << 16) | (uint(v.w) << 24);
}

void compress_sh(in float sh[9], out uint bo[2])
{
    float bv = float(1 << 7) - 1.0f;
    float lc = 1.0f / (sh[0] * 1.73205);
    float qz = 1.0f / (sh[0] * 2.23606);
    float nz = 1.0f / (sh[0] * 1.93649);

    bo[0] = pack4(vec4(sh[1], sh[2], sh[3], sh[4]) * vec4(lc, lc, lc, nz), bv);
    bo[1] = pack4(vec4(sh[5], sh[6], sh[7], sh[8]) * vec4(nz, qz, nz, nz), bv);
}

vec4 unpack4(in uint r, in float bv)
{
    const uint m8 = 0xFF;
    vec4 tmp = vec4(r & m8, (r >> 8) & m8, (r >> 16) & m8, (r >> 24) & m8) / bv;
    tmp -= vec4(1.0f);
    return tmp;
}

void
uncompress_sh_chan(in uint shBits[2], in float DC, out float shOut[9])
{
    float bv = float(1 << 7) - 1.0f;
    float lc = DC * sqrt(3.0f);
    float qz = DC * sqrt(5.0f);
    float nz = DC * (sqrt(15.0f) / 2.0f);
    vec4 shA = unpack4(shBits[0], bv) * vec4(lc, lc, lc, nz);
    vec4 shB = unpack4(shBits[1], bv) * vec4(nz, qz, nz, nz);

    shOut[0] = DC;
    shOut[1] = shA.x;
    shOut[2] = shA.y;
    shOut[3] = shA.z;
    shOut[4] = shA.w;
    shOut[5] = shB.x;
    shOut[6] = shB.y;
    shOut[7] = shB.z;
    shOut[8] = shB.w;
}

void
uncompress_sh(inout vec3 sh[9], in int probe_index, in float weight)
{
    uvec3 comp_sh[2];
    vec3 dc;

    comp_sh[0] = sh_data[probe_index].comp_sh[0];
    comp_sh[1] = sh_data[probe_index].comp_sh[1];
    dc = sh_data[probe_index].dc;

    uint chan_bits[2];
    float chan_sh[9];

    chan_bits[0] = comp_sh[0].r;
    chan_bits[1] = comp_sh[1].r;
    uncompress_sh_chan(chan_bits, dc.r, chan_sh);
    for (int i = 0; i < 9; i++) {
        sh[i].r += chan_sh[i] * weight;
    }

    chan_bits[0] = comp_sh[0].g;
    chan_bits[1] = comp_sh[1].g;
    uncompress_sh_chan(chan_bits, dc.g, chan_sh);
    for (int i = 0; i < 9; i++) {
        sh[i].g += chan_sh[i] * weight;
    }

    chan_bits[0] = comp_sh[0].b;
    chan_bits[1] = comp_sh[1].b;
    uncompress_sh_chan(chan_bits, dc.b, chan_sh);
    for (int i = 0; i < 9; i++) {
        sh[i].b += chan_sh[i] * weight;
    }
};

int
get_index(in ivec3 grid_pos)
{
    return grid_pos.x + grid_pos.y * probe_dim.x + grid_pos.z * probe_dim.x * probe_dim.y;
}

void
interp_in_dir(in ivec3 origin, in ivec3 dir, inout vec3 sh[9], inout float weight_accum)
{
    bool is_valid = false;
    float steps = 1.0;
    ivec3 w = origin + dir;
    //while (!is_valid && all(lessThan(w, probe_dim)) && all(greaterThanEqual(w, ivec3(0)))) {
        is_valid = valid[get_index(w)];
        if (is_valid) {
            weight_accum += 1.0;
            uncompress_sh(sh, get_index(w), 1.0 / steps);
        }
        //steps += 1.0;
        //w += dir;
    //}
}

void
main()
{
    if (gl_GlobalInvocationID.x >= invalid_len) {
        return;
    }

    vec3 sh[9];
    for (int i = 0; i < 9; i++) {
        sh[i] = vec3(0.0);
    }

    ivec3 origin = invalid_probes[gl_GlobalInvocationID.x].xyz;
    uint probe_index = get_index(origin);
    bool is_valid = false;
    float weight_accum = 0.0;
    ivec3 w;

    interp_in_dir(origin, ivec3( 1,  0,  0), sh, weight_accum);
    interp_in_dir(origin, ivec3(-1,  0,  0), sh, weight_accum);
    interp_in_dir(origin, ivec3( 0,  1,  0), sh, weight_accum);
    interp_in_dir(origin, ivec3( 0, -1,  0), sh, weight_accum);
    interp_in_dir(origin, ivec3( 0,  0,  1), sh, weight_accum);
    interp_in_dir(origin, ivec3( 0,  0, -1), sh, weight_accum);
    
    for (int i = 0; i < 9; i++) {
        sh[i] /= weight_accum;
    }

    sh_data[probe_index].dc = sh[0];

    float channel_sh[9];
    uint channel_comp_sh[2];

    for (int i = 0; i < 9; i++) {
        channel_sh[i] = sh[i].r;
    }
    compress_sh(channel_sh, channel_comp_sh);
    sh_data[probe_index].comp_sh[0].r = channel_comp_sh[0];
    sh_data[probe_index].comp_sh[1].r = channel_comp_sh[1];

    for (int i = 0; i < 9; i++) {
        channel_sh[i] = sh[i].g;
    }
    compress_sh(channel_sh, channel_comp_sh);
    sh_data[probe_index].comp_sh[0].g = channel_comp_sh[0];
    sh_data[probe_index].comp_sh[1].g = channel_comp_sh[1];

    for (int i = 0; i < 9; i++) {
        channel_sh[i] = sh[i].b;
    }
    compress_sh(channel_sh, channel_comp_sh);
    sh_data[probe_index].comp_sh[0].b = channel_comp_sh[0];
    sh_data[probe_index].comp_sh[1].b = channel_comp_sh[1];
}
