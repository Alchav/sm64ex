#include <ultra64.h>
#include "sm64.h"
#include "geo_commands.h"

#include "game/level_geo.h"
#include "game/geo_misc.h"
#include "game/camera.h"
#include "game/moving_texture.h"
#include "game/screen_transition.h"
#include "game/paintings.h"
#include "sm64ap.h"

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

Gfx *geo_sm64ap_castle_wall_sign(s32 callContext, struct GraphNode *node, UNUSED Mat4 *mtx) {
    struct GraphNodeGenerated *generatedNode = (struct GraphNodeGenerated *) node;

    if (callContext != GEO_CONTEXT_RENDER || !SM64AP_HaveSigns(LEVEL_CASTLE)
        || generatedNode->parameter >= ARRAY_COUNT(sCastleWallSignDisplayLists)) {
        return NULL;
    }

    generatedNode->fnNode.node.flags =
        (generatedNode->fnNode.node.flags & 0xFF) | (LAYER_ALPHA << 8);
    return (Gfx *) sCastleWallSignDisplayLists[generatedNode->parameter];
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
