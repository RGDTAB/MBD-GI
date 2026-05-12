#include "renderer.h"


void Renderer::update_mbd_buffers()
{
    if (!mbd_meta_buffer) {
        glGenBuffers(1, &mbd_meta_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mbd_meta_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 48 * sizeof(float), nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mbd_meta_buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 3 * sizeof(float), &probe_grid_extent);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 4  * sizeof(float), 3 * sizeof(float), &probe_grid_offset);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 8  * sizeof(float), 3 * sizeof(float), &mbd_data.basis_res);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 12 * sizeof(float), 3 * sizeof(float), &mbd_data.coeff_res);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 15 * sizeof(float), sizeof(float), &mbd_data.rank);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 16 * sizeof(float), 27 * sizeof(float), &mbd_data.means);

    if (mbd_basis_buffer) {
        glDeleteBuffers(1, &mbd_basis_buffer);
    }
    unsigned int basis_vector_count = mbd_data.basis_res.x * mbd_data.basis_res.y * mbd_data.basis_res.z;
    glGenBuffers(1, &mbd_basis_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mbd_basis_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, mbd_data.rank * basis_vector_count * 27 * sizeof(float), mbd_data.b, GL_STATIC_DRAW);

    if (mbd_coeff_buffer) {
        glDeleteBuffers(1, &mbd_coeff_buffer);
    }
    unsigned int coeff_vector_count = mbd_data.coeff_res.x * mbd_data.coeff_res.y * mbd_data.coeff_res.z;
    glGenBuffers(1, &mbd_coeff_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mbd_coeff_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, mbd_data.rank * coeff_vector_count * sizeof(float), mbd_data.c, GL_STATIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    mbd_dirty = false;
}
