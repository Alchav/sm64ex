#include <ultra64.h>
#include "sm64.h"
#include "geo_commands.h"

#include "game/level_geo.h"
#include "game/geo_misc.h"
#include "game/camera.h"
#include "game/moving_texture.h"
#include "game/screen_transition.h"
#include "game/memory.h"
#include "game/paintings.h"
#include "sm64ap.h"
#include "sm64ap_visual.h"
#include "dialog_ids.h"

#include "make_const_nonconst.h"

#include "levels/castle_inside/header.h"

static const Gfx *const sCastleWallSignDisplayLists[] = {
    inside_castle_seg7_dl_07031830,
    inside_castle_seg7_dl_07033158,
    inside_castle_seg7_dl_07035288,
    inside_castle_seg7_dl_07038350,
    inside_castle_seg7_dl_0703A808,
    inside_castle_seg7_dl_07043CD8,
    inside_castle_seg7_dl_07066FA0,
};

static const s16 sCastleWallSignDialogs[] = {
    DIALOG_075, -1, DIALOG_069, DIALOG_147, DIALOG_052, DIALOG_019, DIALOG_077,
};

static void geo_sm64ap_append_wall_sign(
    Gfx **head, const Gfx *displayList, s16 dialog
) {
    u8 visualState = SM64AP_IsSignExhausted(LEVEL_CASTLE, dialog)
        ? SM64AP_VISUAL_EXHAUSTED : SM64AP_VISUAL_NORMAL;
    gDPNoOpTag((*head)++, SM64AP_VISUAL_TAG_BASE | visualState);
    gSPDisplayList((*head)++, displayList);
}

Gfx *geo_sm64ap_castle_wall_sign(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated *generatedNode = (struct GraphNodeGenerated *) node;

    if (callContext != GEO_CONTEXT_RENDER || !SM64AP_HaveSigns(LEVEL_CASTLE)
        || generatedNode->parameter >= ARRAY_COUNT(sCastleWallSignDisplayLists)) {
        return NULL;
    }

    generatedNode->fnNode.node.flags =
        (generatedNode->fnNode.node.flags & 0xFF) | (LAYER_ALPHA << 8);

    Gfx *displayList = alloc_display_list(7 * sizeof(*displayList));
    Gfx *head = displayList;
    if (generatedNode->parameter == 1) {
        geo_sm64ap_append_wall_sign(
            &head, inside_castle_sm64ap_wall_sign_dialog_046, DIALOG_046);
        geo_sm64ap_append_wall_sign(
            &head, inside_castle_sm64ap_wall_sign_dialog_070, DIALOG_070);
    } else {
        geo_sm64ap_append_wall_sign(
            &head, sCastleWallSignDisplayLists[generatedNode->parameter],
            sCastleWallSignDialogs[generatedNode->parameter]);
    }
    gDPNoOpTag(head++, SM64AP_VISUAL_TAG_BASE | SM64AP_VISUAL_NORMAL);
    gSPEndDisplayList(head);
    return displayList;
}

#include "levels/castle_inside/star_door/geo.inc.c"
#include "levels/castle_inside/trap_door/geo.inc.c"
#include "levels/castle_inside/areas/1/geo.inc.c"
#include "levels/castle_inside/areas/2/pendulum/geo.inc.c" // Another weird case
#include "levels/castle_inside/clock_minute_hand/geo.inc.c"
#include "levels/castle_inside/clock_hour_hand/geo.inc.c"
#include "levels/castle_inside/areas/2/geo.inc.c"
#include "levels/castle_inside/water_level_pillar/geo.inc.c"
#include "levels/castle_inside/areas/3/geo.inc.c"
