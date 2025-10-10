#version 430 core
out vec4 frag_color;

in vec3 vert_norm;
in vec4 vert_color;
in vec4 light_pos;
in vec4 world_pos;

layout (location = 3) uniform vec3 light_dir;
layout (location = 4) uniform bool ambient_only;
layout (binding = 0) uniform sampler2DShadow shadow_map;

struct comp_SH2 {
    uvec3 comp_sh[2];
    vec3 dc;
};

layout(std430, binding = 0) restrict readonly buffer sh_buffer {
    ivec3 probe_dim;
    vec3 probe_extents;
    vec3 probe_offset;
    comp_SH2 sh_data[];
};

const float c[] = {
    0.282095,
    0.488603,
    1.092548,
    0.315392,
    0.546274
};

const ivec3 trilin_offsets[8] = {
    ivec3(0, 0, 0),
    ivec3(1, 0, 0),
    ivec3(0, 1, 0),
    ivec3(1, 1, 0),
    ivec3(0, 0, 1),
    ivec3(1, 0, 1),
    ivec3(0, 1, 1),
    ivec3(1, 1, 1),
};

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
    dc = sh_data[probe_index].dc.rgb;

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
}

int
get_index(in ivec3 grid_pos)
{
    return grid_pos.x + grid_pos.y * probe_dim.x + grid_pos.z * probe_dim.x * probe_dim.y;
}

void
trilin_interp_sh(vec3 p, inout vec3 sh[9])
{
    ivec3 g = ivec3(p);
    p = p - trunc(p);
    ivec3 o;
    vec3 d;
    float w;
    int sh_pos;

    for (int i = 0; i < 8; i++) {
        o = g + trilin_offsets[i];
        bool valid = all(lessThanEqual(o, probe_dim - 1));
        o = min(o, probe_dim - 1);
        sh_pos = get_index(o);

        d = vec3(1.0f) - p - vec3(trilin_offsets[i]);
        w = abs(d.x * d.y * d.z);
        w = valid ? w : 0.0f;
        uncompress_sh(sh, sh_pos, w);
    }
}

void
get_probe_indices(in vec3 pos, out ivec4 indices, out vec4 weights)
{
    pos -= probe_offset;
    pos = pos / probe_extents;

    vec3 c_pos = clamp(pos, 0.0, 1.0);
    c_pos *= vec3(probe_dim - 1);
    ivec3 g = ivec3(trunc(c_pos));
    vec3 r = c_pos - trunc(c_pos);

    ivec3 vert2 = ivec3(0, 0, 0);
    ivec3 vert3 = ivec3(1, 1, 1);
    bvec3 c = greaterThanEqual(r.xyz, r.yzx);
    bool c_xy = c.x, c_yz = c.y, c_zx = c.z;
    bool c_yx = !c.x, c_zy = !c.y, c_xz = !c.z;
    bool cond;
    vec3 s;
    #define ORDER(X, Y, Z) \
    cond = c_ ## X ## Y && c_ ## Y ## Z; \
    s = cond ? r.X ## Y ## Z : s; \
    vert2.X = cond ? 1 : vert2.X; \
    vert3.Z = cond ? 0 : vert3.Z;

    ORDER(x,y,z)
    ORDER(x,z,y)
    ORDER(z,x,y)

    ORDER(z,y,x)
    ORDER(y,z,x)
    ORDER(y,x,z)

    weights = vec4(1 - s.x, s.z, s.x - s.y, s.y - s.z);

    ivec3 grid_pos;
    grid_pos = clamp(g + ivec3(0, 0, 0), ivec3(0), probe_dim - 1);
    indices.x = get_index(grid_pos);
    grid_pos = clamp(g + ivec3(1, 1, 1), ivec3(0), probe_dim - 1);
    indices.y = get_index(grid_pos);
    grid_pos = clamp(g + vert2, ivec3(0), probe_dim - 1);
    indices.z = get_index(grid_pos);
    grid_pos = clamp(g + vert3, ivec3(0), probe_dim - 1);
    indices.w = get_index(grid_pos);
}

vec3
get_ambient(in vec3 pos, in vec3 norm)
{
    vec3 sh[9];
    for (int i = 0; i < 9; i++) {
        sh[i] = vec3(0.0);
    }

    pos -= probe_offset;
    pos = pos / probe_extents;
    pos = clamp(pos, 0.0, 1.0);
    pos *= vec3(probe_dim - 1);
    trilin_interp_sh(pos, sh);

    /*ivec4 probe_indices;
    vec4 weights;
    get_probe_indices(pos, probe_indices, weights);
    uncompress_sh(sh, probe_indices.x, weights.x);
    uncompress_sh(sh, probe_indices.y, weights.y);
    uncompress_sh(sh, probe_indices.z, weights.z);
    uncompress_sh(sh, probe_indices.w, weights.w);*/


    float dx = norm.x;
    float dy = norm.y;
    float dz = norm.z;
    vec3 ambient = vec3(0.0);
    ambient += c[0] * sh[0];
    ambient += c[1] * sh[1] * dy;
    ambient += c[1] * sh[2] * dz;
    ambient += c[1] * sh[3] * dx;
    ambient += c[2] * sh[4] * dy * dx;
    ambient += c[2] * sh[5] * dy * dz;
    ambient += c[3] * sh[6] * (3.0 * dz * dz - 1.0);
    ambient += c[2] * sh[7] * dx * dz;
    ambient += c[4] * sh[8] * (dx * dx - dy * dy);
    return ambient;
}

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
    vec3 diffuse = 1.0 * vec3((1.0 - shadow) * max(0.0, dot(light_dir, normal)));
    vec3 ambient = get_ambient(world_pos.xyz, normal);

    if (ambient_only) {
        frag_color = vec4(ambient, 1.0);
    } else {
        frag_color = vec4(vert_color.xyz * (diffuse + ambient), 1.0);
    }
}
