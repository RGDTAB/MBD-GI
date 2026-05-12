#include <iostream>

#include "renderer/renderer.h"
#include "solver/solver.h"

int
main()
{
    Renderer renderer;
    Solver solver;
    renderer.init();

    while(!renderer.should_close()) {
        renderer.draw();
        if (renderer.should_solve_mbd()) {
            QuantSH *probe_data = nullptr;
            probe_data = renderer.get_probe_data();
            if (probe_data != nullptr) {
                MBD mbd = renderer.get_mbd();
                solver.set_probe_data(probe_data, mbd.coeff_res);

                solver.solve_mbd(mbd, renderer.mbd_iter);
                renderer.set_mbd(mbd);
            }
        }
    }

    renderer.cleanup();
    return 0;
}
