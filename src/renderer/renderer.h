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
    GLFWwindow *window;

    int width, height;
    unsigned int hdr_color_buffer;
    unsigned int tonemap_program;
    unsigned int fullscreen_tri_vbo;
    unsigned int fullscreen_tri_vao;
    unsigned int fbo;

    unsigned int draw_no_gi_program;
    unsigned int draw_gi_program;
    unsigned int draw_mbd_program;
    unsigned int cornell_box_vertex_count;
    unsigned int cornell_box_vbo;
    unsigned int cornell_box_vao;

    unsigned int shadow_program;
    unsigned int shadow_map_tex;

    struct Probe {
        unsigned int index;
        glm::vec3 world_pos;
        glm::ivec3 local_pos;
        bool valid;
    };

    glm::ivec3 probe_grid_res;
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
    unsigned int probe_bounce_count = 0;
    unsigned int probe_color_buffer;
    unsigned int probe_depth_rbo;

    MBD mbd_data;
    bool mbd_dirty = false;
    unsigned int mbd_meta_buffer = 0;
    unsigned int mbd_basis_buffer = 0;
    unsigned int mbd_coeff_buffer = 0;

    bool should_render_cornell_box = true;
    bool should_render_probes = false;
    int draw_mode = 0;
    bool mbd_collected = false;

    glm::mat4 shadow_matrix;

    glm::vec3 camera_pos;
    glm::vec3 camera_dir;
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
    void place_probes(glm::vec3 extents, glm::vec3 offset, glm::ivec3 res);

    void capture_probes();

    void update_mbd_buffers();

    public:
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
        return draw_mode == 3 && !mbd_collected;
    }

    unsigned int compile_shader_program(const char *vert, const char *frag);
    unsigned int compile_compute_shader(const char *comp);

    QuantSH *get_probe_data();
    int get_probe_count()
    {
        return probe_grid_res.x * probe_grid_res.y * probe_grid_res.z;
    }
    glm::ivec3 get_grid_res()
    {
        return probe_grid_res;
    }

    void set_mbd(MBD &mbd)
    {
        mbd_data = mbd;
        mbd_dirty = true;
        mbd_collected = true;
    }
};
