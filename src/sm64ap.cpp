#include "sm64ap.h"
#include "Archipelago.h"

extern "C" {
    #include "game/area.h"
    #include "game/print.h"
    #include "behavior_data.h"
    #include "gfx_dimensions.h"
    #include "level_table.h"
    #include "course_table.h"
    #include "model_ids.h"
    #include "seq_ids.h"
    #include "engine/behavior_script.h"
    #include "game/level_update.h"
    #include "game/object_list_processor.h"
    #include "object_constants.h"

    void SM64AP_SetMarioShirtColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioOverallsColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioGlovesColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioShoesColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioSkinColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioHairColor(u8 r, u8 g, u8 b);
    void SM64AP_ResetMarioSideburnColor(void);
    void SM64AP_SetMarioSideburnColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioCapShirtColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioCapGlovesColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioCapHairColor(u8 r, u8 g, u8 b);
}

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <bitset>
#include <set>
#include <queue>
#include <cctype>
#include <utility>

#define WARP_NODE_CREDITS_MIN 0xF8 // level_update.c
#define NUM_PAINTING_LOCKS SM64AP_NUM_PAINTING_LOCKS

// Set to false on some branch for compat with patches
static constexpr bool SM64AP_SUPPORT_MOVE_RANDO = true;
static constexpr const char *SM64AP_GAME_NAME = "SM64: Spicy Mycena 64";

int starsCollected = 0;
bool sm64_locations[SM64AP_NUM_LOCS];
bool sm64_have_first_floor_key = false;
int sm64_have_progressive_basement_keys = 0;
int sm64_have_progressive_upstairs_keys = 0;
int sm64_have_progressive_keys = 0;
int sm64_have_progressive_mips = 0;
bool sm64_have_wing_cap_light = false;
bool sm64_have_bbh = false;
bool sm64_have_toads = false;
bool sm64_have_castle_cannon = false;
bool sm64_have_yoshi = false;
bool sm64_have_wingcap = false;
bool sm64_have_metalcap = false;
bool sm64_have_vanishcap = false;
bool sm64_show_global_cap_display = false;
int sm64_moat_state = 0;
bool sm64_have_cannon[15];
bool sm64_have_painting[NUM_PAINTING_LOCKS];
bool sm64_painting_rando_enabled = false;
int sm64_completion_type = 0;
std::bitset<SM64AP_NUM_ABILITIES> sm64_have_abilities;
std::bitset<SM64AP_NUM_FEATURES> sm64_have_features;
std::bitset<SM64AP_NUM_LEVEL_CAPS> sm64_have_level_caps;
std::bitset<SM64AP_NUM_OBJECT_ITEMS> sm64_have_object_items;
int* sm64_clockaction = nullptr;
int sm64_cost_firstbowserdoor = 8;
int sm64_cost_basementdoor = 30;
int sm64_cost_secondfloordoor = 50;
int sm64_cost_endlessstairs = 70;
int sm64_cost_mips1 = 15;
int sm64_cost_mips2 = 50;
int msg_frame_duration = 90; // 3 Secounds at 30F/s
int cur_msg_frame_duration = msg_frame_duration;
std::queue<int64_t> delayed_queue;

std::map<int,int> map_entrances;
std::set<int> course_dest_supported;

std::map<int,int> map_boxid_locid;

int sm64_exit_return_to;
int sm64_exit_orig_entrancelvl;
int sm64_wdw_entrance_variant = 0;
int sm64_ttc_entrance_variant = SM64AP_ENTRANCE_TTC_STOPPED;
int sm64_music_shuffle_mode = 0;
std::map<int,int> map_music;
int sm64_coin_star_requirements[15] = {
    100, 100, 100, 100, 100,
    100, 100, 100, 100, 100,
    100, 100, 100, 100, 100,
};

static constexpr int SM64AP_MUSIC_SHUFFLE_OFF = 0;
static constexpr int SM64AP_MUSIC_SHUFFLE_MAP = 1;
static constexpr int SM64AP_MUSIC_SHUFFLE_RANDOM_ON_LOAD = 2;
static constexpr int SM64AP_NUM_COIN_STAR_REQUIREMENTS = 15;
static constexpr int SM64AP_DEFAULT_COIN_STAR_REQUIREMENT = 100;

static constexpr int SM64AP_RANDOM_MUSIC_POOL[] = {
    SEQ_LEVEL_GRASS,
    SEQ_LEVEL_INSIDE_CASTLE,
    SEQ_LEVEL_WATER,
    SEQ_LEVEL_WATER | SEQ_VARIATION,
    SEQ_LEVEL_HOT,
    SEQ_LEVEL_BOSS_KOOPA,
    SEQ_LEVEL_SNOW,
    SEQ_LEVEL_SLIDE,
    SEQ_LEVEL_SPOOKY,
    SEQ_LEVEL_UNDERGROUND,
    SEQ_LEVEL_KOOPA_ROAD,
    SEQ_EVENT_MERRY_GO_ROUND,
    SEQ_EVENT_RACE,
    SEQ_EVENT_BOSS,
    SEQ_EVENT_ENDLESS_STAIRS,
    SEQ_LEVEL_BOSS_KOOPA_FINAL,
    SEQ_MENU_FILE_SELECT,
};

static void SM64AP_IncrementClamped(int &value, int maxValue) {
    if (value < maxValue) {
        value++;
    }
}

static void SM64AP_SetMin(int &value, int minValue) {
    if (value < minValue) {
        value = minValue;
    }
}

void SM64AP_RecvItem(int64_t idx, bool notify) {
    switch (idx) {
        case SM64AP_ITEMID_STAR:
            starsCollected++;
            break;
        case SM64AP_ID_KEY1:
            SM64AP_SetMin(sm64_have_progressive_basement_keys, 1);
            break;
        case SM64AP_ID_KEY2:
            SM64AP_SetMin(sm64_have_progressive_upstairs_keys, 1);
            break;
        case SM64AP_ID_KEYPROG:
            SM64AP_IncrementClamped(sm64_have_progressive_keys, SM64AP_NUM_CASTLE_KEYS);
            break;
        case SM64AP_ID_FIRST_FLOOR_KEY:
            sm64_have_first_floor_key = true;
            break;
        case SM64AP_ID_BASEMENT_KEYPROG:
            SM64AP_IncrementClamped(sm64_have_progressive_basement_keys, 2);
            break;
        case SM64AP_ID_UPSTAIRS_KEYPROG:
            SM64AP_IncrementClamped(sm64_have_progressive_upstairs_keys, 3);
            break;
        case SM64AP_ID_PROGRESSIVE_MIPS:
            SM64AP_IncrementClamped(sm64_have_progressive_mips, 2);
            break;
        case SM64AP_ID_WING_CAP_LIGHT:
            sm64_have_wing_cap_light = true;
            break;
        case SM64AP_ID_BBH:
            sm64_have_bbh = true;
            break;
        case SM64AP_ID_TOADS:
            sm64_have_toads = true;
            break;
        case SM64AP_ID_CASTLE_CANNON:
            sm64_have_castle_cannon = true;
            break;
        case SM64AP_ID_YOSHI:
            sm64_have_yoshi = true;
            break;
        case SM64AP_ID_WINGCAP:
            sm64_have_wingcap = true;
            break;
        case SM64AP_ID_METALCAP:
            sm64_have_metalcap = true;
            break;
        case SM64AP_ID_VANISHCAP:
            sm64_have_vanishcap = true;
            break;
        case SM64AP_ID_LEVEL_CAP(0) ... SM64AP_ID_LEVEL_CAP(SM64AP_NUM_LEVEL_CAPS - 1):
            sm64_have_level_caps[idx - SM64AP_LEVEL_CAP_OFFSET] = true;
            break;
        case SM64AP_ID_OBJECT_ITEM(0) ... SM64AP_ID_OBJECT_ITEM(SM64AP_NUM_OBJECT_ITEMS - 1):
            sm64_have_object_items[idx - SM64AP_OBJECT_ITEM_OFFSET] = true;
            break;
        case SM64AP_ITEMID_1UP:
            gMarioState->numLives++;
            break;
        case SM64AP_ID_CANNONUNLOCK(0) ... SM64AP_ID_CANNONUNLOCK(15-1):
            sm64_have_cannon[idx-(SM64AP_ID_CANNONUNLOCK(0))] = true;
            break;
        case SM64AP_ID_PAINTINGUNLOCK(0) ... SM64AP_ID_PAINTINGUNLOCK(NUM_PAINTING_LOCKS-1):
            // We don't have a painting unlock for BoB, so (0) will never appear; index 1 corresponds to WF, and so on
            sm64_have_painting[idx-(SM64AP_ID_PAINTINGUNLOCK(0))] = true;
            break;
        case SM64AP_ID_ABILITY(0):
            sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET+1] = sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET];
            sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET] = true;
            break;
        case SM64AP_ID_ABILITY(1) ... SM64AP_ID_ABILITY(SM64AP_NUM_ABILITIES-1):
            sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET] = true;
            break;
        case SM64AP_ID_1_HEALTH_PIP ... SM64AP_ID_GUST_TRAP:
            if(!notify) break;
            delayed_queue.push(idx);
            break;
        case SM64AP_ID_FEATURE(0) ... SM64AP_ID_FEATURE(SM64AP_NUM_FEATURES-1):
            sm64_have_features[idx-(SM64AP_ID_FEATURE(0))] = true;
            break;
    }
}

void SM64AP_CheckLocation(int64_t loc_id) {
    sm64_locations[loc_id - SM64AP_ID_OFFSET] = true;
}

u32 SM64AP_CourseStarFlags(s32 courseIdx) {
    u32 starflags = 0;
    s32 courseIndex = courseIdx;
    if (courseIdx == -1) {
        courseIndex = 24;
    }
    for (int i = 0; i < 7; i++) {
        if (sm64_locations[i + (courseIndex * 7)]) {
            starflags |= (1 << i);
        }
    }
    return starflags;
}

static constexpr s32 AP_COURSE_BOB = 0;
static constexpr s32 AP_COURSE_WF = 1;
static constexpr s32 AP_COURSE_JRB = 2;
static constexpr s32 AP_COURSE_CCM = 3;
static constexpr s32 AP_COURSE_BBH = 4;
static constexpr s32 AP_COURSE_LLL = 6;
static constexpr s32 AP_COURSE_SSL = 7;

static bool behavior_is(const void *behavior, const BehaviorScript *target) {
    return behavior == target;
}

static u8 beh_param_star(u32 behParam) {
    return (behParam >> 24) & 0xFF;
}

static u8 beh_param_second_byte(u32 behParam) {
    return (behParam >> 16) & 0xFF;
}

bool SM64AP_HaveFeature(int feature) {
    return feature >= 0 && feature < SM64AP_NUM_FEATURES && sm64_have_features[feature];
}

bool SM64AP_HaveObjectItem(int item) {
    return item >= 0 && item < SM64AP_NUM_OBJECT_ITEMS && sm64_have_object_items[item];
}

bool SM64AP_CollectedCourseStar(int courseIdx, int starIdx) {
    return courseIdx >= 0 && starIdx >= 0 && starIdx < 7
        && (SM64AP_CourseStarFlags(courseIdx) & (1 << starIdx));
}

static bool SM64AP_ShouldSpawnBobObject(s16 x, s16, s16, u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvKingBobomb)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_BOB_KING_BOBOMB);
    }
    if (behavior_is(behavior, bhvKoopaRaceEndpoint) || behavior_is(behavior, bhvKoopa)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_BOB_KOOPA_THE_QUICK);
    }
    if (behavior_is(behavior, bhvBobombBuddyOpensCannon) || behavior_is(behavior, bhvCannonClosed)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_BOB_BOBOMB_BUDDY);
    }
    if (behavior_is(behavior, bhvWaterBombCannon)) {
        return !SM64AP_HaveFeature(SM64AP_FEATURE_BOB_BOBOMB_BUDDY);
    }
    if (behavior_is(behavior, bhvBobombBuddy)) {
        return beh_param_second_byte(behParam) == 3
            ? SM64AP_HaveFeature(SM64AP_FEATURE_BOB_BOBOMB_BUDDY)
            : !SM64AP_HaveFeature(SM64AP_FEATURE_BOB_BOBOMB_BUDDY);
    }
    if (behavior_is(behavior, bhvBobBowlingBallSpawner)) {
        return SM64AP_CollectedCourseStar(AP_COURSE_BOB, 0);
    }
    if (behavior_is(behavior, bhvTtmBowlingBallSpawner)) {
        return SM64AP_CollectedCourseStar(AP_COURSE_BOB, 1);
    }
    if (behavior_is(behavior, bhvPitBowlingBall) && x == -93) {
        return SM64AP_CollectedCourseStar(AP_COURSE_BOB, 0);
    }
    return true;
}

static bool SM64AP_ShouldSpawnWfObject(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvWhompKingBoss)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_WF_WHOMP_KING);
    }
    if (behavior_is(behavior, bhvKickableBoard) || behavior_is(behavior, bhv1Up)
        || behavior_is(behavior, bhvBulletBill) || behavior_is(behavior, bhvTower)
        || behavior_is(behavior, bhvBulletBillCannon) || behavior_is(behavior, bhvTowerPlatformGroup)
        || behavior_is(behavior, bhvTowerDoor)
        || (behavior_is(behavior, bhvStar) && beh_param_star(behParam) == 1)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_WF_FORTRESS);
    }
    if (behavior_is(behavior, bhvBobombBuddyOpensCannon)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_WF_BOBOMB_BUDDY);
    }
    if (behavior_is(behavior, bhvHoot)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_WF_HOOT);
    }
    return true;
}

static bool SM64AP_ShouldSpawnCcmObject(const void *behavior) {
    bool snowmanStar = SM64AP_CollectedCourseStar(AP_COURSE_CCM, 4);

    if (behavior_is(behavior, bhvSmallPenguin)) {
        return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_CCM_BABY_PENGUINS);
    }
    if (behavior_is(behavior, bhvSnowmansBottom)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_CCM_SNOWMANS_HEAD) && !snowmanStar;
    }
    if (behavior_is(behavior, bhvSnowmansHead)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_CCM_SNOWMANS_HEAD) || snowmanStar;
    }
    if (behavior_is(behavior, bhvRacingPenguin)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_CCM_BIG_PENGUIN);
    }
    return true;
}

static bool SM64AP_ShouldSpawnJrbObject(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvSunkenShipPart) || behavior_is(behavior, bhvSunkenShipPart2)
        || behavior_is(behavior, bhvInSunkenShip) || behavior_is(behavior, bhvInSunkenShip2)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_JRB_SUNKEN_SHIP);
    }
    if (behavior_is(behavior, bhvShipPart3) || behavior_is(behavior, bhvInSunkenShip3)
        || behavior_is(behavior, bhvJrbSlidingBox)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_JRB_RAISED_SHIP);
    }
    if (behavior_is(behavior, bhvBobombBuddyOpensCannon)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_JRB_BOBOMB_BUDDY);
    }
    if (behavior_is(behavior, bhvJetStream)
        || (behavior_is(behavior, bhvStar) && beh_param_star(behParam) == 5)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_JRB_JET_STREAM);
    }
    if (behavior_is(behavior, bhvUnagi)) {
        bool star2Collected = SM64AP_CollectedCourseStar(AP_COURSE_JRB, 1);
        if (!SM64AP_HaveFeature(SM64AP_FEATURE_JRB_UNAGI)) {
            return false;
        }
        switch (beh_param_second_byte(behParam)) {
            case 1:
                return !star2Collected;
            case 2:
                return star2Collected;
            default:
                return false;
        }
    }
    return true;
}

static bool SM64AP_ShouldSpawnSslObject(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvPyramidElevator)) {
        return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_SSL_PYRAMID_ELEVATOR);
    }
    if (behavior_is(behavior, bhvKlepto)) {
        bool kleptoStarCollected = SM64AP_CollectedCourseStar(AP_COURSE_SSL, 0);
        bool kleptoShouldHoldStar = SM64AP_HaveFeature(SM64AP_FEATURE_SSL_KLEPTO_STAR)
            && !kleptoStarCollected;

        return beh_param_second_byte(behParam) != 0
            ? kleptoShouldHoldStar
            : !kleptoShouldHoldStar;
    }
    return true;
}

static bool SM64AP_ShouldSpawnDddObject(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvMantaRay)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_DDD_MANTA_RAY);
    }
    if (behavior_is(behavior, bhvBowserSubDoor)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_DDD_BOWSERS_SUB)
            || SM64AP_HaveFeature(SM64AP_FEATURE_DDD_POLES);
    }
    if (behavior_is(behavior, bhvBowsersSub)) {
        if (behParam == 0x000B0000) {
            return SM64AP_HaveFeature(SM64AP_FEATURE_DDD_BOWSERS_SUB);
        }
        return SM64AP_HaveFeature(SM64AP_FEATURE_DDD_POLES)
            && !SM64AP_HaveFeature(SM64AP_FEATURE_DDD_BOWSERS_SUB);
    }
    if (behavior_is(behavior, bhvDDDPole)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_DDD_POLES);
    }
    return true;
}

static bool SM64AP_IsBbhNormalBooPosition(s16 x, s16 y, s16 z) {
    return (x == 20 && y == 100 && z == -908)
        || (x == 3150 && y == 100 && z == 398)
        || (x == -2000 && y == 150 && z == -800)
        || (x == 2851 && y == 100 && z == 2289)
        || (x == -1551 && y == 100 && z == -1018);
}

static bool SM64AP_ShouldSpawnBbhObject(s16 x, s16 y, s16 z, const void *behavior) {
    bool ghostHuntStarCollected = SM64AP_CollectedCourseStar(AP_COURSE_BBH, 0);

    if (behavior_is(behavior, bhvGhostHuntBigBoo) || behavior_is(behavior, bhvGhostHuntBoo)) {
        return !ghostHuntStarCollected;
    }
    if (behavior_is(behavior, bhvBoo) && SM64AP_IsBbhNormalBooPosition(x, y, z)) {
        return ghostHuntStarCollected;
    }
    if (behavior_is(behavior, bhvHiddenStaircaseStep)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_BBH_STAIRCASE);
    }
    if (behavior_is(behavior, bhvMerryGoRound) || behavior_is(behavior, bhvFlamethrower)
        || behavior_is(behavior, bhvMerryGoRoundBooManager)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_BBH_MERRY_GO_ROUND);
    }
    return true;
}

static bool SM64AP_IsCheckerboardPlatformObject(s16 level, s16 model, const void *behavior) {
    if (behavior_is(behavior, bhvCheckerboardElevatorGroup)) {
        return level == LEVEL_BOB || level == LEVEL_WF || level == LEVEL_VCUTM;
    }

    return behavior_is(behavior, bhvPlatformOnTrack)
        && model == MODEL_CHECKERBOARD_PLATFORM
        && (level == LEVEL_LLL || level == LEVEL_HMC);
}

static bool SM64AP_IsPurpleSwitchObject(s16 model, const void *behavior) {
    return model == MODEL_PURPLE_SWITCH
        && (behavior_is(behavior, bhvFloorSwitchAnimatesObject)
            || behavior_is(behavior, bhvFloorSwitchGrills)
            || behavior_is(behavior, bhvFloorSwitchHardcodedModel)
            || behavior_is(behavior, bhvFloorSwitchHiddenObjects)
            || behavior_is(behavior, bhvPurpleSwitchHiddenBoxes));
}

bool SM64AP_ShouldSpawnLevelObject(s16 level, s16, s16 model, s16 x, s16 y, s16 z, u32 behParam, const void *behavior) {
    if (SM64AP_IsPurpleSwitchObject(model, behavior)) {
        return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_PURPLE_SWITCHES);
    }

    if (SM64AP_IsCheckerboardPlatformObject(level, model, behavior)) {
        return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_CHECKERBOARD_PLATFORMS);
    }

    switch (level) {
        case LEVEL_BOB:
            return SM64AP_ShouldSpawnBobObject(x, y, z, behParam, behavior);
        case LEVEL_WF:
            return SM64AP_ShouldSpawnWfObject(behParam, behavior);
        case LEVEL_CCM:
            return SM64AP_ShouldSpawnCcmObject(behavior);
        case LEVEL_JRB:
            return SM64AP_ShouldSpawnJrbObject(behParam, behavior);
        case LEVEL_LLL:
            if (behavior_is(behavior, bhvExclamationBox) && behParam == 0x00030000) {
                return SM64AP_HaveFeature(SM64AP_FEATURE_LLL_KOOPA_SHELL);
            }
            if (behavior_is(behavior, bhvLllRollingLog)) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_ROLLING_LOGS);
            }
            return true;
        case LEVEL_SSL:
            return SM64AP_ShouldSpawnSslObject(behParam, behavior);
        case LEVEL_THI:
            if (behavior_is(behavior, bhvWarpPipe)) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_THI_WARP_PIPES);
            }
            if (behavior_is(behavior, bhvKoopa) || behavior_is(behavior, bhvKoopaRaceEndpoint)) {
                return SM64AP_HaveFeature(SM64AP_FEATURE_THI_KOOPA_THE_QUICK);
            }
            return true;
        case LEVEL_HMC:
            if (behavior_is(behavior, bhvDorrie)) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_HMC_SWIMMING_BEAST);
            }
            return true;
        case LEVEL_SL:
            if (behavior_is(behavior, bhvSLWalkingPenguin)) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_SL_PENGUIN);
            }
            return true;
        case LEVEL_RR:
            if (behavior_is(behavior, bhvPlatformOnTrack) && model == MODEL_RR_FLYING_CARPET) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_RR_CARPETS);
            }
            return true;
        case LEVEL_TTM:
            if (behavior_is(behavior, bhvTtmRollingLog)) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_ROLLING_LOGS);
            }
            if (behavior_is(behavior, bhvUkiki) || behavior_is(behavior, bhvUkikiCage)) {
                return SM64AP_HaveFeature(SM64AP_FEATURE_TTM_UKIKI);
            }
            return true;
        case LEVEL_DDD:
            return SM64AP_ShouldSpawnDddObject(behParam, behavior);
        case LEVEL_BBH:
            return SM64AP_ShouldSpawnBbhObject(x, y, z, behavior);
        default:
            return true;
    }
}

bool SM64AP_ShouldCreateWhirlpool(s16 level, s16, s8, s8 condition, s16, s16, s16, s16) {
    switch (condition) {
        case 0:
            return true;
        case 2:
            return level == LEVEL_DDD && SM64AP_HaveFeature(SM64AP_FEATURE_DDD_POLES);
        case 3:
            return level == LEVEL_JRB && SM64AP_HaveFeature(SM64AP_FEATURE_JRB_JET_STREAM);
        default:
            return true;
    }
}

void setCourseNodeAndArea(int coursenum, s16* oldnode, bool isDeathWarp, int warpOp) {
    switch (coursenum) {
        case LEVEL_BOB:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x64 : 0x32;
            return;
        case LEVEL_CCM:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x65 : 0x33;
            return;
        case LEVEL_WF:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x66 : 0x34;
            return;
        case LEVEL_JRB:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x67 : 0x35;
            return;
        case LEVEL_BBH:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x0B : 0x0A;
            return;
        case LEVEL_LLL:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x64 : 0x32;
            return;
        case LEVEL_SSL:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x65 : 0x33;
            return;
        case LEVEL_HMC:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x66 : 0x34;
            return;
        case LEVEL_DDD:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x67 : 0x35;
            return;
        case LEVEL_WDW:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x64 : 0x32;
            return;
        case LEVEL_THI:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x65 : 0x33;
            return;
        case LEVEL_TTM:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x66 : 0x34;
            return;
        case LEVEL_TTC:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x67 : 0x35;
            return;
        case LEVEL_SL:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x68 : 0x36;
            return;
        case LEVEL_RR:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x6C : 0x3A;
            return;
        case LEVEL_PSS:
        case LEVEL_TOTWC:
            *oldnode = isDeathWarp ? 0x21 : (warpOp == WARP_OP_STAR_EXIT ? 0x26: 0x20);
            return;
        case LEVEL_SA:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x28 : 0x27;
            return;
        case LEVEL_BITDW:
        case LEVEL_BOWSER_1:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x25 : 0x24;
            return;
        case LEVEL_VCUTM:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x06 : 0x07;
            return;
        case LEVEL_BITFS:
        case LEVEL_BOWSER_2:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x68 : 0x36;
            return;
        case LEVEL_WMOTR:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x6D : 0x38;
        default:
            return;
    }
}

static int SM64AP_GetEntranceDefaultVariant() {
    return 1;
}

static int SM64AP_DefaultEntranceKey(int entranceKey) {
    int level = entranceKey / 10;
    int variant = entranceKey % 10;

    if ((level == LEVEL_WDW && variant >= SM64AP_ENTRANCE_WDW_LOW && variant <= SM64AP_ENTRANCE_WDW_HIGH)
        || (level == LEVEL_TTC && variant >= SM64AP_ENTRANCE_TTC_STOPPED && variant <= SM64AP_ENTRANCE_TTC_FAST)) {
        return SM64AP_ENTRANCE_ID(level, SM64AP_GetEntranceDefaultVariant());
    }

    return entranceKey;
}

static int SM64AP_GetMappedEntrance(int sourceEntrance) {
    auto itr = map_entrances.find(sourceEntrance);
    if (itr != map_entrances.end()) {
        return itr->second;
    }

    int defaultEntrance = SM64AP_DefaultEntranceKey(sourceEntrance);
    itr = map_entrances.find(defaultEntrance);
    if (itr != map_entrances.end()) {
        return itr->second;
    }

    return sourceEntrance;
}

static void SM64AP_SetTTCEntranceVariantSpeed(int variant) {
    switch (variant) {
        case SM64AP_ENTRANCE_TTC_STOPPED:
            gTTCSpeedSetting = TTC_SPEED_STOPPED;
            break;
        case SM64AP_ENTRANCE_TTC_SLOW:
            gTTCSpeedSetting = TTC_SPEED_SLOW;
            break;
        case SM64AP_ENTRANCE_TTC_RANDOM:
            gTTCSpeedSetting = TTC_SPEED_RANDOM;
            break;
        case SM64AP_ENTRANCE_TTC_FAST:
            gTTCSpeedSetting = TTC_SPEED_FAST;
            break;
    }
}

static void SM64AP_SetWDWEntranceVariant(int variant) {
    sm64_wdw_entrance_variant = variant;
}

static void SM64AP_ApplyEntranceDestination(int destination, s16* destLevel, s16* destArea) {
    int level = destination / 10;
    int variant = destination % 10;

    SM64AP_SetWDWEntranceVariant(0);
    *destLevel = level;
    *destArea = variant;

    if (level == LEVEL_WDW && variant >= SM64AP_ENTRANCE_WDW_LOW && variant <= SM64AP_ENTRANCE_WDW_HIGH) {
        *destArea = 1;
        SM64AP_SetWDWEntranceVariant(variant);
    } else if (level == LEVEL_TTC && variant >= SM64AP_ENTRANCE_TTC_STOPPED && variant <= SM64AP_ENTRANCE_TTC_FAST) {
        *destArea = 1;
        SM64AP_SetTTCEntranceVariantSpeed(variant);
    }
}

static int SM64AP_SourceEntranceKey(s16 destLevel, s16 destArea, s32 sourceEntrance) {
    if (sourceEntrance != 0) {
        return sourceEntrance;
    }

    switch (destLevel) {
        case LEVEL_LLL:
        case LEVEL_SSL:
        case LEVEL_TTM:
        case LEVEL_COTMC:
            return SM64AP_ENTRANCE_ID(destLevel, 1);
        default:
            return SM64AP_ENTRANCE_ID(destLevel, destArea);
    }
}

void SM64AP_RedirectWarp(s16* curLevel, s16* destLevel, s8* curArea, s16* destArea, s16* destWarpNode, bool isDeathWarp, int warpOp, s32 sourceEntrance) {
    // When warping, always lock the clock and reset var to avoid segfault if old clock val is not in new area
    SM64AP_SetClockToTTCState();
    if (*destLevel == LEVEL_BOWSER_3 || *curLevel == LEVEL_BOWSER_3 ||
        *destLevel == LEVEL_BITS || *curLevel == LEVEL_BITS) return; // Dont play around with this one
    if (*destWarpNode >= WARP_NODE_CREDITS_MIN) return; // Credit Warps
    if ((*curLevel == LEVEL_CASTLE || *curLevel == LEVEL_CASTLE_COURTYARD || *curLevel == LEVEL_CASTLE_GROUNDS || *curLevel == LEVEL_HMC) && 
         *destLevel != LEVEL_CASTLE && *destLevel != LEVEL_CASTLE_COURTYARD && *destLevel != LEVEL_CASTLE_GROUNDS) {
        if (*curLevel == LEVEL_HMC && *destLevel != LEVEL_COTMC) return; // Safety Check: If in HMC only relevant warp is to COTMC
        int sourceKey = SM64AP_SourceEntranceKey(*destLevel, *destArea, sourceEntrance);
        int destination = SM64AP_GetMappedEntrance(sourceKey);
        if (*curLevel != LEVEL_HMC) { // HMC -> COTMC transition should not set new return point
            sm64_exit_return_to = *curLevel * 10 + *curArea;
            sm64_exit_orig_entrancelvl = sourceKey / 10;
        }
        SM64AP_ApplyEntranceDestination(destination, destLevel, destArea);
        *destWarpNode = 0x0A;
        return;
    }

    if ((*destLevel == LEVEL_CASTLE || *destLevel == LEVEL_CASTLE_COURTYARD || *destLevel == LEVEL_CASTLE_GROUNDS) && course_dest_supported.find(*curLevel) != course_dest_supported.end()) {
        if (*destLevel == LEVEL_CASTLE && (*destWarpNode == 0x1F || *destWarpNode == 0x00)) return; //Exit Course or Inter-Castle warp
        *destLevel = sm64_exit_return_to / 10;
        *destArea = sm64_exit_return_to % 10;
        setCourseNodeAndArea(sm64_exit_orig_entrancelvl, destWarpNode, isDeathWarp, warpOp);
        return;
    }
}

int SM64AP_EntranceToTTC() {
    return SM64AP_ENTRANCE_ID(LEVEL_TTC, 1);
}

void SM64AP_SetClockToTTCAction(int* action) {
    sm64_clockaction = action;
}

void SM64AP_SetClockToTTCState() {
    if (sm64_clockaction) *sm64_clockaction = 6;
    sm64_clockaction = nullptr;
}

void SM64AP_SetTTCEntranceSpeed(int speed) {
    switch (speed) {
        case TTC_SPEED_STOPPED:
            sm64_ttc_entrance_variant = SM64AP_ENTRANCE_TTC_STOPPED;
            break;
        case TTC_SPEED_SLOW:
            sm64_ttc_entrance_variant = SM64AP_ENTRANCE_TTC_SLOW;
            break;
        case TTC_SPEED_RANDOM:
            sm64_ttc_entrance_variant = SM64AP_ENTRANCE_TTC_RANDOM;
            break;
        case TTC_SPEED_FAST:
            sm64_ttc_entrance_variant = SM64AP_ENTRANCE_TTC_FAST;
            break;
    }
}

int SM64AP_GetTTCEntranceVariant() {
    return sm64_ttc_entrance_variant;
}

s16 SM64AP_GetWDWEntranceWaterLevel(s16 vanillaWaterLevel) {
    s16 waterLevel = vanillaWaterLevel;

    switch (sm64_wdw_entrance_variant) {
        case SM64AP_ENTRANCE_WDW_LOW:
            waterLevel = SM64AP_WDW_LOW_WATER_LEVEL;
            break;
        case SM64AP_ENTRANCE_WDW_MIDDLE:
            waterLevel = SM64AP_WDW_MIDDLE_WATER_LEVEL;
            break;
        case SM64AP_ENTRANCE_WDW_HIGH:
            waterLevel = SM64AP_WDW_HIGH_WATER_LEVEL;
            break;
    }

    sm64_wdw_entrance_variant = 0;
    return waterLevel;
}

void SM64AP_SetFirstBowserDoorCost(int amount) {
    sm64_cost_firstbowserdoor = amount;
}

void SM64AP_SetBasementDoorCost(int amount) {
    sm64_cost_basementdoor = amount;
}

void SM64AP_SetSecondFloorDoorCost(int amount) {
    sm64_cost_secondfloordoor = amount;
}

void SM64AP_SetMIPS1Cost(int amount) {
    sm64_cost_mips1 = amount;
}

void SM64AP_SetMIPS2Cost(int amount) {
    sm64_cost_mips2 = amount;
}

void SM64AP_SetStarsToFinish(int amount) {
    sm64_cost_endlessstairs = amount;
}

void SM64AP_SetCompletionType(int type) {
    sm64_completion_type = type;
}

void SM64AP_SetCourseMap(std::map<int,int> map) {
    map_entrances = map;
}

void SM64AP_SetMusicShuffleMode(int mode) {
    switch (mode) {
        case SM64AP_MUSIC_SHUFFLE_MAP:
        case SM64AP_MUSIC_SHUFFLE_RANDOM_ON_LOAD:
            sm64_music_shuffle_mode = mode;
            break;
        default:
            sm64_music_shuffle_mode = SM64AP_MUSIC_SHUFFLE_OFF;
            break;
    }
}

void SM64AP_SetGlobalCapDisplay(int enabled) {
    sm64_show_global_cap_display = enabled != 0;
}

static void SM64AP_ResetCoinStarRequirements() {
    for (int i = 0; i < SM64AP_NUM_COIN_STAR_REQUIREMENTS; i++) {
        sm64_coin_star_requirements[i] = SM64AP_DEFAULT_COIN_STAR_REQUIREMENT;
    }
}

static bool SM64AP_IsValidCoinStarRequirement(int amount) {
    return amount >= 1 && amount <= 999;
}

static void SM64AP_SetCoinStarRequirementByIndex(int courseIndex, int amount) {
    if (courseIndex >= 0
        && courseIndex < SM64AP_NUM_COIN_STAR_REQUIREMENTS
        && SM64AP_IsValidCoinStarRequirement(amount)) {
        sm64_coin_star_requirements[courseIndex] = amount;
    }
}

static void SM64AP_SkipJsonWhitespace(const std::string &text, std::string::size_type &pos);
static bool SM64AP_ConsumeJsonChar(const std::string &text, std::string::size_type &pos, char expected);

static bool SM64AP_ParseJsonInt(const std::string &text, std::string::size_type &pos, int &value) {
    SM64AP_SkipJsonWhitespace(text, pos);
    if (pos >= text.size()) {
        return false;
    }

    int sign = 1;
    if (text[pos] == '-') {
        sign = -1;
        pos++;
    }
    if (pos >= text.size() || !std::isdigit((unsigned char) text[pos])) {
        return false;
    }

    int parsed = 0;
    while (pos < text.size() && std::isdigit((unsigned char) text[pos])) {
        parsed = parsed * 10 + text[pos] - '0';
        pos++;
    }

    value = parsed * sign;
    return true;
}

static bool SM64AP_ParseJsonQuotedIntKey(const std::string &text, std::string::size_type &pos, int &key) {
    SM64AP_SkipJsonWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }
    pos++;

    int parsed = 0;
    if (pos >= text.size() || !std::isdigit((unsigned char) text[pos])) {
        return false;
    }
    while (pos < text.size() && std::isdigit((unsigned char) text[pos])) {
        parsed = parsed * 10 + text[pos] - '0';
        pos++;
    }
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }
    pos++;

    key = parsed;
    return true;
}

static bool SM64AP_ParseCoinStarRequirementArray(const std::string &rawRequirements, std::string::size_type &pos) {
    if (!SM64AP_ConsumeJsonChar(rawRequirements, pos, '[')) {
        return false;
    }

    SM64AP_SkipJsonWhitespace(rawRequirements, pos);
    if (pos < rawRequirements.size() && rawRequirements[pos] == ']') {
        pos++;
        return true;
    }

    for (int courseIndex = 0; courseIndex < SM64AP_NUM_COIN_STAR_REQUIREMENTS; courseIndex++) {
        int amount = SM64AP_DEFAULT_COIN_STAR_REQUIREMENT;
        if (!SM64AP_ParseJsonInt(rawRequirements, pos, amount)) {
            return false;
        }
        SM64AP_SetCoinStarRequirementByIndex(courseIndex, amount);

        SM64AP_SkipJsonWhitespace(rawRequirements, pos);
        if (pos < rawRequirements.size() && rawRequirements[pos] == ',') {
            pos++;
            continue;
        }
        if (pos < rawRequirements.size() && rawRequirements[pos] == ']') {
            pos++;
            return true;
        }
        return false;
    }

    SM64AP_SkipJsonWhitespace(rawRequirements, pos);
    if (pos < rawRequirements.size() && rawRequirements[pos] == ']') {
        pos++;
        return true;
    }

    return false;
}

static bool SM64AP_ParseCoinStarRequirementMap(const std::string &rawRequirements, std::string::size_type &pos) {
    if (!SM64AP_ConsumeJsonChar(rawRequirements, pos, '{')) {
        return false;
    }

    SM64AP_SkipJsonWhitespace(rawRequirements, pos);
    if (pos < rawRequirements.size() && rawRequirements[pos] == '}') {
        pos++;
        return true;
    }

    std::vector<std::pair<int, int>> entries;
    bool usesZeroBasedKeys = false;

    while (pos < rawRequirements.size()) {
        int key = 0;
        int amount = SM64AP_DEFAULT_COIN_STAR_REQUIREMENT;
        if (!SM64AP_ParseJsonQuotedIntKey(rawRequirements, pos, key)
            || !SM64AP_ConsumeJsonChar(rawRequirements, pos, ':')
            || !SM64AP_ParseJsonInt(rawRequirements, pos, amount)) {
            return false;
        }

        if (key == 0) {
            usesZeroBasedKeys = true;
        }
        entries.push_back(std::make_pair(key, amount));

        SM64AP_SkipJsonWhitespace(rawRequirements, pos);
        if (pos < rawRequirements.size() && rawRequirements[pos] == ',') {
            pos++;
            continue;
        }
        if (pos < rawRequirements.size() && rawRequirements[pos] == '}') {
            pos++;
            break;
        }
        return false;
    }

    for (const auto &entry : entries) {
        int courseIndex = usesZeroBasedKeys ? entry.first : entry.first - COURSE_MIN;
        SM64AP_SetCoinStarRequirementByIndex(courseIndex, entry.second);
    }

    return true;
}

static void SM64AP_SetCoinStarRequirements(std::string rawRequirements) {
    SM64AP_ResetCoinStarRequirements();

    std::string::size_type pos = 0;
    SM64AP_SkipJsonWhitespace(rawRequirements, pos);
    if (pos >= rawRequirements.size() || rawRequirements.compare(pos, 4, "null") == 0) {
        return;
    }

    bool parsed = false;
    if (rawRequirements[pos] == '[') {
        parsed = SM64AP_ParseCoinStarRequirementArray(rawRequirements, pos);
    } else if (rawRequirements[pos] == '{') {
        parsed = SM64AP_ParseCoinStarRequirementMap(rawRequirements, pos);
    }

    SM64AP_SkipJsonWhitespace(rawRequirements, pos);
    if (!parsed || pos != rawRequirements.size()) {
        SM64AP_ResetCoinStarRequirements();
    }
}

static void SM64AP_SetMusicMap(std::string rawMap) {
    map_music.clear();

    std::string::size_type pos = 0;
    if (!SM64AP_ConsumeJsonChar(rawMap, pos, '{')) {
        return;
    }

    SM64AP_SkipJsonWhitespace(rawMap, pos);
    if (pos < rawMap.size() && rawMap[pos] == '}') {
        return;
    }

    while (pos < rawMap.size()) {
        int key = 0;
        int value = 0;
        if (!SM64AP_ParseJsonQuotedIntKey(rawMap, pos, key)
            || !SM64AP_ConsumeJsonChar(rawMap, pos, ':')
            || !SM64AP_ParseJsonInt(rawMap, pos, value)) {
            map_music.clear();
            return;
        }

        map_music[key] = value;

        SM64AP_SkipJsonWhitespace(rawMap, pos);
        if (pos < rawMap.size() && rawMap[pos] == ',') {
            pos++;
            continue;
        }
        if (pos < rawMap.size() && rawMap[pos] == '}') {
            return;
        }

        map_music.clear();
        return;
    }

    map_music.clear();
}

static void SM64AP_SkipJsonWhitespace(const std::string &text, std::string::size_type &pos) {
    while (pos < text.size() && std::isspace((unsigned char) text[pos])) {
        pos++;
    }
}

static bool SM64AP_ParseJsonByte(const std::string &text, std::string::size_type &pos, u8 &value) {
    SM64AP_SkipJsonWhitespace(text, pos);
    if (pos >= text.size() || !std::isdigit((unsigned char) text[pos])) {
        return false;
    }

    int parsed = 0;
    while (pos < text.size() && std::isdigit((unsigned char) text[pos])) {
        parsed = parsed * 10 + text[pos] - '0';
        if (parsed > 255) {
            return false;
        }
        pos++;
    }

    value = parsed;
    return true;
}

static bool SM64AP_ConsumeJsonChar(const std::string &text, std::string::size_type &pos, char expected) {
    SM64AP_SkipJsonWhitespace(text, pos);
    if (pos >= text.size() || text[pos] != expected) {
        return false;
    }
    pos++;
    return true;
}

static bool SM64AP_ReadMarioColor(const std::string &rawColors, const char *key, u8 color[3]) {
    std::string quotedKey = std::string("\"") + key + "\"";
    std::string::size_type keyPos = rawColors.find(quotedKey);
    if (keyPos == std::string::npos) {
        return false;
    }

    std::string::size_type pos = rawColors.find(':', keyPos + quotedKey.size());
    if (pos == std::string::npos) {
        return false;
    }
    pos++;

    if (!SM64AP_ConsumeJsonChar(rawColors, pos, '[')
        || !SM64AP_ParseJsonByte(rawColors, pos, color[0])
        || !SM64AP_ConsumeJsonChar(rawColors, pos, ',')
        || !SM64AP_ParseJsonByte(rawColors, pos, color[1])
        || !SM64AP_ConsumeJsonChar(rawColors, pos, ',')
        || !SM64AP_ParseJsonByte(rawColors, pos, color[2])
        || !SM64AP_ConsumeJsonChar(rawColors, pos, ']')) {
        return false;
    }

    return true;
}

static void SM64AP_ResetMarioColors() {
    SM64AP_SetMarioShirtColor(255, 0, 0);
    SM64AP_SetMarioCapShirtColor(255, 0, 0);
    SM64AP_SetMarioOverallsColor(0, 0, 255);
    SM64AP_SetMarioGlovesColor(255, 255, 255);
    SM64AP_SetMarioCapGlovesColor(255, 255, 255);
    SM64AP_SetMarioShoesColor(114, 28, 14);
    SM64AP_SetMarioSkinColor(254, 193, 121);
    SM64AP_SetMarioHairColor(115, 6, 0);
    SM64AP_ResetMarioSideburnColor();
    SM64AP_SetMarioCapHairColor(115, 6, 0);
}

static void SM64AP_SetMarioColors(std::string rawColors) {
    SM64AP_ResetMarioColors();

    u8 color[3];
    if (SM64AP_ReadMarioColor(rawColors, "shirt", color)) {
        SM64AP_SetMarioShirtColor(color[0], color[1], color[2]);
        SM64AP_SetMarioCapShirtColor(color[0], color[1], color[2]);
    }
    if (SM64AP_ReadMarioColor(rawColors, "overalls", color)) {
        SM64AP_SetMarioOverallsColor(color[0], color[1], color[2]);
    }
    if (SM64AP_ReadMarioColor(rawColors, "gloves", color)) {
        SM64AP_SetMarioGlovesColor(color[0], color[1], color[2]);
        SM64AP_SetMarioCapGlovesColor(color[0], color[1], color[2]);
    }
    if (SM64AP_ReadMarioColor(rawColors, "shoes", color)) {
        SM64AP_SetMarioShoesColor(color[0], color[1], color[2]);
    }
    if (SM64AP_ReadMarioColor(rawColors, "skin", color)) {
        SM64AP_SetMarioSkinColor(color[0], color[1], color[2]);
    }
    if (SM64AP_ReadMarioColor(rawColors, "hair", color)) {
        SM64AP_SetMarioHairColor(color[0], color[1], color[2]);
        SM64AP_SetMarioSideburnColor(color[0], color[1], color[2]);
        SM64AP_SetMarioCapHairColor(color[0], color[1], color[2]);
    }
}

void SM64AP_SetMoveRandoVec(int vec) {
    for (int i = 1; i < SM64AP_NUM_ABILITIES; i++) { // Start at 1, DJ bit is unnecessary
        sm64_have_abilities[i] = !std::bitset<SM64AP_NUM_ABILITIES>(vec).test(i) || sm64_have_abilities[i];
    }
}
void SM64AP_SetPaintingRando(int enabled) {
    sm64_painting_rando_enabled = enabled != 0;
    if(!sm64_painting_rando_enabled) {
        // Not enabled, so unlock all paintings
        for (int i = 0; i < NUM_PAINTING_LOCKS; i++)
            sm64_have_painting[i] = true;
    }
}

void SM64AP_ResetItems() {
    for (int i = 0; i < SM64AP_NUM_LOCS; i++) {
        sm64_locations[i] = false;
    }
    for (int i = 0; i < 15; i++) {
        sm64_have_cannon[i] = false;
    }
    for (int i = 0; i < NUM_PAINTING_LOCKS; i++) {
        sm64_have_painting[i] = false;
    }
    sm64_painting_rando_enabled = false;
    sm64_have_abilities.reset();
    sm64_have_features.reset();
    sm64_have_level_caps.reset();
    sm64_have_object_items.reset();
    sm64_have_first_floor_key = false;
    sm64_have_progressive_basement_keys = 0;
    sm64_have_progressive_upstairs_keys = 0;
    sm64_have_progressive_keys = 0;
    sm64_have_progressive_mips = 0;
    sm64_have_wing_cap_light = false;
    sm64_have_bbh = false;
    sm64_have_toads = false;
    sm64_have_castle_cannon = false;
    sm64_have_yoshi = false;
    sm64_have_wingcap = false;
    sm64_have_metalcap = false;
    sm64_have_vanishcap = false;
    starsCollected = 0;

    AP_SetServerDataRequest moat_request;
    moat_request.key = AP_GetPrivateServerDataPrefix() + "MoatDrained";
    moat_request.type = AP_DataType::Int;
    int def_val = 0;
    moat_request.operations = {{ "default", &def_val }};
    moat_request.default_value = &def_val;
    moat_request.want_reply = true;
    AP_SetServerData(&moat_request);
}

void SM64AP_SetReplyHandler(AP_SetReply reply) {
    if (reply.key == AP_GetPrivateServerDataPrefix() + "FinishedBowser") {
        switch (sm64_completion_type) {
            case 0: // Only BitS
                if ((*(int*)(reply.value) & 0b100) > 0) AP_StoryComplete();
                break;
            case 1: // All Bowser Stages
                if (*(int*)(reply.value) == 0b111) AP_StoryComplete();
                break;
        }
    } else if (reply.key == AP_GetPrivateServerDataPrefix() + "MoatDrained") {
        sm64_moat_state = *(int *) (reply.value);
    }
}

void SM64AP_GenericInit() {
    AP_SetDeathLinkSupported(true);
    AP_SetItemClearCallback(&SM64AP_ResetItems);
    AP_SetLocationCheckedCallback(&SM64AP_CheckLocation);
    AP_SetItemRecvCallback(&SM64AP_RecvItem);
    AP_RegisterSetReplyCallback(&SM64AP_SetReplyHandler);
    AP_SetNotify(AP_GetPrivateServerDataPrefix() + "FinishedBowser", AP_DataType::Int);
    AP_SetNotify(AP_GetPrivateServerDataPrefix() + "MoatDrained", AP_DataType::Int);

    AP_RegisterSlotDataIntCallback("FirstBowserDoorCost", &SM64AP_SetFirstBowserDoorCost);
    AP_RegisterSlotDataIntCallback("BasementDoorCost", &SM64AP_SetBasementDoorCost);
    AP_RegisterSlotDataIntCallback("SecondFloorDoorCost", &SM64AP_SetSecondFloorDoorCost);
    AP_RegisterSlotDataIntCallback("MIPS1Cost", &SM64AP_SetMIPS1Cost);
    AP_RegisterSlotDataIntCallback("MIPS2Cost", &SM64AP_SetMIPS2Cost);
    AP_RegisterSlotDataIntCallback("StarsToFinish", &SM64AP_SetStarsToFinish);
    AP_RegisterSlotDataIntCallback("CompletionType", &SM64AP_SetCompletionType);
    AP_RegisterSlotDataIntCallback("MoveRandoVec", &SM64AP_SetMoveRandoVec);
    AP_RegisterSlotDataIntCallback("PaintingRando", &SM64AP_SetPaintingRando);
    AP_RegisterSlotDataIntCallback("GlobalCapItems", &SM64AP_SetGlobalCapDisplay);
    AP_RegisterSlotDataIntCallback("ShowGlobalCapDisplay", &SM64AP_SetGlobalCapDisplay);
    AP_RegisterSlotDataMapIntIntCallback("AreaRando", &SM64AP_SetCourseMap);
    AP_RegisterSlotDataIntCallback("MusicShuffleMode", &SM64AP_SetMusicShuffleMode);
    AP_RegisterSlotDataRawCallback("MusicMap", static_cast<void (*)(std::string)>(&SM64AP_SetMusicMap));
    AP_RegisterSlotDataRawCallback("MarioColors", &SM64AP_SetMarioColors);
    AP_RegisterSlotDataRawCallback("CoinStarRequirements", &SM64AP_SetCoinStarRequirements);

    course_dest_supported = {
        LEVEL_BOB, LEVEL_WF, LEVEL_JRB, LEVEL_CCM, LEVEL_BBH, LEVEL_HMC, LEVEL_LLL, LEVEL_SSL, LEVEL_DDD, LEVEL_SL,
        LEVEL_WDW, LEVEL_TTM, LEVEL_THI, LEVEL_TTC, LEVEL_RR, LEVEL_PSS, LEVEL_SA, LEVEL_BITDW, LEVEL_TOTWC, LEVEL_COTMC,
        LEVEL_VCUTM, LEVEL_BITFS, LEVEL_WMOTR, LEVEL_BOWSER_1, LEVEL_BOWSER_2, LEVEL_BOWSER_3
    };
    
    map_boxid_locid[LEVEL_CCM*10 + 1] = 3626215;
    map_boxid_locid[LEVEL_CCM*10 + 2] = 3626216;
    map_boxid_locid[LEVEL_CCM*10 + 3] = 3626217;
    map_boxid_locid[LEVEL_BBH*10 + 1] = 3626218;
    map_boxid_locid[LEVEL_HMC*10 + 1] = 3626219;
    map_boxid_locid[LEVEL_HMC*10 + 2] = 3626220;
    map_boxid_locid[LEVEL_SSL*10 + 1] = 3626221;
    map_boxid_locid[LEVEL_SSL*10 + 2] = 3626222;
    map_boxid_locid[LEVEL_SSL*10 + 3] = 3626223;
    map_boxid_locid[LEVEL_SL*10 + 1] = 3626224;
    map_boxid_locid[LEVEL_SL*10 + 2] = 3626225;
    map_boxid_locid[LEVEL_WDW*10 + 2] = 3626226; // Uses first bit as flag for something, makes mario invisible :/
    map_boxid_locid[LEVEL_TTM*10 + 1] = 3626227;
    map_boxid_locid[LEVEL_THI*10 + 1] = 3626228;
    map_boxid_locid[LEVEL_THI*10 + 2] = 3626229;
    map_boxid_locid[LEVEL_THI*10 + 3] = 3626230;
    map_boxid_locid[LEVEL_TTC*10 + 1] = 3626231;
    map_boxid_locid[LEVEL_TTC*10 + 2] = 3626232;
    map_boxid_locid[LEVEL_RR*10 + 1] = 3626233;
    map_boxid_locid[LEVEL_RR*10 + 2] = 3626234;
    map_boxid_locid[LEVEL_RR*10 + 3] = 3626235;
    map_boxid_locid[LEVEL_BITDW*10 + 1] = 3626236;
    map_boxid_locid[LEVEL_BITDW*10 + 2] = 3626237;
    map_boxid_locid[LEVEL_BITFS*10 + 1] = 3626238;
    map_boxid_locid[LEVEL_BITFS*10 + 2] = 3626239;
    map_boxid_locid[LEVEL_BITS*10 + 1] = 3626240;
    map_boxid_locid[LEVEL_COTMC*10 + 1] = 3626241;
    map_boxid_locid[LEVEL_VCUTM*10 + 1] = 3626242;
    map_boxid_locid[LEVEL_WMOTR*10 + 1] = 3626243;
}

void SM64AP_InitMW(const char* ip, const char* player_name, const char* passwd) {
    AP_Init(ip, SM64AP_GAME_NAME, player_name, passwd);
    SM64AP_GenericInit();
    AP_Start();
}

void SM64AP_InitSP(const char * filename) {
    AP_Init(filename);
    SM64AP_GenericInit();
    AP_Start();
}

void SM64AP_SendByBoxID(int id) {
    SM64AP_SendItem(map_boxid_locid[id]);
}

void SM64AP_SendItem(int idx) {
    AP_SendItem(idx);
}

// If an item exists on the stack, return it, otherwise 0
int64_t SM64AP_PopDelayedStack() {
    if(delayed_queue.empty()) return 0;
    int64_t item = delayed_queue.front();
    delayed_queue.pop();
    return item;
}

void SM64AP_FinishBowser(int i) {
    AP_SetServerDataRequest req;
    req.key = AP_GetPrivateServerDataPrefix() + "FinishedBowser";
    int def_val = 0;
    req.default_value = &def_val;
    req.type = AP_DataType::Int;
    req.want_reply = true;
    int flag = 0b001 << i;
    req.operations = std::vector<AP_DataStorageOperation>{{{"or", &flag}}};
    AP_SetServerData(&req);
}


void SM64AP_SetMoatDrained() {
    AP_SetServerDataRequest req;
    req.key = AP_GetPrivateServerDataPrefix() + "MoatDrained";
    req.type = AP_DataType::Int;
    req.want_reply = true;
    int new_val = 1;
    req.operations = std::vector<AP_DataStorageOperation>{ { { "replace", &new_val } } };
    AP_SetServerData(&req);
}

int SM64AP_GetStars() {
    return starsCollected;
}

int SM64AP_GetCoinStarRequirement(int courseNum) {
    int courseIndex = courseNum - COURSE_MIN;
    if (courseIndex >= 0 && courseIndex < SM64AP_NUM_COIN_STAR_REQUIREMENTS) {
        return sm64_coin_star_requirements[courseIndex];
    }
    return SM64AP_DEFAULT_COIN_STAR_REQUIREMENT;
}

int SM64AP_GetRequiredStars(int idprx) {
    switch (idprx) {
        case 8: // Star Door 8
            return sm64_cost_firstbowserdoor;
        case 30: // Star Door 30
            return sm64_cost_basementdoor;
        case 50: // Star Door 50
            return sm64_cost_secondfloordoor;
        case 70: // Star Door 70
            return sm64_cost_endlessstairs;
        case SM64AP_LOCATIONID_MIPS1: // MIPS 1
            return sm64_cost_mips1;
        case SM64AP_LOCATIONID_MIPS2: // MIPS 2
            return sm64_cost_mips2;
        default:
            return idprx;
    }
}

bool SM64AP_CheckedLoc(int x) {
    return sm64_locations[x - SM64AP_ID_OFFSET];
}

static bool SM64AP_IsMusicAreaKey(int key) {
    switch (key) {
        case SM64AP_ENTRANCE_ID(LEVEL_BBH, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_CCM, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_CCM, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_CASTLE, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_CASTLE, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_CASTLE, 3):
        case SM64AP_ENTRANCE_ID(LEVEL_HMC, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_SSL, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_SSL, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_SSL, 3):
        case SM64AP_ENTRANCE_ID(LEVEL_BOB, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_SL, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_SL, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_WDW, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_WDW, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_JRB, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_JRB, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_THI, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_THI, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_THI, 3):
        case SM64AP_ENTRANCE_ID(LEVEL_TTC, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_RR, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_CASTLE_GROUNDS, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_BITDW, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_VCUTM, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_BITFS, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_SA, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_BITS, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_LLL, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_LLL, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_DDD, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_DDD, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_WF, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_CASTLE_COURTYARD, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_PSS, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_COTMC, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_TOTWC, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_BOWSER_1, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_WMOTR, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_BOWSER_2, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_BOWSER_3, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_TTM, 1):
        case SM64AP_ENTRANCE_ID(LEVEL_TTM, 2):
        case SM64AP_ENTRANCE_ID(LEVEL_TTM, 3):
        case SM64AP_ENTRANCE_ID(LEVEL_TTM, 4):
            return true;
        default:
            return false;
    }
}

static bool SM64AP_IsValidMusicSeq(int seq) {
    if (seq < 0 || seq > 0xFF) {
        return false;
    }

    int seqId = seq & 0x7F;
    if (seqId >= SEQ_COUNT) {
        return false;
    }
#ifdef VERSION_JP
    if (seqId == SEQ_EVENT_CUTSCENE_LAKITU) {
        return false;
    }
#endif
    return true;
}

s16 SM64AP_ResolveAreaMusic(s16 level, s16 area, s16 vanillaSeq) {
    int key = SM64AP_ENTRANCE_ID(level, area);
    if (!SM64AP_IsMusicAreaKey(key)) {
        return vanillaSeq;
    }

    switch (sm64_music_shuffle_mode) {
        case SM64AP_MUSIC_SHUFFLE_MAP: {
            auto entry = map_music.find(key);
            if (entry != map_music.end() && SM64AP_IsValidMusicSeq(entry->second)) {
                return static_cast<s16>(entry->second);
            }
            return vanillaSeq;
        }

        case SM64AP_MUSIC_SHUFFLE_RANDOM_ON_LOAD: {
            constexpr int poolSize = sizeof(SM64AP_RANDOM_MUSIC_POOL) / sizeof(SM64AP_RANDOM_MUSIC_POOL[0]);
            return static_cast<s16>(SM64AP_RANDOM_MUSIC_POOL[random_u16() % poolSize]);
        }

        default:
            return vanillaSeq;
    }
}

bool SM64AP_HaveKey1() {
    return SM64AP_HaveCastleKey(SM64AP_CASTLE_KEY_BASEMENT);
}

bool SM64AP_HaveKey2() {
    return SM64AP_HaveCastleKey(SM64AP_CASTLE_KEY_UPSTAIRS);
}

bool SM64AP_HaveCastleKey(int key) {
    switch (key) {
        case SM64AP_CASTLE_KEY_FIRST_FLOOR:
            return sm64_have_first_floor_key || sm64_have_progressive_keys >= 1;
        case SM64AP_CASTLE_KEY_BASEMENT:
            return sm64_have_progressive_basement_keys >= 1 || sm64_have_progressive_keys >= 2;
        case SM64AP_CASTLE_KEY_BASEMENT_STAR:
            return sm64_have_progressive_basement_keys >= 2 || sm64_have_progressive_keys >= 3;
        case SM64AP_CASTLE_KEY_UPSTAIRS:
            return sm64_have_progressive_upstairs_keys >= 1 || sm64_have_progressive_keys >= 4;
        case SM64AP_CASTLE_KEY_50_STAR:
            return sm64_have_progressive_upstairs_keys >= 2 || sm64_have_progressive_keys >= 5;
        case SM64AP_CASTLE_KEY_70_STAR:
            return sm64_have_progressive_upstairs_keys >= 3 || sm64_have_progressive_keys >= 6;
    }

    return false;
}

bool SM64AP_HaveProgressiveMips(int tier) {
    return sm64_have_progressive_mips >= tier;
}

bool SM64AP_HaveWingCapLight() {
    return sm64_have_wing_cap_light;
}

bool SM64AP_HaveBBH() {
    return sm64_have_bbh;
}

bool SM64AP_HaveToads() {
    return sm64_have_toads;
}

bool SM64AP_HaveCastleCannon() {
    return sm64_have_castle_cannon;
}

bool SM64AP_HaveYoshi() {
    return sm64_have_yoshi;
}

static bool SM64AP_HaveLevelCap(int cap) {
    return cap >= 0 && cap < SM64AP_NUM_LEVEL_CAPS && sm64_have_level_caps[cap];
}

bool SM64AP_HaveLevelCapOrGlobal(int cap) {
    if (cap >= SM64AP_LEVEL_CAP_BOB_WING && cap <= SM64AP_LEVEL_CAP_WMOTR_WING) {
        return sm64_have_wingcap || SM64AP_HaveLevelCap(cap);
    }
    if (cap >= SM64AP_LEVEL_CAP_WF_METAL && cap <= SM64AP_LEVEL_CAP_BITDW_METAL) {
        return sm64_have_metalcap || SM64AP_HaveLevelCap(cap);
    }
    if (cap >= SM64AP_LEVEL_CAP_BBH_VANISH && cap <= SM64AP_LEVEL_CAP_WDW_VANISH) {
        return sm64_have_vanishcap || SM64AP_HaveLevelCap(cap);
    }

    return false;
}

static int SM64AP_LevelCapForCurrentLevel(int flag) {
    switch (flag) {
        case 2:
            switch (gCurrLevelNum) {
                case LEVEL_BOB:
                    return SM64AP_LEVEL_CAP_BOB_WING;
                case LEVEL_CASTLE_GROUNDS:
                    return SM64AP_LEVEL_CAP_CASTLE_WING;
                case LEVEL_LLL:
                    return SM64AP_LEVEL_CAP_LLL_WING;
                case LEVEL_SSL:
                    return SM64AP_LEVEL_CAP_SSL_WING;
                case LEVEL_TOTWC:
                    return SM64AP_LEVEL_CAP_TOTWC_WING;
                case LEVEL_WMOTR:
                    return SM64AP_LEVEL_CAP_WMOTR_WING;
            }
            break;
        case 4:
            switch (gCurrLevelNum) {
                case LEVEL_WF:
                    return SM64AP_LEVEL_CAP_WF_METAL;
                case LEVEL_JRB:
                    return SM64AP_LEVEL_CAP_JRB_METAL;
                case LEVEL_HMC:
                    return SM64AP_LEVEL_CAP_HMC_METAL;
                case LEVEL_DDD:
                    return SM64AP_LEVEL_CAP_DDD_METAL;
                case LEVEL_WDW:
                    return SM64AP_LEVEL_CAP_WDW_METAL;
                case LEVEL_COTMC:
                    return SM64AP_LEVEL_CAP_COTMC_METAL;
                case LEVEL_BITDW:
                    return SM64AP_LEVEL_CAP_BITDW_METAL;
            }
            break;
        case 8:
            switch (gCurrLevelNum) {
                case LEVEL_BBH:
                    return SM64AP_LEVEL_CAP_BBH_VANISH;
                case LEVEL_DDD:
                    return SM64AP_LEVEL_CAP_DDD_VANISH;
                case LEVEL_SL:
                    return SM64AP_LEVEL_CAP_SL_VANISH;
                case LEVEL_VCUTM:
                    return SM64AP_LEVEL_CAP_VCUTM_VANISH;
                case LEVEL_WDW:
                    return SM64AP_LEVEL_CAP_WDW_VANISH;
            }
            break;
    }

    return -1;
}

static bool SM64AP_HaveAnyLevelCap(int first, int last) {
    for (int cap = first; cap <= last; cap++) {
        if (SM64AP_HaveLevelCap(cap)) {
            return true;
        }
    }

    return false;
}

static int SM64AP_CountLevelCapRange(int first, int last) {
    int count = 0;
    for (int cap = first; cap <= last; cap++) {
        if (SM64AP_HaveLevelCap(cap)) {
            count++;
        }
    }

    return count;
}

bool SM64AP_HaveCap(int flag) {
    switch (flag) {
        case 2:
            return sm64_have_wingcap || SM64AP_HaveLevelCap(SM64AP_LevelCapForCurrentLevel(flag));
        case 4:
            return sm64_have_metalcap || SM64AP_HaveLevelCap(SM64AP_LevelCapForCurrentLevel(flag));
        case 8:
            return sm64_have_vanishcap || SM64AP_HaveLevelCap(SM64AP_LevelCapForCurrentLevel(flag));
        default:
            //Probably coin/1up or something
            return true;
    }
}

int SM64AP_CountLevelCaps(int flag) {
    switch (flag) {
        case 2:
            return SM64AP_CountLevelCapRange(SM64AP_LEVEL_CAP_BOB_WING, SM64AP_LEVEL_CAP_WMOTR_WING);
        case 4:
            return SM64AP_CountLevelCapRange(SM64AP_LEVEL_CAP_WF_METAL, SM64AP_LEVEL_CAP_BITDW_METAL);
        case 8:
            return SM64AP_CountLevelCapRange(SM64AP_LEVEL_CAP_BBH_VANISH, SM64AP_LEVEL_CAP_WDW_VANISH);
        default:
            return 0;
    }
}

bool SM64AP_HaveAnyCap(int flag) {
    switch (flag) {
        case 2:
            return sm64_have_wingcap
                || SM64AP_HaveAnyLevelCap(SM64AP_LEVEL_CAP_BOB_WING, SM64AP_LEVEL_CAP_WMOTR_WING);
        case 4:
            return sm64_have_metalcap
                || SM64AP_HaveAnyLevelCap(SM64AP_LEVEL_CAP_WF_METAL, SM64AP_LEVEL_CAP_BITDW_METAL);
        case 8:
            return sm64_have_vanishcap
                || SM64AP_HaveAnyLevelCap(SM64AP_LEVEL_CAP_BBH_VANISH, SM64AP_LEVEL_CAP_WDW_VANISH);
        default:
            return true;
    }
}

bool SM64AP_HaveGlobalCap(int flag) {
    switch (flag) {
        case 2:
            return sm64_have_wingcap;
        case 4:
            return sm64_have_metalcap;
        case 8:
            return sm64_have_vanishcap;
        default:
            return true;
    }
}

bool SM64AP_ShowGlobalCapDisplay() {
    return sm64_show_global_cap_display;
}

bool SM64AP_PressedSwitch(int flag) {
    switch (flag) {
        case 2:
            return SM64AP_CheckedLoc(SM64AP_ID_WINGCAP);
        case 4:
            return SM64AP_CheckedLoc(SM64AP_ID_METALCAP);
        case 8:
            return SM64AP_CheckedLoc(SM64AP_ID_VANISHCAP);
        default:
            // Shouldn't happen, but just in case, this shouldn't be pressed
            return false;
    }
}

bool SM64AP_HaveCannon(int courseIdx) {
    if (courseIdx < 15) return sm64_have_cannon[courseIdx];
    return true;
}

bool SM64AP_HavePainting(int courseIdx) {
    switch(courseIdx) {
        case 1:  // BOB painting is always unlocked
        case 5:  // BBH doesn't have a painting
        case 6:  // HMC has a painting but you get stuck in an infinite loop of falling in and getting pushed out, so let's not do that :)
        case 15: // RR doesn't have a painting
            return true;
        default:
            // courses are 1-indexed, the items are 0-indexed
            return sm64_have_painting[courseIdx-1];
    }
}

bool SM64AP_PaintingRandoEnabled() {
    return sm64_painting_rando_enabled;
}

bool SM64AP_MoatDrained() {
    return sm64_moat_state != 0;
}

bool SM64AP_DeathLinkPending() {
    return AP_DeathLinkPending();
}

void SM64AP_DeathLinkClear() {
    AP_DeathLinkClear();
}

void SM64AP_DeathLinkSend() {
    if (!SM64AP_DeathLinkPending()) {
        return AP_DeathLinkSend();
    } else {
        SM64AP_DeathLinkClear();
    }
}

bool SM64AP_CanDoubleJump() {
    return sm64_have_abilities[SM64AP_ID_DOUBLEJUMP - SM64AP_ABILITY_OFFSET]
           || sm64_have_abilities[SM64AP_ID_TRIPLEJUMP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanTripleJump() {
    return sm64_have_abilities[SM64AP_ID_TRIPLEJUMP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanLongJump() {
    return sm64_have_abilities[SM64AP_ID_LONGJUMP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanBackflip() {
    return sm64_have_abilities[SM64AP_ID_BACKFLIP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanSideFlip() {
    return sm64_have_abilities[SM64AP_ID_SIDEFLIP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanWallKick() {
    return sm64_have_abilities[SM64AP_ID_WALLKICK - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanDive() {
    return sm64_have_abilities[SM64AP_ID_DIVE - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanGroundPound() {
    return sm64_have_abilities[SM64AP_ID_GROUNDPOUND - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanKick() {
    return sm64_have_abilities[SM64AP_ID_KICK - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanClimb() {
    return sm64_have_abilities[SM64AP_ID_CLIMB - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanLedgeGrab() {
    return sm64_have_abilities[SM64AP_ID_LEDGEGRAB - SM64AP_ABILITY_OFFSET];
}


void SM64AP_PrintNext() {
    if (AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected) {
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 7, SCREEN_HEIGHT / 2, "Connecting");
    }
    if (AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused) {
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2, "CONNECTION REFUSED");
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2 - 20, "CHECK ARGS");
    }
    if (!sm64_have_abilities.all() && !SM64AP_SUPPORT_MOVE_RANDO) {
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2, "INCOMPATIBLE WITH");
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2 - 20, "MOUE RANDO");
    }
    if (!AP_IsMessagePending()) return;
    AP_Message* msg = AP_GetLatestMessage();
    if (msg->type == AP_MessageType::ItemSend) {
        AP_ItemSendMessage* o_msg = static_cast<AP_ItemSendMessage*>(msg);
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1-0)*20, (o_msg->item + std::string(" was sent")).c_str());
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1-1)*20, (std::string("to ") + o_msg->recvPlayer).c_str());
    } else if (msg->type == AP_MessageType::ItemRecv) {
        AP_ItemRecvMessage* o_msg = static_cast<AP_ItemRecvMessage*>(msg);
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1-0)*20, (std::string("Got ") + o_msg->item).c_str());
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1-1)*20, (std::string("From ") + o_msg->sendPlayer).c_str());
    } else if (msg->type == AP_MessageType::Countdown) {
        cur_msg_frame_duration = std::min(cur_msg_frame_duration, 30);
        AP_CountdownMessage* o_msg = static_cast<AP_CountdownMessage*>(msg);
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0) + SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, std::to_string(o_msg->timer).c_str());
    } else {
        //print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1-0)*20, msg->text.c_str());
    }
    if (cur_msg_frame_duration > 0) {
        cur_msg_frame_duration--;
    } else {
        AP_ClearLatestMessage();
        cur_msg_frame_duration = msg_frame_duration;
    }
}
