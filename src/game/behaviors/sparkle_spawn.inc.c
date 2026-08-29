/**
 * Behavior for bhvSparkleSpawn.
 *
 * This spawns the sparkles used by various objects. After being given a
 * random local position and scale, each sparkle's behavior is thereafter
 * controlled by bhvSparkle. This spawner is deleted after 1 frame.
 */
void bhv_sparkle_spawn_loop(void) {
    struct Object *sparkle = try_to_spawn_object(0, 1.0f, o, MODEL_SPARKLES_ANIMATION, bhvSparkle);
    if (sparkle != NULL) {
        sparkle->oBehParams2ndByte = o->oBehParams2ndByte;
        obj_translate_xyz_random(sparkle, 90.0f);
        obj_scale_random(sparkle, 1.0f, 0.0f);
    }
    if (o->oTimer > 1) {
        obj_mark_for_deletion(o);
    }
}

void spawn_ap_trigger_sparkles(struct Object *parent, s32 color) {
    struct Object *spawner = spawn_object(parent, MODEL_NONE, bhvSparkleSpawn);
    spawner->oBehParams2ndByte = color;
}

Gfx *geo_ap_sparkle_color(s32 callContext, struct GraphNode *node, UNUSED void *context) {
    Gfx *displayList;
    Gfx *head;
    u8 red = 255;
    u8 green = 255;
    u8 blue = 255;

    if (callContext != GEO_CONTEXT_RENDER || gCurGraphNodeObject == NULL) {
        return NULL;
    }
    switch (((struct Object *) gCurGraphNodeObject)->oBehParams2ndByte) {
        case SM64AP_TRIGGER_SPARKLE_GREEN:  red = 64;  green = 255; blue = 64;  break;
        case SM64AP_TRIGGER_SPARKLE_PURPLE: red = 200; green = 64;  blue = 255; break;
        case SM64AP_TRIGGER_SPARKLE_BLUE:   red = 64;  green = 128; blue = 255; break;
        case SM64AP_TRIGGER_SPARKLE_YELLOW: red = 255; green = 224; blue = 32;  break;
    }

    displayList = alloc_display_list(2 * sizeof(*displayList));
    if (displayList == NULL) {
        return NULL;
    }
    head = displayList;
    ((struct GraphNodeGenerated *) node)->fnNode.node.flags =
        (((struct GraphNodeGenerated *) node)->fnNode.node.flags & 0xFF) | LAYER_TRANSPARENT << 8;
    gDPSetPrimColor(head++, 0, 0, red, green, blue, 255);
    gSPEndDisplayList(head);
    return displayList;
}
