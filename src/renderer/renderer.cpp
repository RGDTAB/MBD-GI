#include <chrono>
#include <iostream>

#include "renderer.h"

void Renderer::init()
{
    // Init glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

    window = glfwCreateWindow(800, 800, "Learn OpenGL", NULL, NULL);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    glViewport(0, 0, 800, 800);
    width = height = 800;

    // Create an HDR color buffer
    glGenTextures(1, &hdr_color_buffer);
    glBindTexture(GL_TEXTURE_2D, hdr_color_buffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create a primary depth buffer
    glGenRenderbuffers(1, &depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 4096, 4096);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create the necessary shader programs
    draw_no_gi_program = compile_shader_program("src/renderer/shaders/draw.vs", "src/renderer/shaders/draw_no_gi.fs");
    draw_gi_program = compile_shader_program("src/renderer/shaders/draw.vs", "src/renderer/shaders/draw_gi.fs");
    draw_mbd_program = compile_shader_program("src/renderer/shaders/draw.vs", "src/renderer/shaders/draw_mbd.fs");
    shadow_program = compile_shader_program("src/renderer/shaders/shadow.vs", "src/renderer/shaders/shadow.fs");
    tonemap_program = compile_shader_program("src/renderer/shaders/tonemap.vs", "src/renderer/shaders/tonemap.fs");

    // Create the vbo and vao for the Cornell box
    create_vertex_buffers("assets/CornellBox.obj", "assets/", cornell_box_vbo, cornell_box_vao, cornell_box_vertex_count);

    // Create a fullscreen tri for tonemapping
    const float tri_verts[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f, 3.0f, 0.0f, 2.0f,
        3.0f, -1.0f, 2.0f, 0.0f,
    };

    glGenBuffers(1, &fullscreen_tri_vbo);
    glGenVertexArrays(1, &fullscreen_tri_vao);

    glBindVertexArray(fullscreen_tri_vao);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreen_tri_vbo);
    glBufferData(GL_ARRAY_BUFFER, (3 * 4) * sizeof(float), tri_verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenFramebuffers(1, &fbo);

    float sPhi = sin(light_angles.x);
    float cPhi = cos(light_angles.x);
    float sTheta = sin(light_angles.y);
    float cTheta = cos(light_angles.y);
    light_dir = glm::vec3(sTheta * cPhi, cTheta, sTheta * sPhi);

    // Create a shadow map for the scene
    generate_shadow_map(4096, &shadow_fbo, shadow_map_tex);
    draw_shadow_maps();

    camera_pos = glm::vec3(-0.278f, 0.273f, 0.80f);
    camera_dir = glm::vec3(0.0f, 0.0f, -1.0f);
    init_probes();
    place_probes(glm::vec3(0.51f, 0.51f, 0.52f), glm::vec3(-0.53f, 0.02f, -0.54f));

    init_imgui();
}

// Draws the scene from the lights perspective
void Renderer::draw_shadow_maps()
{
    glViewport(0, 0, 4096, 4096);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_map_tex, 0);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glEnable(GL_DEPTH_TEST);

    glm::mat4 model = glm::scale(glm::mat4(1.0), glm::vec3(0.01));

    glm::vec3 light_pos = light_dir * 3.0f;
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(light_pos, glm::vec3(0.0f), up);

    glm::mat4 projection = glm::mat4(1.0f);
    projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 5.0f);

    shadow_matrix = projection * view;

    glUseProgram(shadow_program);
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(shadow_matrix));
    glBindVertexArray(cornell_box_vao);
    glDrawArrays(GL_TRIANGLES, 0, cornell_box_vertex_count);

    glBindVertexArray(0);
    glUseProgram(0);
}

// Draws the cornell box using the current lighting settings
void Renderer::draw_cornell_box()
{
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_DEPTH_TEST);

    glm::mat4 model = glm::scale(glm::mat4(1.0), glm::vec3(0.01));

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

    glm::mat4 vp = projection * view;


    switch (draw_mode) {
        case DRAW_NO_GI:
            glUseProgram(draw_no_gi_program);
            break;
        case DRAW_PROBE_GI:
            glUseProgram(draw_gi_program);
            glUniform1i(4, indirect_only);
            break;
        case DRAW_MBD_GI:
            glUseProgram(draw_mbd_program);
            glUniform1i(4, indirect_only);
            break;
    }

    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(vp));
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(shadow_matrix));
    glUniform3fv(3, 1, glm::value_ptr(light_dir));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow_map_tex);

    switch (draw_mode) {
        case DRAW_NO_GI:
            break;
        case DRAW_PROBE_GI:
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, probe_data_buffer);
            break;
        case DRAW_MBD_GI:
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mbd_meta_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mbd_basis_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mbd_coeff_buffer);
            break;
    }

    glBindVertexArray(cornell_box_vao);

    glDrawArrays(GL_TRIANGLES, 0, cornell_box_vertex_count);

    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::tonemap()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(tonemap_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_color_buffer);

    glBindVertexArray(fullscreen_tri_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glUseProgram(0);
}

// Draw the scene
void Renderer::draw()
{
    glfwPollEvents();

    if (should_gather_probes && probe_bounce_count < max_probe_bounce) {
        capture_probes();
        probe_bounce_count++;

        glFinish();
    }

    if (mbd_dirty) {
        update_mbd_buffers();
    }

    int w_width, w_height;
    glfwGetWindowSize(window, &w_width, &w_height);

    if (width != w_width && height != w_height) {
        width = w_width;
        height = w_height;

        glBindTexture(GL_TEXTURE_2D, hdr_color_buffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdr_color_buffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (should_render_cornell_box) {
        draw_cornell_box();
    }
    if (should_render_probes) {
        draw_probes();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    tonemap();
    draw_imgui_window();

    glfwSwapBuffers(window);
}

// Cleanup buffers
void Renderer::cleanup()
{
    glDeleteVertexArrays(1, &cornell_box_vao);
    glDeleteBuffers(1, &cornell_box_vbo);
    glDeleteProgram(draw_no_gi_program);

    cleanup_imgui();

    glfwDestroyWindow(window);
    glfwTerminate();
}
