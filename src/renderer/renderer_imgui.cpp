#include "renderer.h"

#include <cmath>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>


void Renderer::init_imgui()
{
    const char *glsl_version = "#version 430 core";
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void Renderer::draw_imgui_window()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("MBD");

    ImGui::SeparatorText("Scene Settings");
    ImGui::Checkbox("Render Cornell Box", &should_render_cornell_box);
    ImGui::Checkbox("Render Probe Locations", &should_render_probes);
    ImGui::Checkbox("Orthogonal", &orthogonal);

    if (ImGui::Button("Front View")) {
        camera_pos = glm::vec3(-0.278f, 0.273f, 0.80f);
        camera_dir = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    if (ImGui::Button("Top View")) {
        camera_pos = glm::vec3(-0.278, 2.0f, -0.50f);
        camera_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    }

    glm::vec2 new_light_angles = light_angles;
    ImGui::SliderFloat2("Light Angles", glm::value_ptr(new_light_angles), 0.0f, 6.2831f);
    if (new_light_angles != light_angles) {
        light_angles = new_light_angles;
        float sPhi = sin(light_angles.x);
        float cPhi = cos(light_angles.x);
        float sTheta = sin(light_angles.y);
        float cTheta = cos(light_angles.y);
        light_dir = glm::normalize(glm::vec3(sTheta * cPhi, cTheta, sTheta * sPhi));

        clear_probe_data();
        draw_shadow_maps();
    }

    ImGui::SeparatorText("GI Settings");
    const char draw_mode_labels[] = {
        "No GI\0Probe GI\0MBD GI",
    };
    ImGui::Combo("GI", &draw_mode, draw_mode_labels, 3);
    ImGui::Checkbox("Indirect Only", &indirect_only);
    ImGui::Checkbox("Gather Probes", &should_gather_probes);

    if (probe_bounce_count != max_probe_bounce) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Solve MBD")) {
        _should_solve_mbd = true;

        if (mbd_data.b) {
            delete[] mbd_data.b;
            mbd_data.b = nullptr;
        }
        if (mbd_data.c) {
            delete[] mbd_data.c;
            mbd_data.c = nullptr;
        }
    }
    if (probe_bounce_count != max_probe_bounce) {
        ImGui::EndDisabled();
    }


    if (ImGui::CollapsingHeader("Probe Properties", ImGuiTreeNodeFlags_None)) {
        glm::ivec3 next_probe_res = probe_grid_res;
        ImGui::InputInt3("Probe Grid Resolution", glm::value_ptr(next_probe_res));
        if (next_probe_res != probe_grid_res) {
            probe_grid_res = next_probe_res;
            mbd_data.coeff_res = next_probe_res;
            place_probes(glm::vec3(0.51f, 0.51f, 0.52f), glm::vec3(-0.53f, 0.02f, -0.54f));
        }

        int next_max = max_probe_bounce;
        ImGui::InputInt("# of Light Bounces", &next_max);
        if (next_max != max_probe_bounce) {
            if (next_max < max_probe_bounce) {
                clear_probe_data();
            }

            max_probe_bounce = next_max;
        }
    }

    if (ImGui::CollapsingHeader("MBD Settings", ImGuiTreeNodeFlags_None)) {
        ImGui::InputInt("Iterations", &mbd_iter);
        ImGui::InputInt("Rank", &mbd_data.rank);

        ImGui::InputInt3("Basis Grid Resolution", glm::value_ptr(mbd_data.basis_res));
    }

    ImGui::End();

    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::cleanup_imgui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
