#include <chrono>
#include <iostream>
#include <vector>

#include "renderer.h"

void Renderer::init_probes()
{
    probe_draw_program = compile_shader_program("src/renderer/shaders/probe.vs", "src/renderer/shaders/probe.fs");
    probe_eval_program = compile_compute_shader("src/renderer/shaders/compute/probe_eval.cs");
    probe_comp_program = compile_compute_shader("src/renderer/shaders/compute/probe_comp.cs");

    // Create a vbo and vao to represent a simple bounding box for the probes
    const int probe_box_vertex_count = 36;
    glm::vec3 probe_box_vertices[probe_box_vertex_count] = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),

        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),

        glm::vec3(0.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f),

        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),

        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),

        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
    };

    glGenBuffers(1, &probe_box_vbo);
    glGenVertexArrays(1, &probe_box_vao);

    glBindVertexArray(probe_box_vao);
    glBindBuffer(GL_ARRAY_BUFFER, probe_box_vbo);
    glBufferData(GL_ARRAY_BUFFER, probe_box_vertex_count * sizeof(glm::vec3), probe_box_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Create the renderbuffers used when gathering the probes
    glGenTextures(1, &probe_color_buffer);
    glBindTexture(GL_TEXTURE_2D, probe_color_buffer);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, 4096, 4096);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &probe_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, probe_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 4096, 4096);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create a temporary buffer to store sh coeffs before quantizing them
    glGenBuffers(1, &probe_temp_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, probe_temp_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (4 * 9 * 4096) * sizeof(float), nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// TODO: check to make sure the probe isn't inside of the mesh
bool Renderer::check_probe(glm::vec3 pos)
{
    return true;
}

void Renderer::place_probes(glm::vec3 extents, glm::vec3 offset, glm::ivec3 res)
{
    probe_grid_offset = offset;
    probe_grid_res = res;
    probe_grid_extent = extents;

    std::vector<glm::vec3> probe_coords;
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    for (int z = 0; z < res.z; z++) {
        for (int y = 0; y < res.y; y++) {
            for (int x = 0; x < res.x; x++) {
                glm::vec3 pos = glm::vec3((extents.x / (float) (res.x - 1)) * x + offset.x,
                        (extents.y / (float) (res.y - 1)) * y + offset.y,
                        (extents.z / (float) (res.z - 1)) * z + offset.z);
                glm::ivec3 local_pos = glm::ivec3(x, y, z);
                unsigned int index = x + y * res.x + z * res.x * res.y;
                bool is_valid = check_probe(pos);

                Probe new_probe = {.index = index, .world_pos = pos, .local_pos = local_pos, .valid = is_valid};
                probes.push_back(new_probe);
                probe_coords.push_back(pos);
            }
        }
    }

    std::vector<unsigned int> valid_list;
    valid_list.resize(probes.size(), 0);

    // Create GL buffers for probe data
    glGenBuffers(1, &probe_vbo);
    glGenVertexArrays(1, &probe_vao);

    glBindVertexArray(probe_vao);
    glBindBuffer(GL_ARRAY_BUFFER, probe_vbo);
    glBufferData(GL_ARRAY_BUFFER, probe_coords.size() * sizeof(glm::vec3), probe_coords.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenBuffers(1, &probe_data_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, probe_data_buffer);
    // Using quantization, SH3 for RGB can fit within 48 bytes or less
    glBufferData(GL_SHADER_STORAGE_BUFFER, (probe_grid_res.x * probe_grid_res.y * probe_grid_res.z * 12 + 12) * sizeof(float), nullptr, GL_STATIC_DRAW);
    unsigned int val = 0;
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &val);

    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 12, &probe_grid_res);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 16, 12, &probe_grid_extent);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 32, 12, &probe_grid_offset);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Renderer::capture_probes()
{
    // Create model and projection matrices, and bind render buffers
    glm::vec3 target_vectors[] = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
    };

    glm::vec3 up_vectors[] = {
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
    };

    glm::mat4 model = glm::scale(glm::mat4(1.0), glm::vec3(0.01));
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 3.0f);
    glm::vec3 light_dir = glm::normalize(glm::vec3(-1.7213f, 0.5176f, 0.87704f));

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, probe_depth_rbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, probe_color_buffer, 0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    /* The render buffer is shared between probes, and their render data is
     * converted to irradiance SH in batches. This is done as it's
     * considerably faster than going through the expensive context switching
     * at each draw.
     *
     * Pseudocode:
     * while probes remaining:
     *   for each cubemap face:
     *     for each probe in the batch:
     *       draw cornell box;
     *     convert batch to irradiance SH
     *   quantize SH and store in the final buffer
     */
    int length = probes.size();
    int width = 4096 / 64;
    int area = width * width;
    int compress_groups = area / 64;
    int probe_index = 0;
    for (int probe_index = 0; probe_index < length; probe_index += area) {
        int diff = length - probe_index;

        unsigned int val = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, probe_temp_buffer);
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &val);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        for (int face = 0; face < 6; face++) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(draw_gi_program);

            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(shadow_matrix));
            glUniform3fv(3, 1, glm::value_ptr(light_dir));
            glUniform1i(4, false);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, probe_data_buffer);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, shadow_map_tex);

            glBindVertexArray(cornell_box_vao);
            int p_id = 0;
            for (p_id = 0; p_id < area && probe_index + p_id < length; p_id++) {
                int ind = probe_index + p_id;
                glViewport(64 * (p_id % width), 64 * (p_id / width), 64, 64);
                glm::vec3 pos = probes[ind].world_pos;
                glm::mat4 view = glm::mat4(1.0f);
                view = glm::lookAt(pos, pos + target_vectors[face], up_vectors[face]);
                glm::mat4 vp = projection * view;
                glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(vp));

                glDrawArrays(GL_TRIANGLES, 0, cornell_box_vertex_count);
            }
            glUseProgram(probe_eval_program);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, probe_color_buffer);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, probe_temp_buffer);
            glUniform1i(0, face);
            glUniform1ui(1, diff);

            glDispatchCompute(width, (p_id - 1) / width + 1, 1);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        }
        glUseProgram(probe_comp_program);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, probe_temp_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, probe_data_buffer);
        glUniform1ui(0, diff);
        glUniform1ui(1, probe_index);

        glDispatchCompute(compress_groups, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glUseProgram(0);
}

QuantSH *
Renderer::get_probe_data()
{
    QuantSH *data = new QuantSH[probe_grid_res.x * probe_grid_res.y * probe_grid_res.z];
    if (data == nullptr) {
        return data;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, probe_data_buffer);

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 12 * sizeof(float), (probe_grid_res.x * probe_grid_res.y * probe_grid_res.z * 12) * sizeof(float), data);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return data;
}

void Renderer::draw_probes()
{
    glEnable(GL_DEPTH_TEST);
    glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 view = glm::mat4(1.0f);
    glm::vec3 offset = camera_pos + camera_dir;
    if (camera_dir.y == -1.0f) {
        view = glm::lookAt(camera_pos, offset, glm::vec3(0.0f, 0.0f, -1.0f));
    } else {
        view = glm::lookAt(camera_pos, offset, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 projection = glm::mat4(1.0f);
    if (!orthogonal) {
        projection = glm::perspective(glm::radians(39.0f), (float) width / (float) height, 0.01f, 5.0f);
    } else {
        projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.01f, 5.0f);
    }

    glPointSize(2.0f);

    glUseProgram(probe_draw_program);
    glm::mat4 mvp = projection * view * model;

    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(probe_vao);
    glDrawArrays(GL_POINTS, 0, probes.size());

    model = glm::mat4(1.0f);
    model = glm::translate(model, probe_grid_offset);
    model = glm::scale(model, probe_grid_extent);
    mvp = projection * view * model;
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(probe_box_vao);
    glDrawArrays(GL_LINES, 0, 36);

    glBindVertexArray(0);
    glUseProgram(0);
}
