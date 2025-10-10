#include <cstdio>
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

#include "renderer.h"

unsigned int Renderer::compile_shader_program(const char *vert, const char *frag)
{
    int success;
    char info_log[512];
    std::FILE *fp;
    long len;
    char *shader_text;

    fp = std::fopen(vert, "r");
    std::fseek(fp, 0, SEEK_END);
    len = std::ftell(fp);
    shader_text = new char[len + 1];
    std::fseek(fp, 0, SEEK_SET);
    len = std::fread(shader_text, 1, len, fp);
    std::fclose(fp);
    shader_text[len] = '\0';

    unsigned int vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &shader_text, NULL);
    glCompileShader(vert_shader);
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vert_shader, 512, NULL, info_log);
        std::cout << "VERT SHADER COMPILE INFO\n" << info_log << std::endl;
    }
    delete[] shader_text;

    fp = std::fopen(frag, "r");
    std::fseek(fp, 0, SEEK_END);
    len = std::ftell(fp);
    shader_text = new char[len + 1];
    std::fseek(fp, 0, SEEK_SET);
    len = std::fread(shader_text, 1, len, fp);
    std::fclose(fp);
    shader_text[len] = '\0';

    unsigned int frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &shader_text, NULL);
    glCompileShader(frag_shader);
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(frag_shader, 512, NULL, info_log);
        std::cout << "FRAG SHADER COMPILE INFO\n" << info_log << std::endl;
    }
    delete[] shader_text;

    unsigned int shader_program = glCreateProgram();
    glAttachShader(shader_program, vert_shader);
    glAttachShader(shader_program, frag_shader);
    glLinkProgram(shader_program);

    int log_len;
    glGetProgramInfoLog(shader_program, 512, &log_len, info_log);
    if (log_len > 0) {
        std::cout << "LINKER INFO: \n" << info_log << std::endl;
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return shader_program;
}

unsigned int Renderer::compile_compute_shader(const char *comp)
{
    int success;
    char info_log[512];
    std::FILE *fp;
    long len;
    char *shader_text;

    fp = std::fopen(comp, "r");
    std::fseek(fp, 0, SEEK_END);
    len = std::ftell(fp);
    shader_text = new char[len + 1];
    std::fseek(fp, 0, SEEK_SET);
    len = std::fread(shader_text, 1, len, fp);
    std::fclose(fp);
    shader_text[len] = '\0';

    unsigned int comp_shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(comp_shader, 1, &shader_text, NULL);
    glCompileShader(comp_shader);
    glGetShaderiv(comp_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(comp_shader, 512, NULL, info_log);
        std::cout << "COMP SHADER COMPILE FAIL\n" << info_log << std::endl;
    }
    delete[] shader_text;

    unsigned int shader_program = glCreateProgram();
    glAttachShader(shader_program, comp_shader);
    glLinkProgram(shader_program);

    glDeleteShader(comp_shader);
    return shader_program;
}

void Renderer::create_vertex_buffers(const char *obj, const char *mtl_dir, unsigned int &vbo, unsigned int &vao, unsigned int &vertex_count)
{
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warnings;
    std::string errors;
    tinyobj::LoadObj(&attributes, &shapes, &materials, &warnings, &errors, obj, mtl_dir);

    std::vector<Vertex> verts;
    for (auto shape : shapes) {
        tinyobj::mesh_t mesh = shape.mesh;
        for (int i = 0; i < mesh.indices.size(); i++) {
            int vertex_index = mesh.indices[i].vertex_index;
            glm::vec3 position = {
                attributes.vertices[vertex_index * 3],
                attributes.vertices[vertex_index * 3 + 1],
                attributes.vertices[vertex_index * 3 + 2],
            };

            int normal_index = mesh.indices[i].normal_index;
            glm::vec3 normal = {
                attributes.normals[normal_index * 3],
                attributes.normals[normal_index * 3 + 1],
                attributes.normals[normal_index * 3 + 2],
            };

            int mat_index = mesh.material_ids[i / 3];
            tinyobj::material_t mat = materials[mat_index];
            glm::vec3 color = {
                mat.diffuse[0],
                mat.diffuse[1],
                mat.diffuse[2],
            };

            Vertex vert = { position, normal, color };
            verts.push_back(vert);
        }
    }
    vertex_count = verts.size();
    unsigned int size = vertex_count * sizeof(Vertex);

    glGenBuffers(1, &vbo);
    glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, size, verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *) 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *) (3 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *) (6 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::generate_shadow_map(unsigned int resolution, unsigned int *fbo, unsigned int &tex)
{
    if (fbo != nullptr) {
        glGenFramebuffers(1, fbo);
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);
    float border_color[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
    glBindTexture(GL_TEXTURE_2D, 0);
}
