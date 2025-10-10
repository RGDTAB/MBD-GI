#version 430 core
out vec4 frag_color;

in vec3 vert_norm;
in vec4 vert_color;
in vec4 light_pos;
in vec4 world_pos;

layout (location = 3) uniform vec3 light_dir;
layout (binding = 0) uniform sampler2DShadow shadow_map;

layout(std430, binding = 0) restrict readonly buffer mbd_info_buffer {
    vec3 mbd_extents;
    vec3 mbd_offset;
    ivec3 basis_res;
    ivec3 coeff_res;
    int mbd_rank;
    float means[27];
};

layout(std430, binding = 1) restrict readonly buffer mbd_basis_buffer {
    float mbd_b[];
};

layout(std430, binding = 2) restrict readonly buffer mbd_coeff_buffer {
    float mbd_c[];
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

int
get_index(in ivec3 grid_pos, in ivec3 grid_dim)
{
    return grid_pos.x + grid_pos.y * grid_dim.x + grid_pos.z * grid_dim.x * grid_dim.y;
}

void
trilin_interp_c(vec3 p, inout float c[6])
{
    ivec3 g = ivec3(p);
    p = p - trunc(p);
    ivec3 o;
    vec3 d;
    float w;
    int c_pos;

    for (int l = 0; l < mbd_rank; l++) {
        c[l] = 0.0f;
    }

    for (int i = 0; i < 8; i++) {
        o = g + trilin_offsets[i];
        bool valid = all(lessThanEqual(o, coeff_res - 1));
        o = min(o, coeff_res - 1);
        c_pos = get_index(o, coeff_res) * mbd_rank;

        d = vec3(1.0f) - p - vec3(trilin_offsets[i]);
        w = abs(d.x * d.y * d.z);

        for (int l = 0; l < mbd_rank; l++) {
            c[l] = valid ? c[l] + w * mbd_c[c_pos + l] : c[l];
        }
    }

    //c_pos = get_index(g, coeff_res) * mbd_rank;
    /*c_pos = 0;
    for (int l = 0; l < mbd_rank; l++) {
        c[l] = mbd_c[c_pos + l];
    }*/
}

void
trilin_interp_b(vec3 p, inout vec3 b[9 * 6])
{
    ivec3 g = ivec3(p);
    p = p - trunc(p);
    ivec3 o;
    vec3 d;
    float w;
    int b_pos;

    for (int l = 0; l < mbd_rank; l++) {
        for (int k = 0; k < 9; k++) {
            b[l * 9 + k] = vec3(0.0f);
        }
    }

    for (int i = 0; i < 8; i++) {
        o = g + trilin_offsets[i];
        bool valid = all(lessThanEqual(o, basis_res - 1));
        o = min(o, basis_res - 1);
        b_pos = get_index(o, basis_res) * mbd_rank * 27;

        d = vec3(1.0f) - p - vec3(trilin_offsets[i]);
        w = abs(d.x * d.y * d.z);

        for (int l = 0; l < mbd_rank; l++) {
            for (int k = 0; k < 9; k++) {
                b[l * 9 + k].r = valid ? b[l * 9 + k].r + w * mbd_b[b_pos + l * 27 + k * 3 + 0] : b[l * 9 + k].r;
                b[l * 9 + k].g = valid ? b[l * 9 + k].g + w * mbd_b[b_pos + l * 27 + k * 3 + 1] : b[l * 9 + k].g;
                b[l * 9 + k].b = valid ? b[l * 9 + k].b + w * mbd_b[b_pos + l * 27 + k * 3 + 2] : b[l * 9 + k].b;
            }
        }
    }

    //b_pos = get_index(g, basis_res) * mbd_rank * 27;
    /*b_pos = 0;
    for (int l = 0; l < mbd_rank; l++) {
        for (int k = 0; k < 9; k++) {
            b[l * 9 + k].r = mbd_b[b_pos + l * 27 + k * 3 + 0];
            b[l * 9 + k].g = mbd_b[b_pos + l * 27 + k * 3 + 1];
            b[l * 9 + k].b = mbd_b[b_pos + l * 27 + k * 3 + 2];
        }
    }*/
}

void
eval_mbd(in vec3 pos, inout vec3 sh[9])
{
    float c[6];
    vec3 b[9 * 6];
    pos -= mbd_offset;
    pos = clamp(pos / mbd_extents, 0.0f, 1.0f);
    vec3 b_pos = pos * vec3(basis_res - 1);
    vec3 c_pos = pos * vec3(coeff_res - 1);

    trilin_interp_c(c_pos, c);
    trilin_interp_b(b_pos, b);

    for (int i = 0; i < 9; i++) {
        sh[i].r = means[i * 3 + 0];
        sh[i].g = means[i * 3 + 1];
        sh[i].b = means[i * 3 + 2];
        //sh[i] = vec3(0.0f);
    }
    for (int l = 0; l < mbd_rank; l++) {
        for (int k = 0; k < 9; k++) {
            sh[k] += b[l * 9 + k] * c[l];
        }
    }
}

vec3
get_ambient(in vec3 pos, in vec3 norm)
{
    vec3 sh[9];

    eval_mbd(pos, sh);

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

    //frag_color = vec4(vert_color.xyz * (diffuse + ambient), 1.0);
    frag_color = vec4(ambient, 1.0);
}
