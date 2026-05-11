#include <iostream>
#include <thread>
#include <vector>

#include "solver.h"

int
get_index(glm::ivec3 g, glm::ivec3 res)
{
    return g.x + (g.y + g.z * res.y) * res.x;
}

glm::ivec3
get_pos_from_index(int id, glm::ivec3 res)
{
    return glm::ivec3(id % res.x, (id / res.x) % res.y, (id / (res.x * res.y)));
}

const glm::ivec3 trilin_offsets[8] = {
    glm::ivec3(0, 0, 0),
    glm::ivec3(1, 0, 0),
    glm::ivec3(0, 1, 0),
    glm::ivec3(1, 1, 0),
    glm::ivec3(0, 0, 1),
    glm::ivec3(1, 0, 1),
    glm::ivec3(0, 1, 1),
    glm::ivec3(1, 1, 1),
};

void get_trilin_ids_and_weights(glm::vec3 p, glm::ivec3 res, int len, int *ids, float *w)
{
    glm::ivec3 g = glm::ivec3(p);
    p = p - glm::vec3(g);
    glm::ivec3 o;
    glm::vec3 d;

    for (int i = 0; i < 8; i++) {
        o = g + trilin_offsets[i];
        bool valid = glm::all(glm::lessThanEqual(o, res - 1));
        o = glm::min(o, res - 1);
        ids[i] = get_index(o, res) * len;

        d = glm::vec3(1.0f) - p - glm::vec3(trilin_offsets[i]);
        if (valid) {
            w[i] = std::abs(d.x * d.y * d.z);
        } else {
            w[i] = 0.0f;
        }
    }
}

void Solver::trilin_interp(glm::vec3 p, glm::ivec3 res, int len, float *in, float *out)
{
    float w[8];
    int ids[8];

    for (int l = 0; l < len; l++) {
        out[l] = 0.0f;
    }

    get_trilin_ids_and_weights(p, res, len, ids, w);
    for (int i = 0; i < 8; i++) {
        for (int l = 0; l < len; l++) {
            out[l] += w[i] * in[ids[i] + l];
        }
    }
}

void Solver::interp_probe_data(glm::vec3 p, float *out)
{
    trilin_interp(p, probe_grid_res, DIMS, probe_data, out);
}

glm::vec3 Solver::rand_sample(glm::ivec3 pos)
{
    glm::vec3 s = glm::vec3(pos);
    s.x += rand_norm() * - 0.5f;
    s.y += rand_norm() * - 0.5f;
    s.z += rand_norm() * - 0.5f;
    return s;
}

void Solver::generate_random_samples(MBD &mbd)
{
    int probe_count = probe_grid_res.x * probe_grid_res.y * probe_grid_res.z;
    for (int i = 0; i < probe_count; i++) {
        glm::ivec3 pos = get_pos_from_index(i, probe_grid_res);
        samples[i] = glm::clamp(rand_sample(pos), glm::vec3(0.0f), glm::vec3(probe_grid_res - 1));
    }
}

void Solver::eval_mbd(MBD &mbd, glm::vec3 p, float *b, float *c, float *out)
{
    glm::vec3 b_p = (p / glm::vec3(mbd.coeff_res - 1)) * glm::vec3(mbd.basis_res - 1);

    trilin_interp(b_p, mbd.basis_res, DIMS * mbd.rank, mbd.b, b);
    trilin_interp(p, mbd.coeff_res, mbd.rank, mbd.c, c);

    for (int k = 0; k < DIMS; k++) {
        out[k] = 0.0f;
    }
    for (int l = 0; l < mbd.rank; l++) {
        for (int k = 0; k < DIMS; k++) {
            out[k] += c[l] * b[l * DIMS + k];
        }
    }
}

// First and second partial derivs calculated from formulas in MBD paper
void Solver::estimate_gradients(MBD &mbd, glm::vec3 p, float *b, float *c, float *r)
{
    int c_ids[8];
    float phi[8];
    get_trilin_ids_and_weights(p, mbd.coeff_res, mbd.rank, c_ids, phi);

    int b_ids[8];
    float psi[8];
    p /= glm::vec3(mbd.coeff_res - 1);
    p *= glm::vec3(mbd.basis_res - 1);
    get_trilin_ids_and_weights(p, mbd.basis_res, mbd.rank * DIMS, b_ids, psi);
    gradient_mutex.lock();
    for (int i = 0; i < 8; i++) {
        for (int l = 0; l < mbd.rank; l++) {
            for (int k = 0; k < DIMS; k++) {
                g_c[c_ids[i] + l] += r[k] * phi[i] * b[l * DIMS + k];
                h_c[c_ids[i] + l] += phi[i] * phi[i] * b[l * DIMS + k] * b[l * DIMS + k];

                g_b[b_ids[i] + l * DIMS + k] += r[k] * psi[i] * c[l];
                h_b[b_ids[i] + l * DIMS + k] += psi[i] * psi[i] * c[l] * c[l];
            }
        }
    }
    gradient_mutex.unlock();
}

float Solver::get_mbd_error(MBD &mbd, bool compute_gradients, int index)
{
    int coeff_count = mbd.coeff_res.x * mbd.coeff_res.y * mbd.coeff_res.z;
    int offset = cv_per_thread * index;
    int max_c = std::min(cv_per_thread, coeff_count - offset) + offset;

    float local_error = 0.0f;
    int probe_count = probe_grid_res.x * probe_grid_res.y * probe_grid_res.z;
    for (int i = offset; i < max_c; i++) {
        glm::vec3 s = samples[i];

        float interp_sh[DIMS];
        interp_probe_data(s, interp_sh);

        float mbd_sh[DIMS];
        float b[DIMS * mbd.rank];
        float c[mbd.rank];
        eval_mbd(mbd, s, b, c, mbd_sh);

        float r[DIMS];
        for (int k = 0; k < DIMS; k++) {
            r[k] = mbd_sh[k] - interp_sh[k];
            local_error += r[k] * r[k];
        }

        if (compute_gradients) {
            estimate_gradients(mbd, s, b, c, r);
        }
    }

    if (compute_gradients) {
        gradient_mutex.lock();
    }
    for (int i = offset; i < max_c; i++) {
        for (int l = 0; l < mbd.rank; l++) {
            local_error += LAMBDA * mbd.c[i * mbd.rank + l] * mbd.c[i * mbd.rank + l];
            if (compute_gradients) {
                g_c[i * mbd.rank + l] += LAMBDA * mbd.c[i * mbd.rank + l];
                h_c[i * mbd.rank + l] += LAMBDA;
            }
        }
    }
    if (compute_gradients) {
        gradient_mutex.unlock();
    }
    local_error *= 0.5f;

    return local_error;
}


void Solver::fit_coeffs_to_pca(MBD &mbd)
{
    float *pca = mbd.b;
    int coeff_count = mbd.coeff_res.x * mbd.coeff_res.y * mbd.coeff_res.z;
    for (int i = 0; i < coeff_count; i++) {
        for (int l = 0; l < mbd.rank; l++) {
            mbd.c[i * mbd.rank + l] = dot(&probe_data[i * DIMS], &pca[l * DIMS]);
        }
    }
}

float Solver::newton_step(MBD &mbd, float step_size, int index)
{
    int basis_vector_count = mbd.basis_res.x * mbd.basis_res.y * mbd.basis_res.z;
    int coeff_count = mbd.coeff_res.x * mbd.coeff_res.y * mbd.coeff_res.z;

    int coeff_offset = cv_per_thread * index;
    int basis_offset = bv_per_thread * index;

    int max_b = std::min(bv_per_thread, basis_vector_count - basis_offset) + basis_offset;
    int max_c = std::min(cv_per_thread, coeff_count - coeff_offset) + coeff_offset;

    float l_m = 0.0f;
    for (int b = basis_offset; b < max_b; b++) {
        for (int l = 0; l < mbd.rank; l++) {
            for (int k = 0; k < DIMS; k++) {
                if (h_b[(b * mbd.rank + l) * DIMS + k] != 0.0f) {
                    float p_k = (g_b[(b * mbd.rank + l) * DIMS + k] / h_b[(b * mbd.rank + l) * DIMS + k]);
                    l_m += p_k * g_b[(b * mbd.rank + l) * DIMS + k];
                    mbd.b[(b * mbd.rank + l) * DIMS + k] -= step_size * p_k;
                }
            }
        }
    }

    for (int c = coeff_offset; c < max_c; c++) {
        for (int l = 0; l < mbd.rank; l++) {
            if (h_c[c * mbd.rank + l] != 0.0f) {
                float p_k = (g_c[c * mbd.rank + l] / h_c[c * mbd.rank + l]);
                l_m += p_k * g_c[c * mbd.rank + l];
                mbd.c[c * mbd.rank + l] -= step_size * p_k;
            }
        }
    }

    return l_m;
}

void Solver::main_signal_ready(int &ready_pos)
{
    std::unique_lock<std::mutex> lock(main_mutex);
    error = 0.0f;
    m = 0.0f;
    main_ready[ready_pos] = true;
    main_ready[ready_pos == 1 ? 0 : 1] = false;
    ready_pos = (ready_pos == 1) ? 0 : 1;
    pending.store(n_threads);
    thread_cv.notify_all();
    main_cv.wait(lock, [this] {
        return pending.load() == 0;
    });
}

void Solver::solver_thread_wait(int index, int &ready_pos)
{
    std::unique_lock<std::mutex> lock(thread_mutexes[index]);
    if (!main_ready[ready_pos]) {
        if (pending.fetch_sub(1) == 1) {
            main_cv.notify_one();
        }
        thread_cv.wait(lock, [this, ready_pos] {
            return main_ready[ready_pos];
        });
    }
    ready_pos = (ready_pos == 1) ? 0 : 1;
}

void Solver::solver_thread(MBD &mbd, int index)
{
    int basis_vector_count = mbd.basis_res.x * mbd.basis_res.y * mbd.basis_res.z;
    int coeff_count = mbd.coeff_res.x * mbd.coeff_res.y * mbd.coeff_res.z;

    int coeff_offset = cv_per_thread * index;
    int basis_offset = bv_per_thread * index;

    int max_b = std::min(bv_per_thread, basis_vector_count - basis_offset) + basis_offset;
    int max_c = std::min(cv_per_thread, coeff_count - coeff_offset) + coeff_offset;

    int ready_pos = 0;
    while (!finished) {
        for (int b = basis_offset; b < max_b; b++) {
            for (int l = 0; l < mbd.rank; l++) {
                for (int k = 0; k < DIMS; k++) {
                    g_b[(b * mbd.rank + l) * DIMS + k] = h_b[(b * mbd.rank + l) * DIMS + k] = 0.0f;
                }
            }
        }
        for (int c = coeff_offset; c < max_c; c++) {
            for (int l = 0; l < mbd.rank; l++) {
                g_c[c * mbd.rank + l] = h_c[c * mbd.rank + l] = 0.0f;
            }
        }

        // Waiting for random samples to be generated
        solver_thread_wait(index, ready_pos);

        // Error calculations
        float local_error = get_mbd_error(mbd, true, index);
        error_mutex.lock();
        error += local_error;
        error_mutex.unlock();

        solver_thread_wait(index, ready_pos);

        while (!backtracking_finished) {
            // Newton Step
            float l_m = newton_step(mbd, step_size, index);
            error_mutex.lock();
            m += l_m;
            error_mutex.unlock();

            solver_thread_wait(index, ready_pos);

            // Updated Error Calculations
            float local_error = get_mbd_error(mbd, false, index);
            error_mutex.lock();
            error += local_error;
            error_mutex.unlock();

            solver_thread_wait(index, ready_pos);
        }
    }
}

void Solver::solve_mbd(MBD &mbd)
{
    // Allocate data for basis and coeffs, as well as their partial derivs
    int basis_vector_count = mbd.basis_res.x * mbd.basis_res.y * mbd.basis_res.z;
    mbd.b = new float[mbd.rank * DIMS * basis_vector_count];

    int coeff_count = mbd.coeff_res.x * mbd.coeff_res.y * mbd.coeff_res.z;
    mbd.c = new float[mbd.rank * coeff_count];

    g_b = new float[mbd.rank * DIMS * basis_vector_count];
    h_b = new float[mbd.rank * DIMS * basis_vector_count];
    g_c = new float[mbd.rank * coeff_count];
    h_c = new float[mbd.rank * coeff_count];

    samples = new glm::vec3[coeff_count];

    // Create threads and mutex objects
    // Performance doesn't really scale well with num of threads
    n_threads = std::thread::hardware_concurrency() - 1;
    thread_mutexes = new std::mutex[n_threads];
    bv_per_thread = (basis_vector_count - 1) / n_threads + 1;
    cv_per_thread = (coeff_count - 1) / n_threads + 1;

    std::memcpy(mbd.means, means, DIMS * sizeof(float));
    const float *pca = compute_pca(mbd.rank);
    for (int b = 0; b < basis_vector_count; b++) {
        std::memcpy(mbd.b + b * mbd.rank * DIMS, pca, mbd.rank * DIMS * sizeof(float));
    }
    fit_coeffs_to_pca(mbd);

    finished = false;
    main_ready[0] = false;
    pending.store(n_threads);
    std::vector<std::thread> threads;
    for (int t = 0; t < n_threads; t++) {
        threads.push_back(std::thread(&Solver::solver_thread, this, std::ref(mbd), t));
    }

    int ready_pos = 0;
    const int max_iter = 64;
    int i;
    for (i = 0; i < max_iter; i++) {
        generate_random_samples(mbd);

        // Error calculation synchronization
        main_signal_ready(ready_pos);

        std::cout << "MBD Error: " << error << std::endl;
        float old_error = error;

        // Newton Step Synchronization
        step_size = 1.0f;
        backtracking_finished = false;
        main_signal_ready(ready_pos);

        float l_m = m;

        // New Error Generation
        main_signal_ready(ready_pos);

        float new_error = error;

        // Perform backtracking line search
        // c_1 coeff is taken from Nocedal and Wright
        // alpha is somewhat coarse, but fast enough
        float actual_step_size = 1.0f;
        int steps = 0;
        const float alpha = 0.3f;
        const float c_1 = 0.0001f;
        l_m *= c_1;
        while (old_error - new_error <= actual_step_size * l_m && steps < 10) {
            steps++;
            float next_step_size = alpha * actual_step_size;

            step_size = next_step_size - actual_step_size;
            main_signal_ready(ready_pos);

            // New Error Generation
            main_signal_ready(ready_pos);

            actual_step_size = next_step_size;
            new_error = error;
        }

        backtracking_finished = true;
        std::cout << "MBD New Error: " << new_error << std::endl;

        if (i < max_iter - 1) {
            main_signal_ready(ready_pos);
        }
    }

    finished = true;
    main_ready[0] = main_ready[1] = true;
    thread_cv.notify_all();

    std::cout << "Finished in " << i << " iterations" << std::endl;

    for (int t = 0; t < n_threads; t++) {
        threads[t].join();
    }

    delete[] samples;
    samples = nullptr;

    delete[] g_b;
    g_b = nullptr;
    delete[] h_b;
    h_b = nullptr;

    delete[] g_c;
    g_c = nullptr;
    delete[] h_c;
    h_c = nullptr;
}
