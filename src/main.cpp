#include <iostream>

#include "renderer/renderer.h"
#include "solver/solver.h"

int
main()
{
    Renderer renderer;
    Solver solver;
    renderer.init();

    MBD mbd;
    mbd.basis_res = glm::ivec3(3);
    mbd.rank = 4;
    while(!renderer.should_close()) {
        renderer.draw();
        if (renderer.should_solve_mbd()) {
            CompSH *probe_data = nullptr;
            probe_data = renderer.get_probe_data();
            if (probe_data != nullptr) {
                glm::ivec3 res = renderer.get_grid_res();
                solver.set_probe_data(probe_data, res);

                mbd.coeff_res = res;
                solver.solve_mbd(mbd);
                renderer.set_mbd(mbd);
            }
        }
    }

    renderer.cleanup();
    /*float data[] = {
        0.682, 0.160, 0.483,
        0.296, 0.170, 0.615,
        0.806, 0.956, 0.635,
        0.733, 0.516, 0.131,
        0.772, 0.677, 0.757,
        0.107, 0.985, 0.675,
    };*/

    /*solver.orthonormalize(data, 2);
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            std::cout << data[i * 3 + j] << ", ";
        }
        std::cout << std::endl;
    }*/
    /*solver.set_raw_probe_data(data, glm::ivec3(6, 1, 1));
    solver.compute_pca(2);*/
    return 0;
}
