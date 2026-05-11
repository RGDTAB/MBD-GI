#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdlib>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <mutex>

#include "data_types.h"

#define DIMS 27
#define LAMBDA 0.0001f

class Solver {
    private:
        // Probe data is centered
        float *probe_data = nullptr;
        float means[DIMS] = { 0.0f };
        glm::ivec3 probe_grid_res = glm::ivec3(0);

        int n_threads = 0;
        int bv_per_thread = 0;
        int cv_per_thread = 0;

        std::mutex gradient_mutex;
        std::mutex error_mutex;

        std::atomic<int> pending;
        bool main_ready[2] = { false };
        bool finished = false;
        bool backtracking_finished = false;

        float error = 0.0f;
        float m = 0.0f;
        float step_size = 1.0f;

        std::condition_variable thread_cv;
        std::mutex *thread_mutexes;
        std::condition_variable main_cv;
        std::mutex main_mutex;

        // Basis vector gradients
        float *g_b = nullptr;
        // Basis vector diagonal hessian
        float *h_b = nullptr;
        // Coefficient vector gradients
        float *g_c = nullptr;
        // Coefficient vector diagonal hessian
        float *h_c = nullptr;

        // Locations where stochastic samples are taken from
        glm::vec3 *samples = nullptr;

        glm::vec4 unpack4(unsigned int r, float bv);
        void unquant_probe_data(unsigned int *bits, float dc, float *out);

        float *compute_cov();

        glm::vec3 rand_sample(glm::ivec3 pos);
        void generate_random_samples(MBD &mbd);
        void trilin_interp(glm::vec3 p, glm::ivec3 res, int len, float *in, float *out);


        void interp_probe_data(glm::vec3 p, float *out);
        void eval_mbd(MBD &mbd, glm::vec3 p, float *b, float *c, float *out);

        void estimate_gradients(MBD &mbd, glm::vec3 p, float *b, float *c, float *r);

        float get_mbd_error(MBD &mbd, bool compute_gradients, bool reuse_samples);
        float newton_step(MBD &mbd, float step_size = 1.0f);

        void fit_coeffs_to_pca(MBD &mbd);

        float get_mbd_error(MBD &mbd, bool compute_gradients, int index);
        float newton_step(MBD &mbd, float step_size, int index);

        void main_signal_ready(int &ready_pos);
        void solver_thread_wait(int index, int &ready_pos);
        void solver_thread(MBD &mbd, int index);
    public:
        ~Solver()
        {
            if (probe_data != nullptr) {
                delete[] probe_data;
            }
        }

        void set_probe_data(QuantSH *comp_sh, glm::ivec3 probe_res);

        float *compute_pca(int n_components);
        void solve_mbd(MBD &mbd);

        float rand_float()
        {
            return float(rand()) / float(rand());
        }
        float rand_norm()
        {
            return float(rand()) / float(RAND_MAX);
        }

        void normalize(float *v);
        float dot(float *a, float *b);
        void proj(float *u, float *v, float *out);
        void orthonormalize(float *r, int n_vectors);
};
