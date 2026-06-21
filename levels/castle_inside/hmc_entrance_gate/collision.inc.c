const Collision inside_castle_seg7_collision_hmc_entrance_gate[] = {
    COL_INIT(),
    COL_VERTEX_INIT(4),
    COL_VERTEX(-430, 0, -430),
    COL_VERTEX(430, 0, -430),
    COL_VERTEX(430, 0, 430),
    COL_VERTEX(-430, 0, 430),
    COL_TRI_INIT(SURFACE_DEFAULT, 2),
    COL_TRI(0, 1, 2),
    COL_TRI(0, 2, 3),
    COL_TRI_STOP(),
    COL_END(),
};
