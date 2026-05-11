#pragma once

#include <glm/glm.hpp>

struct QuantSH {
    glm::ivec4 quant_bits[2];
    glm::vec4 dc;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

struct MBD {
    // Number of basis vectors per point
    // Also the dimensions of the coeffs
    int rank;
    // Resolution of the basis vector grid
    glm::ivec3 basis_res;
    // Resolution of the coeff vector grid
    // Same as the input probe grid
    glm::ivec3 coeff_res;

    // Basis vectors
    float *b;
    // Coefficients
    float *c;

    float means[27];
};
