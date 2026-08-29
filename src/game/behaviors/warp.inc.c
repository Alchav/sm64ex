// warp.c.inc

void bhv_warp_loop(void) {
    u16 sp6;
    u8 warpNodeId = (o->oBehParams & 0x00FF0000) >> 16;
    struct ObjectWarpNode *warpNode = area_get_warp_node(warpNodeId);
    bool levelEntranceLocked = gCurrLevelNum == LEVEL_CASTLE && warpNode != NULL
        && ((warpNode->node.destLevel == LEVEL_PSS && !SM64AP_HavePainting(COURSE_PSS))
            || (warpNode->node.destLevel == LEVEL_SA && !SM64AP_HavePainting(COURSE_SA)));

    sp6 = (o->oBehParams >> 24) & 0xFF;
    if (levelEntranceLocked) {
        o->hitboxRadius = 0.0f;
        o->hitboxHeight = 0.0f;
    } else {
        if (sp6 == 0)
            o->hitboxRadius = 50.0f;
        else if (sp6 == 0xFF)
            o->hitboxRadius = 10000.0f;
        else
            o->hitboxRadius = sp6 * 10.0;
        o->hitboxHeight = 50.0f;
    }
    o->oInteractStatus = 0;
}

void bhv_fading_warp_loop() // identical to the above function except for o->hitboxRadius
{
    u16 sp6;
    if (o->oTimer == 0) {
        sp6 = (o->oBehParams >> 24) & 0xFF;
        if (sp6 == 0)
            o->hitboxRadius = 85.0f;
        else if (sp6 == 0xFF)
            o->hitboxRadius = 10000.0f;
        else
            o->hitboxRadius = sp6 * 10.0;
        o->hitboxHeight = 50.0f;
    }
    if (SM64AP_TriggerSparkles() && gGlobalTimer % 5 == 0) {
        spawn_ap_trigger_sparkles(o, SM64AP_TRIGGER_SPARKLE_BLUE);
    }
    o->oInteractStatus = 0;
}
