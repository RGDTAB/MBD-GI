#include "renderer.h"

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
    const char draw_mode_labels[] = {
        "No GI\0Probe GI\0Indirect Only\0MBD GI",
    };
    ImGui::Combo("GI", &draw_mode, draw_mode_labels, 3);

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
