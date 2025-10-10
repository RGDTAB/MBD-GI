#include <iomanip>
#include <iostream>

#include "solver.h"

glm::vec4 Solver::unpack4(unsigned int r, float bv)
{
    const unsigned int m8 = 0xFF;
    glm::vec4 tmp = glm::vec4(r & m8, (r >> 8) & m8, (r >> 16) & m8, (r >> 24) & m8) / bv;
    tmp -= glm::vec4(1.0f);
    return tmp;
}

void Solver::uncomp_probe_data(unsigned int *bits, float dc, float *out)
{
    float bv = float(1 << 7) - 1.0f;
    float lc = dc * sqrt(3.0f);
    float qz = dc * sqrt(5.0f);
    float nz = dc * (sqrt(15.0f) / 2.0f);
    glm::vec4 shA = unpack4(bits[0], bv) * glm::vec4(lc, lc, lc, nz);
    glm::vec4 shB = unpack4(bits[1], bv) * glm::vec4(nz, qz, nz, nz);

    out[0] = dc;
    out[1] = shA.x;
    out[2] = shA.y;
    out[3] = shA.z;
    out[4] = shA.w;
    out[5] = shB.x;
    out[6] = shB.y;
    out[7] = shB.z;
    out[8] = shB.w;
}

void Solver::set_probe_data(CompSH *comp_sh, glm::ivec3 probe_res)
{
    probe_grid_res = probe_res;
    int probe_count = probe_grid_res.x * probe_grid_res.y * probe_grid_res.z;
    probe_data = new float[probe_count * DIMS];

    for (int i = 0; i < probe_count; i++) {
        unsigned int bits[2];
        float dc;
        float r[9], g[9], b[9];

        dc = comp_sh[i].dc.r;
        bits[0] = comp_sh[i].comp_bits[0].r;
        bits[1] = comp_sh[i].comp_bits[1].r;
        uncomp_probe_data(bits, dc, r);

        dc = comp_sh[i].dc.g;
        bits[0] = comp_sh[i].comp_bits[0].g;
        bits[1] = comp_sh[i].comp_bits[1].g;
        uncomp_probe_data(bits, dc, g);

        dc = comp_sh[i].dc.b;
        bits[0] = comp_sh[i].comp_bits[0].b;
        bits[1] = comp_sh[i].comp_bits[1].b;
        uncomp_probe_data(bits, dc, b);
        for (int j = 0; j < 9; j++) {
            probe_data[i * DIMS + j * 3 + 0] = r[j];
            means[j * 3 + 0] += probe_data[i * DIMS + j * 3 + 0];
            probe_data[i * DIMS + j * 3 + 1] = g[j];
            means[j * 3 + 1] += probe_data[i * DIMS + j * 3 + 1];
            probe_data[i * DIMS + j * 3 + 2] = b[j];
            means[j * 3 + 2] += probe_data[i * DIMS + j * 3 + 2];
        }
    }

    for (int k = 0; k < DIMS; k++) {
        means[k] /= float(probe_count);
    }
    for (int p = 0; p < probe_count; p++) {
        for (int k = 0; k < DIMS; k++) {
            probe_data[p * DIMS + k] -= means[k];
        }
    }


    /*std::cout << std::setprecision(3);
    for (int i = 0; i < probe_count; i++) {
        std::cout << "[";
        for (int j = 0; j < DIMS; j++) {
            std::cout << probe_data[i * DIMS + j] << ", ";
        }
        std::cout << "],\n";
    }*/
}

void Solver::set_raw_probe_data(float *data, glm::ivec3 probe_res)
{
    probe_grid_res = probe_res;
    unsigned int probe_count = probe_grid_res.x * probe_grid_res.y * probe_grid_res.z;
    probe_data = new float[probe_count * DIMS];
    std::memcpy(probe_data, data, probe_count * DIMS * sizeof(float));
    /*for (int i = 0; i < probe_count; i++) {
        std::cout << "[";
        for (int j = 0; j < DIMS; j++) {
            std::cout << probe_data[i * DIMS + j] << ", ";
        }
        std::cout << "],\n";
    }*/
}

void Solver::normalize(float *v)
{
    float sq_sum = 0.0f;
    for (int i = 0; i < DIMS; i++) {
        sq_sum += v[i] * v[i];
    }
    float e = sqrt(sq_sum);
    for (int i = 0; i < DIMS; i++) {
        v[i] /= e;
    }
}

float Solver::dot(float *a, float *b)
{
    float sum = 0.0f;
    for (int i = 0; i < DIMS; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

void Solver::proj(float *u, float *v, float *out)
{
    float m = dot(v, u) / dot(u, u);
    for (int i = 0; i < DIMS; i++) {
        out[i] = u[i] * m;
    }
}

void Solver::orthonormalize(float *r, int n_vectors)
{
    for (int u = 1; u < n_vectors; u++) {
        for (int v = 0; v < u; v++) {
            float tmp[DIMS];
            proj(r + v * DIMS, r + u * DIMS, tmp);

            for (int k = 0; k < DIMS; k++) {
                r[u * DIMS + k] -= tmp[k];
            }
        }
    }

    for (int i = 0; i < n_vectors; i++) {
        normalize(r + i * DIMS);
    }
}

