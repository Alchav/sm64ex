#include <PR/ultratypes.h>

#include "sm64.h"
#include "area.h"
#include "object_helpers.h"
#include "macro_special_objects.h"
#include "object_list_processor.h"
#include "sm64ap.h"

#include "behavior_data.h"

#include "macro_presets.h"

#include "special_presets.h"

/*
 * Converts the rotation value supplied by macro objects into one
 * that can be used by in-game objects.
 */
s16 convert_rotation(s16 inRotation) {
    u16 rotation = ((u16)(inRotation & 0xFF));
    rotation <<= 8;

    if (rotation == 0x3F00) {
        rotation = 0x4000;
    }

    if (rotation == 0x7F00) {
        rotation = 0x8000;
    }

    if (rotation == 0xBF00) {
        rotation = 0xC000;
    }

    if (rotation == 0xFF00) {
        rotation = 0x0000;
    }

    return (s16) rotation;
}

/*
 * Spawns an object at an absolute location with rotation around the y-axis and
 * parameters filling up the upper 2 bytes of newObj->oBehParams.
 * The object will not spawn if 'behavior' is NULL.
 */
void spawn_macro_abs_yrot_2params(u32 model, const BehaviorScript *behavior, s16 x, s16 y, s16 z, s16 ry, s16 params) {
    if (behavior != NULL) {
        struct Object *newObj = spawn_object_abs_with_rot(
            &gMacroObjectDefaultParent, 0, model, behavior, x, y, z, 0, convert_rotation(ry), 0);
        newObj->oBehParams = ((u32) params) << 16;
    }
}

/*
 * Spawns an object at an absolute location with rotation around the y-axis and
 * a single parameter filling up the upper byte of newObj->oBehParams.
 * The object will not spawn if 'behavior' is NULL.
 */
void spawn_macro_abs_yrot_param1(u32 model, const BehaviorScript *behavior, s16 x, s16 y, s16 z, s16 ry, s16 param) {
    if (behavior != NULL) {
        struct Object *newObj = spawn_object_abs_with_rot(
            &gMacroObjectDefaultParent, 0, model, behavior, x, y, z, 0, convert_rotation(ry), 0);
        newObj->oBehParams = ((u32) param) << 24;
    }
}

/*
 * Spawns an object at an absolute location with currently 3 unknown variables that get converted to
 * floats. Oddly enough, this function doesn't care if 'behavior' is NULL or not.
 */
void spawn_macro_abs_special(u32 model, const BehaviorScript *behavior, s16 x, s16 y, s16 z, s16 unkA, s16 unkB,
                             s16 unkC) {
    struct Object *newObj =
        spawn_object_abs_with_rot(&gMacroObjectDefaultParent, 0, model, behavior, x, y, z, 0, 0, 0);

    // Are all three of these values unused?
    newObj->oMacroUnk108 = (f32) unkA;
    newObj->oMacroUnk10C = (f32) unkB;
    newObj->oMacroUnk110 = (f32) unkC;
}

static void spawn_macro_coin_unknown(const BehaviorScript *behavior, s16 a1[]) {
    struct Object *sp3C;
    s16 model;

    model = bhvYellowCoin == behavior ? MODEL_YELLOW_COIN : MODEL_NONE;

    sp3C = spawn_object_abs_with_rot(&gMacroObjectDefaultParent, 0, model, behavior,
                                     a1[1], a1[2], a1[3], 0, convert_rotation(a1[0]), 0);

    sp3C->oUnk1A8 = a1[4];
    sp3C->oBehParams = (a1[4] & 0xFF) >> 16;
}

struct LoadedPreset {
    /*0x00*/ const BehaviorScript *behavior;
    /*0x04*/ s16 param; // huh? why does the below function swap these.. just use the struct..
    /*0x06*/ s16 model;
};

#define MACRO_OBJ_Y_ROT 0
#define MACRO_OBJ_X 1
#define MACRO_OBJ_Y 2
#define MACRO_OBJ_Z 3
#define MACRO_OBJ_PARAMS 4

#define MAX_AP_TRACKED_PLACEMENTS 1024

struct APSuppressedPlacement {
    const s16 *respawnInfo;
    bool suppressed;
};

static struct APSuppressedPlacement sAPSuppressedPlacements[MAX_AP_TRACKED_PLACEMENTS];
static s32 sAPSuppressedPlacementCount;

void clear_ap_suppressed_placement_state(void) {
    sAPSuppressedPlacementCount = 0;
}

static bool ap_placement_is_suppressed(const s16 *respawnInfo) {
    s32 i;
    for (i = 0; i < sAPSuppressedPlacementCount; i++) {
        if (sAPSuppressedPlacements[i].respawnInfo == respawnInfo) {
            return sAPSuppressedPlacements[i].suppressed;
        }
    }
    return FALSE;
}

static void set_ap_placement_suppressed(const s16 *respawnInfo, bool suppressed) {
    s32 i;
    for (i = 0; i < sAPSuppressedPlacementCount; i++) {
        if (sAPSuppressedPlacements[i].respawnInfo == respawnInfo) {
            sAPSuppressedPlacements[i].suppressed = suppressed;
            return;
        }
    }

    if (sAPSuppressedPlacementCount < MAX_AP_TRACKED_PLACEMENTS) {
        sAPSuppressedPlacements[sAPSuppressedPlacementCount].respawnInfo = respawnInfo;
        sAPSuppressedPlacements[sAPSuppressedPlacementCount].suppressed = suppressed;
        sAPSuppressedPlacementCount++;
    }
}

static struct Object *spawn_macro_object_entry(
    s16 areaIndex, const struct LoadedPreset *preset, s16 macroObject[5], s16 *respawnInfo) {
    struct Object *newObj;
    u32 behaviorParams = ((macroObject[MACRO_OBJ_PARAMS] & 0x00FF) << 16)
                         + (macroObject[MACRO_OBJ_PARAMS] & 0xFF00);

    if (((macroObject[MACRO_OBJ_PARAMS] >> 8) & RESPAWN_INFO_DONT_RESPAWN)
        == RESPAWN_INFO_DONT_RESPAWN) {
        return NULL;
    }

    if (!SM64AP_ShouldSpawnLevelObject(
            gCurrLevelNum, areaIndex, preset->model,
            macroObject[MACRO_OBJ_X], macroObject[MACRO_OBJ_Y], macroObject[MACRO_OBJ_Z],
            behaviorParams, preset->behavior)) {
        set_ap_placement_suppressed(respawnInfo, TRUE);
        return NULL;
    }

    set_ap_placement_suppressed(respawnInfo, FALSE);
    newObj = spawn_object_abs_with_rot(
        &gMacroObjectDefaultParent, 0, preset->model, preset->behavior,
        macroObject[MACRO_OBJ_X], macroObject[MACRO_OBJ_Y], macroObject[MACRO_OBJ_Z],
        0, convert_rotation(macroObject[MACRO_OBJ_Y_ROT]), 0);
    newObj->oUnk1A8 = macroObject[MACRO_OBJ_PARAMS];
    newObj->oBehParams = behaviorParams;
    newObj->oBehParams2ndByte = macroObject[MACRO_OBJ_PARAMS] & 0x00FF;
    newObj->respawnInfoType = RESPAWN_INFO_TYPE_16;
    newObj->respawnInfo = respawnInfo;
    newObj->parentObj = newObj;
    SM64AP_AssignPermanentCoinSource(
        newObj, gCurrLevelNum, areaIndex, preset->model,
        macroObject[MACRO_OBJ_X], macroObject[MACRO_OBJ_Y], macroObject[MACRO_OBJ_Z],
        behaviorParams, preset->behavior);
    return newObj;
}

void spawn_macro_objects(s16 areaIndex, s16 *macroObjList) {
    UNUSED u32 pad5C;
    s32 presetID;

    s16 macroObject[5]; // see the 5 #define statements above
    struct LoadedPreset preset;

    gMacroObjectDefaultParent.header.gfx.unk18 = areaIndex;
    gMacroObjectDefaultParent.header.gfx.unk19 = areaIndex;

    while (TRUE) {
        if (*macroObjList == -1) { // An encountered value of -1 means the list has ended.
            break;
        }

        presetID = (*macroObjList & 0x1FF) - 31; // Preset identifier for MacroObjectPresets array

        if (presetID < 0) {
            break;
        }

        // Set macro object properties from the list
        macroObject[MACRO_OBJ_Y_ROT] = ((*macroObjList++ >> 9) & 0x7F) << 1; // Y-Rotation
        macroObject[MACRO_OBJ_X] = *macroObjList++;                          // X position
        macroObject[MACRO_OBJ_Y] = *macroObjList++;                          // Y position
        macroObject[MACRO_OBJ_Z] = *macroObjList++;                          // Z position
        macroObject[MACRO_OBJ_PARAMS] = *macroObjList++;                     // Behavior params

        // Get the preset values from the MacroObjectPresets list.
        preset.model = MacroObjectPresets[presetID].model;
        preset.behavior = MacroObjectPresets[presetID].behavior;
        preset.param = MacroObjectPresets[presetID].param;

        if (preset.param != 0) {
            macroObject[MACRO_OBJ_PARAMS] =
                (macroObject[MACRO_OBJ_PARAMS] & 0xFF00) + (preset.param & 0x00FF);
        }

        spawn_macro_object_entry(areaIndex, &preset, macroObject, macroObjList - 1);
    }
}

static struct Object *find_live_macro_object(s16 *respawnInfo) {
    s32 i;
    for (i = 0; i < OBJECT_POOL_CAPACITY; i++) {
        struct Object *object = &gObjectPool[i];
        if ((object->activeFlags & ACTIVE_FLAG_ACTIVE)
            && object->respawnInfoType == RESPAWN_INFO_TYPE_16
            && object->respawnInfo == respawnInfo) {
            return object;
        }
    }
    return NULL;
}

void reconcile_macro_objects(s16 areaIndex, s16 *macroObjList) {
    while (macroObjList != NULL && *macroObjList != -1) {
        s32 presetID = (*macroObjList & 0x1FF) - 31;
        s16 macroObject[5];
        struct LoadedPreset preset;
        s16 *respawnInfo;
        struct Object *object;
        u32 behaviorParams;
        bool desired;

        if (presetID < 0) {
            break;
        }

        macroObject[MACRO_OBJ_Y_ROT] = ((*macroObjList++ >> 9) & 0x7F) << 1;
        macroObject[MACRO_OBJ_X] = *macroObjList++;
        macroObject[MACRO_OBJ_Y] = *macroObjList++;
        macroObject[MACRO_OBJ_Z] = *macroObjList++;
        respawnInfo = macroObjList;
        macroObject[MACRO_OBJ_PARAMS] = *macroObjList++;

        preset.model = MacroObjectPresets[presetID].model;
        preset.behavior = MacroObjectPresets[presetID].behavior;
        preset.param = MacroObjectPresets[presetID].param;
        if (preset.param != 0) {
            macroObject[MACRO_OBJ_PARAMS] =
                (macroObject[MACRO_OBJ_PARAMS] & 0xFF00) + (preset.param & 0x00FF);
        }

        behaviorParams = ((macroObject[MACRO_OBJ_PARAMS] & 0x00FF) << 16)
                         + (macroObject[MACRO_OBJ_PARAMS] & 0xFF00);
        desired = SM64AP_ShouldSpawnLevelObject(
            gCurrLevelNum, areaIndex, preset.model,
            macroObject[MACRO_OBJ_X], macroObject[MACRO_OBJ_Y], macroObject[MACRO_OBJ_Z],
            behaviorParams, preset.behavior);
        object = find_live_macro_object(respawnInfo);

        if (desired && object == NULL && ap_placement_is_suppressed(respawnInfo)) {
            spawn_macro_object_entry(areaIndex, &preset, macroObject, respawnInfo);
        } else if (!desired && object != NULL) {
            set_ap_placement_suppressed(respawnInfo, TRUE);
            object->oFlags |= OBJ_FLAG_PERSISTENT_RESPAWN;
            object->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        } else if (desired && object != NULL) {
            set_ap_placement_suppressed(respawnInfo, FALSE);
        }
    }
}

void spawn_macro_objects_hardcoded(s16 areaIndex, s16 *macroObjList) {
    UNUSED u8 pad[8];

    // This version of macroObjList has the preset and Y-Rotation separated,
    // and lacks behavior params. Might be an early version of the macro object list?
    s16 macroObjX;
    s16 macroObjY;
    s16 macroObjZ;
    s16 macroObjPreset;
    s16 macroObjRY; // Y Rotation

    UNUSED u8 pad2[10];

    gMacroObjectDefaultParent.header.gfx.unk18 = areaIndex;
    gMacroObjectDefaultParent.header.gfx.unk19 = areaIndex;

    while (TRUE) {
        macroObjPreset = *macroObjList++;

        if (macroObjPreset < 0) {
            break;
        }

        macroObjX = *macroObjList++;
        macroObjY = *macroObjList++;
        macroObjZ = *macroObjList++;
        macroObjRY = *macroObjList++;

        // Spawn objects based on hardcoded presets, and most seem to be for Big Boo's Haunt.
        // However, BBH doesn't use this function so this might just be an early test?
        switch (macroObjPreset) {
            case 0:
                spawn_macro_abs_yrot_2params(MODEL_NONE, bhvBooBossSpawnedBridge, macroObjX, macroObjY,
                                             macroObjZ, macroObjRY, 0);
                break;
            case 1:
                spawn_macro_abs_yrot_2params(MODEL_BBH_TILTING_FLOOR_PLATFORM,
                                             bhvBbhTiltingTrapPlatform, macroObjX, macroObjY, macroObjZ,
                                             macroObjRY, 0);
                break;
            case 2:
                spawn_macro_abs_yrot_2params(MODEL_BBH_TUMBLING_PLATFORM, bhvBbhTumblingBridge,
                                             macroObjX, macroObjY, macroObjZ, macroObjRY, 0);
                break;
            case 3:
                spawn_macro_abs_yrot_2params(MODEL_BBH_MOVING_BOOKSHELF, bhvHauntedBookshelf, macroObjX,
                                             macroObjY, macroObjZ, macroObjRY, 0);
                break;
            case 4:
                spawn_macro_abs_yrot_2params(MODEL_BBH_MESH_ELEVATOR, bhvMeshElevator, macroObjX,
                                             macroObjY, macroObjZ, macroObjRY, 0);
                break;
            case 20:
                spawn_macro_abs_yrot_2params(MODEL_YELLOW_COIN, bhvYellowCoin, macroObjX, macroObjY,
                                             macroObjZ, macroObjRY, 0);
                break;
            case 21:
                spawn_macro_abs_yrot_2params(MODEL_YELLOW_COIN, bhvYellowCoin, macroObjX, macroObjY,
                                             macroObjZ, macroObjRY, 0);
                break;
            default:
                break;
        }
    }
}

struct LoadedSpecialObject {
    s16 *respawnInfo;
    s16 x;
    s16 y;
    s16 z;
    s16 extraParams[3];
#ifdef VERSION_EU
    s16 model;
#else
    u8 model;
#endif
    u8 type;
    u8 defaultParam;
    const BehaviorScript *behavior;
};

#define SM64AP_COTMC_DOOR_PARAM 0xFE

static bool is_cotmc_entrance_door(const struct LoadedSpecialObject *entry) {
    return gCurrLevelNum == LEVEL_HMC
           && entry->y == -4279
           && ((entry->x == -468 && entry->z == 6711)
               || (entry->x == -359 && entry->z == 6602));
}

static void load_special_object_entry(s16 **specialObjList, struct LoadedSpecialObject *entry) {
    s32 offset = 0;
    u8 presetID;

    entry->respawnInfo = *specialObjList;
    presetID = (u8) **specialObjList;
    (*specialObjList)++;
    entry->x = **specialObjList;
    (*specialObjList)++;
    entry->y = **specialObjList;
    (*specialObjList)++;
    entry->z = **specialObjList;
    (*specialObjList)++;

    while (SpecialObjectPresets[offset].preset_id != presetID
           && SpecialObjectPresets[offset].preset_id != 0xFF) {
        offset++;
    }

    entry->model = SpecialObjectPresets[offset].model;
    entry->behavior = SpecialObjectPresets[offset].behavior;
    entry->type = SpecialObjectPresets[offset].type;
    entry->defaultParam = SpecialObjectPresets[offset].defParam;
    entry->extraParams[0] = 0;
    entry->extraParams[1] = 0;
    entry->extraParams[2] = 0;

    switch (entry->type) {
        case SPTYPE_YROT_NO_PARAMS:
        case SPTYPE_DEF_PARAM_AND_YROT:
            entry->extraParams[0] = **specialObjList;
            (*specialObjList)++;
            break;
        case SPTYPE_PARAMS_AND_YROT:
            entry->extraParams[0] = **specialObjList;
            (*specialObjList)++;
            entry->extraParams[1] = **specialObjList;
            (*specialObjList)++;
            break;
        case SPTYPE_UNKNOWN:
            entry->extraParams[0] = **specialObjList;
            (*specialObjList)++;
            entry->extraParams[1] = **specialObjList;
            (*specialObjList)++;
            entry->extraParams[2] = **specialObjList;
            (*specialObjList)++;
            break;
    }


    if (SM64AP_FullLevelUnlocks() && is_cotmc_entrance_door(entry)) {
        entry->model = MODEL_CASTLE_STAR_DOOR_8_STARS;
        entry->behavior = bhvStarDoor;
        entry->type = SPTYPE_DEF_PARAM_AND_YROT;
        entry->defaultParam = SM64AP_COTMC_DOOR_PARAM;
    }
}

static u32 get_special_object_behavior_params(const struct LoadedSpecialObject *entry) {
    if (entry->type == SPTYPE_PARAMS_AND_YROT) {
        return ((u32) entry->extraParams[1]) << 16;
    }
    if (entry->type == SPTYPE_DEF_PARAM_AND_YROT) {
        return ((u32) entry->defaultParam) << 24;
    }
    return 0;
}

static bool should_spawn_special_object(
    s16 areaIndex, const struct LoadedSpecialObject *entry) {
    return (((*entry->respawnInfo >> 8) & RESPAWN_INFO_DONT_RESPAWN)
            != RESPAWN_INFO_DONT_RESPAWN)
           && entry->behavior != NULL
           && SM64AP_ShouldSpawnLevelObject(
               gCurrLevelNum, areaIndex, entry->model, entry->x, entry->y, entry->z,
               get_special_object_behavior_params(entry), entry->behavior);
}

static struct Object *spawn_special_object_entry(
    s16 areaIndex, const struct LoadedSpecialObject *entry) {
    struct Object *newObj = NULL;
    u32 behaviorParams = get_special_object_behavior_params(entry);

    if (!should_spawn_special_object(areaIndex, entry)) {
        if (((*entry->respawnInfo >> 8) & RESPAWN_INFO_DONT_RESPAWN)
            != RESPAWN_INFO_DONT_RESPAWN
            && entry->behavior != NULL) {
            set_ap_placement_suppressed(entry->respawnInfo, TRUE);
        }
        return NULL;
    }

    set_ap_placement_suppressed(entry->respawnInfo, FALSE);
    switch (entry->type) {
        case SPTYPE_NO_YROT_OR_PARAMS:
            newObj = spawn_object_abs_with_rot(
                &gMacroObjectDefaultParent, 0, entry->model, entry->behavior,
                entry->x, entry->y, entry->z, 0, 0, 0);
            break;
        case SPTYPE_YROT_NO_PARAMS:
        case SPTYPE_PARAMS_AND_YROT:
            newObj = spawn_object_abs_with_rot(
                &gMacroObjectDefaultParent, 0, entry->model, entry->behavior,
                entry->x, entry->y, entry->z, 0, convert_rotation(entry->extraParams[0]), 0);
            if (entry->type == SPTYPE_PARAMS_AND_YROT) {
                newObj->oBehParams = behaviorParams;
            }
            break;
        case SPTYPE_UNKNOWN:
            newObj = spawn_object_abs_with_rot(
                &gMacroObjectDefaultParent, 0, entry->model, entry->behavior,
                entry->x, entry->y, entry->z, 0, 0, 0);
            newObj->oMacroUnk108 = (f32) entry->extraParams[0];
            newObj->oMacroUnk10C = (f32) entry->extraParams[1];
            newObj->oMacroUnk110 = (f32) entry->extraParams[2];
            break;
        case SPTYPE_DEF_PARAM_AND_YROT:
            newObj = spawn_object_abs_with_rot(
                &gMacroObjectDefaultParent, 0, entry->model, entry->behavior,
                entry->x, entry->y, entry->z, 0, convert_rotation(entry->extraParams[0]), 0);
            newObj->oBehParams = behaviorParams;
            break;
    }

    if (newObj != NULL) {
        newObj->respawnInfoType = RESPAWN_INFO_TYPE_16;
        newObj->respawnInfo = entry->respawnInfo;
        newObj->parentObj = newObj;
        SM64AP_AssignPermanentCoinSource(
            newObj, gCurrLevelNum, areaIndex, entry->model,
            entry->x, entry->y, entry->z, behaviorParams, entry->behavior);
    }
    return newObj;
}

void spawn_special_objects(s16 areaIndex, s16 **specialObjList) {
    s32 numOfSpecialObjects;
    s32 i;
    struct LoadedSpecialObject entry;

    numOfSpecialObjects = **specialObjList;
    (*specialObjList)++;

    gMacroObjectDefaultParent.header.gfx.unk18 = areaIndex;
    gMacroObjectDefaultParent.header.gfx.unk19 = areaIndex;

    for (i = 0; i < numOfSpecialObjects; i++) {
        load_special_object_entry(specialObjList, &entry);
        spawn_special_object_entry(areaIndex, &entry);
    }
}

void reconcile_special_objects(s16 areaIndex, s16 *specialObjList) {
    s32 numOfSpecialObjects;
    s32 i;
    struct LoadedSpecialObject entry;

    if (specialObjList == NULL) {
        return;
    }

    numOfSpecialObjects = *specialObjList++;
    for (i = 0; i < numOfSpecialObjects; i++) {
        struct Object *object;
        bool desired;

        load_special_object_entry(&specialObjList, &entry);
        desired = should_spawn_special_object(areaIndex, &entry);
        object = find_live_macro_object(entry.respawnInfo);

        if (desired && object == NULL && ap_placement_is_suppressed(entry.respawnInfo)) {
            spawn_special_object_entry(areaIndex, &entry);
        } else if (!desired && object != NULL) {
            set_ap_placement_suppressed(entry.respawnInfo, TRUE);
            object->oFlags |= OBJ_FLAG_PERSISTENT_RESPAWN;
            object->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        } else if (desired && object != NULL) {
            set_ap_placement_suppressed(entry.respawnInfo, FALSE);
        }
    }
}

// PC Port, so always use below
u32 get_special_objects_size(s16 *data) {
    s16 *startPos = data;
    s32 numOfSpecialObjects;
    s32 i;
    u8 presetID;
    s32 offset;

    numOfSpecialObjects = *data++;

    for (i = 0; i < numOfSpecialObjects; i++) {
        presetID = (u8) *data++;
        data += 3;
        offset = 0;

        while (TRUE) {
            if (SpecialObjectPresets[offset].preset_id == presetID) {
                break;
            }
            offset++;
        }

        switch (SpecialObjectPresets[offset].type) {
            case SPTYPE_NO_YROT_OR_PARAMS:
                break;
            case SPTYPE_YROT_NO_PARAMS:
                data++;
                break;
            case SPTYPE_PARAMS_AND_YROT:
                data += 2;
                break;
            case SPTYPE_UNKNOWN:
                data += 3;
                break;
            case SPTYPE_DEF_PARAM_AND_YROT:
                data++;
                break;
            default:
                break;
        }
    }

    return data - startPos;
}
