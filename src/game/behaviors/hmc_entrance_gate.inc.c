void bhv_hmc_entrance_gate_loop(void) {
    if (!SM64AP_PaintingRandoEnabled() || SM64AP_HavePainting(COURSE_HMC)) {
        return;
    }

    load_object_collision_model();
}
