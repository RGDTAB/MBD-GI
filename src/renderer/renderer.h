#pragma once

#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "data_types.h"

class Renderer {
    private:

    enum DrawModes {DRAW_NO_GI, DRAW_PROBE_GI, DRAW_MBD_GI};

    GLFWwindow *window;

    int width, height;
    unsigned int tonemap_program;
    unsigned int fullscreen_tri_vbo;
    unsigned int fullscreen_tri_vao;
    unsigned int fbo;
    unsigned int hdr_color_buffer;
    unsigned int depth_rbo;

    unsigned int draw_no_gi_program;
    unsigned int draw_gi_program;
    unsigned int draw_mbd_program;
    unsigned int cornell_box_vertex_count;
    unsigned int cornell_box_vbo;
    unsigned int cornell_box_vao;

    unsigned int shadow_program;
    unsigned int shadow_map_tex;
    unsigned int shadow_fbo;

    struct Probe {
        unsigned int index;
        glm::vec3 world_pos;
        glm::ivec3 local_pos;
        bool valid;
    };

    glm::ivec3 probe_grid_res = glm::ivec3(16);
    glm::vec3 probe_grid_extent;
    glm::vec3 probe_grid_offset;

    std::vector<Probe> probes;
    unsigned int probe_draw_program;
    unsigned int probe_eval_program;
    unsigned int probe_comp_program;
    unsigned int probe_box_vbo;
    unsigned int probe_box_vao;
    unsigned int probe_vbo;
    unsigned int probe_vao;
    unsigned int probe_data_buffer = 0;
    unsigned int probe_temp_buffer = 0;
    unsigned int max_probe_bounce = 10;
    unsigned int probe_bounce_count = 0;
    unsigned int probe_color_buffer;
    unsigned int probe_depth_rbo;

    MBD mbd_data = {
        .rank = 4,
        .basis_res = glm::ivec3(3),
        .coeff_res = glm::ivec3(16),
        .b = nullptr,
        .c = nullptr,
    };

    bool mbd_dirty = false;
    unsigned int mbd_meta_buffer = 0;
    unsigned int mbd_basis_buffer = 0;
    unsigned int mbd_coeff_buffer = 0;

    bool should_render_cornell_box = true;
    bool should_render_probes = false;
    int draw_mode = DRAW_PROBE_GI;
    bool indirect_only = false;
    bool should_gather_probes = true;
    bool _should_solve_mbd = false;

    bool mbd_collected = false;

    glm::mat4 shadow_matrix;

    glm::vec3 camera_pos;
    glm::vec3 camera_dir;
    glm::vec2 light_angles = glm::vec2(2.67f, 1.38f);
    glm::vec3 light_dir;
    bool orthogonal = false;


    void create_vertex_buffers(const char *obj, const char *mtl_dir, unsigned int &vbo, unsigned int &vao, unsigned int &vertex_count);

    void generate_shadow_map(unsigned int resolution, unsigned int *fbo, unsigned int &tex);

    void draw_shadow_maps();
    void draw_cornell_box();
    void draw_probes();
    void draw_imgui_window();

    void tonemap();

    void init_probes();
    bool check_probe(glm::vec3 pos);
    void place_probes(glm::vec3 extents, glm::vec3 offset);

    void capture_probes();
    void clear_probe_data();

    void update_mbd_buffers();

    public:
    int mbd_iter = 64;

    void init();
    void init_imgui();

    void draw();

    void cleanup();
    void cleanup_imgui();

    bool should_close()
    {
        return glfwWindowShouldClose(window);
    }
    bool should_solve_mbd()
    {
        return _should_solve_mbd && probe_bounce_count == max_probe_bounce;
    }

    unsigned int compile_shader_program(const char *vert, const char *frag);
    unsigned int compile_compute_shader(const char *comp);

    QuantSH *get_probe_data();

    MBD get_mbd() {
        return mbd_data;
    }

    void set_mbd(MBD &mbd)
    {
        mbd_data = mbd;
        mbd_dirty = true;
        mbd_collected = true;
        _should_solve_mbd = false;
    }
};
