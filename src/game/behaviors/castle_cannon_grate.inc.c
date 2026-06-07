// castle_cannon_grate.inc.c

void bhv_castle_cannon_grate_init(void) {
    if (SM64AP_HaveCastleCannon())
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
}
