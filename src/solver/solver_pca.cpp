#include <iomanip>
#include <iostream>
#include <cstring>

#include "solver.h"

float *Solver::compute_cov()
{
    int probe_count = probe_grid_res.x * probe_grid_res.y * probe_grid_res.z;
    float *cov = new float[DIMS * DIMS];
    for (int i = 0; i < DIMS; i++) {
        for (int j = i; j < DIMS; j++) {
            cov[i * DIMS + j] = 0.0f;
            for (int p = 0; p < probe_count; p++) {
                cov[i * DIMS + j] += probe_data[p * DIMS + j] * probe_data[p * DIMS + i];
            }
            cov[j * DIMS + i] = cov[i * DIMS + j];
        }
    }

    return cov;
}

float *Solver::compute_pca(int n_components)
{
    if (probe_data == nullptr) {
        return nullptr;
    }

    unsigned int probe_count = probe_grid_res.x * probe_grid_res.y * probe_grid_res.z;
    float *cov = compute_cov();

    float *r = new float[DIMS * n_components];
    for (int k = 0; k < DIMS * n_components; k++) {
        r[k] = rand_float();
    }
    orthonormalize(r, n_components);

    float *s = new float[DIMS * n_components];

    const int num_iterations = 100;
    float error = 0.0f;
    for (int i = 0; i < num_iterations; i++) {
        for (int j = 0; j < DIMS * n_components; j++) {
            s[j] = 0.0f;
        };

        for (int k = 0; k < DIMS; k++) {
            for (int c = 0; c < n_components; c++) {
                s[c * DIMS + k] = dot(cov + k * DIMS, r + c * DIMS);
            }
        }

        float lambda[n_components];
        error = 0.0f;
        for (int c = 0; c < n_components; c++) {
            lambda[c] = dot(r + c * DIMS, s + c * DIMS);
            for (int k = 0; k < DIMS; k++) {
                error += std::abs(lambda[c] * r[c * DIMS + k] - s[c * DIMS + k]);
            }
        }
        error /= n_components;

        std::memcpy(r, s, DIMS * n_components * sizeof(float));
        orthonormalize(r, n_components);

        if (error < 0.0001f) {
            break;
        }
    }
    std::cout << std::setprecision(2);
    for (int i = 0; i < n_components; i++) {
        std::cout << "[";
        for (int k = 0; k < DIMS; k++) {
            std::cout << r[i * DIMS + k] << ",";
        }
        std::cout << "]\n";
    }

    delete[] cov;
    delete[] s;

    return r;
}
