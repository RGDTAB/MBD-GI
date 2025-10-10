#version 430 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (binding = 0) uniform sampler2DArray front_cube;
layout (binding = 1) uniform sampler2DArray back_cube;

layout(std430, binding = 0) buffer valid_buffer {
    uint valid[];
};

layout(location = 0) uniform uint probe_index;

void
main()
{
    ivec3 sample_pos = ivec3(gl_GlobalInvocationID);
    ivec3 size = textureSize(front_cube, 0);
    if (sample_pos.x >= size.x || sample_pos.y >= size.y) {
        return;
    }

    float front_depth = texelFetch(front_cube, sample_pos, 0).r;
    float back_depth = texelFetch(back_cube, sample_pos, 0).r;

    // If the back face of some object is closer than the nearest front face
    // of another, then we are likely inside of the first object.
    if (front_depth > back_depth) {
        atomicMin(valid[probe_index], 0);
    }

}
