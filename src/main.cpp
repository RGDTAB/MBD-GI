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
            QuantSH *probe_data = nullptr;
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
    return 0;
}
