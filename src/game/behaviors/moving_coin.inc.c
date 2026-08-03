// coin.c.inc

#include "../../sm64ap.h"

// sp18 = collisionFlagsPtr

static struct ObjectHitbox sMovingYellowCoinHitbox = {
    /* interactType:      */ INTERACT_COIN,
    /* downOffset:        */ 0,
    /* damageOrCoinValue: */ 1,
    /* health:            */ 0,
    /* numLootCoins:      */ 0,
    /* radius:            */ 100,
    /* height:            */ 64,
    /* hurtboxRadius:     */ 0,
    /* hurtboxHeight:     */ 0,
};

static struct ObjectHitbox sMovingBlueCoinHitbox = {
    /* interactType:      */ INTERACT_COIN,
    /* downOffset:        */ 0,
    /* damageOrCoinValue: */ 5,
    /* health:            */ 0,
    /* numLootCoins:      */ 0,
    /* radius:            */ 100,
    /* height:            */ 64,
    /* hurtboxRadius:     */ 0,
    /* hurtboxHeight:     */ 0,
};

static void moving_coin_collect_without_contact(void);

static bool moving_coin_collect_on_no_despawn_floor(s16 collisionFlags) {
    if (!SM64AP_NoDespawn()) {
        return false;
    }

    if (sObjFloor != NULL && sObjFloor->type == SURFACE_DEATH_PLANE
        && o->oPosY < o->oFloorHeight + 2048.0f) {
        moving_coin_collect_without_contact();
        return true;
    }

    if (sObjFloor == NULL && o->oPosY <= -10000.0f) {
        moving_coin_collect_without_contact();
        return true;
    }

    if (!(collisionFlags & OBJ_COL_FLAG_GROUNDED)) {
        return false;
    }

    if (sObjFloor != NULL) {
        if (SURFACE_IS_LETHAL_QUICKSAND(sObjFloor->type)) {
            moving_coin_collect_without_contact();
            return true;
        }

        switch (sObjFloor->type) {
            case SURFACE_BURNING:
            case SURFACE_DEATH_PLANE:
                moving_coin_collect_without_contact();
                return true;
            default:
                break;
        }
    }

    return false;
}

s32 coin_step(s16 *collisionFlagsPtr) {
    *collisionFlagsPtr = object_step();

    obj_check_floor_death(*collisionFlagsPtr, sObjFloor);
    if (moving_coin_collect_on_no_despawn_floor(*collisionFlagsPtr)) {
        return 0;
    }

    if ((*collisionFlagsPtr & 0x1) != 0 && (*collisionFlagsPtr & 0x8) == 0) /* bit 0, bit 3 */
    {
        cur_obj_play_sound_2(SOUND_GENERAL_COIN_DROP);
        return 1;
    }

    return 0;
}

void moving_coin_flicker(void) {
    s16 collisionFlags;

    coin_step(&collisionFlags);
    if (!SM64AP_NoDespawn()) {
        obj_flicker_and_disappear(o, 0);
    }
}

void coin_collected(void) {
    spawn_object(o, MODEL_SPARKLES, bhvGoldenCoinSparkles);
    o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
}

static void moving_coin_collect_without_contact(void) {
    s32 coinValue = o->oDamageOrCoinValue;
    s32 coinStarRequirement;

    if (coinValue <= 0) {
        coinValue = cur_obj_has_model(MODEL_BLUE_COIN) ? 5 : 1;
    }

    if (o->apCoinSpent || !SM64AP_CollectPermanentCoin(o, coinValue)) {
        gMarioState->healCounter += 4 * coinValue;
        spawn_outstanding_permanent_coin_star();
        coin_collected();
        return;
    }

    gMarioState->numCoins += coinValue;
    gMarioState->healCounter += 4 * coinValue;
    SM64AP_CheckCoinCount(gCurrCourseNum, gMarioState->numCoins);
    spawn_outstanding_permanent_coin_star();

    coinStarRequirement = SM64AP_GetCoinStarRequirement(gCurrCourseNum);
    if (COURSE_IS_MAIN_COURSE(gCurrCourseNum)
        && !SM64AP_PermanentCoinCollection()
        && !SM64AP_CollectedCourseStar(gCurrCourseNum - COURSE_MIN, 6)
        && gMarioState->numCoins - coinValue < coinStarRequirement
        && gMarioState->numCoins >= coinStarRequirement) {
        bhv_spawn_star_no_level_exit(6);
    }

    if (coinValue >= 2) {
        queue_rumble_data(5, 80);
    }

    coin_collected();
}

void bhv_moving_yellow_coin_init(void) {
    o->oGravity = 3.0f;
    o->oFriction = 1.0f;
    o->oBuoyancy = 1.5f;

    obj_set_hitbox(o, &sMovingYellowCoinHitbox);
    if (o->apCoinSourceKind == 2
        && !SM64AP_AssignPermanentCoinOutput(o->parentObj, o, 1, 1)) {
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        return;
    }
    SM64AP_MarkSpentPermanentCoin(o, 1);
}

void bhv_moving_yellow_coin_loop(void) {
    s16 collisionFlags;
    if (o->apCoinSpent) {
        cur_obj_set_model(MODEL_SPENT_COIN);
    }
    switch (o->oAction) {
        case MOV_YCOIN_ACT_IDLE:
            coin_step(&collisionFlags);

            if (o->oTimer < 10)
                cur_obj_become_intangible();
            else
                cur_obj_become_tangible();

            if (!SM64AP_NoDespawn() && o->oTimer >= 301)
                o->oAction = 1;
            break;

        case MOV_YCOIN_ACT_BLINKING:
            moving_coin_flicker();
            break;

        case MOV_YCOIN_ACT_LAVA_DEATH:
            if (SM64AP_NoDespawn()) {
                moving_coin_collect_without_contact();
                break;
            }
            o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
            break;

        case MOV_YCOIN_ACT_DEATH_PLANE_DEATH:
            if (SM64AP_NoDespawn()) {
                moving_coin_collect_without_contact();
                break;
            }
            o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
            break;
    }

    if ((o->oInteractStatus & INT_STATUS_INTERACTED) != 0) /* bit 15 */
    {
        coin_collected();
        o->oInteractStatus = 0;
    }
}

void bhv_moving_blue_coin_init(void) {
    o->oGravity = 5.0f;
    o->oFriction = 1.0f;
    o->oBuoyancy = 1.5f;

    obj_set_hitbox(o, &sMovingBlueCoinHitbox);
    if (o->apCoinSourceKind == 2
        && !SM64AP_AssignPermanentCoinOutput(o->parentObj, o, 5, 1)) {
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        return;
    }
    SM64AP_MarkSpentPermanentCoin(o, 5);
}

void bhv_moving_blue_coin_loop(void) {
#ifdef VERSION_EU
    s32 collisionFlags;
#else
    s16 collisionFlags;
#endif

    if (o->apCoinSpent) {
        cur_obj_set_model(MODEL_SPENT_BLUE_COIN);
    }

    switch (o->oAction) {
        case MOV_BCOIN_ACT_STILL:
            if (is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 1500))
                o->oAction = 1;
            break;

        case MOV_BCOIN_ACT_MOVING:
            collisionFlags = object_step();
            if ((collisionFlags & OBJ_COL_FLAG_GROUNDED)) /* bit 0 */
            {
                o->oForwardVel += 25.0f;
                if (!(collisionFlags & OBJ_COL_FLAG_NO_Y_VEL))
                    cur_obj_play_sound_2(SOUND_GENERAL_COIN_DROP); /* bit 3 */
            } else
                o->oForwardVel *= 0.98;

            if (o->oForwardVel > 75.0)
                o->oForwardVel = 75.0f;

            if (!SM64AP_NoDespawn()) {
                obj_flicker_and_disappear(o, 600);
            }
            break;
    }

    if ((o->oInteractStatus & INT_STATUS_INTERACTED) != 0) /* bit 15 */
    {
        coin_collected();
        o->oInteractStatus = 0;
    }
}

void bhv_blue_coin_sliding_jumping_init(void) {
    o->oGravity = 3.0;
    o->oFriction = 0.98;
    o->oBuoyancy = 1.5;

    obj_set_hitbox(o, &sMovingBlueCoinHitbox);
}

void blue_coin_sliding_away_from_mario(void) {
    s16 collisionFlags;

    o->oForwardVel = 15.0;
    o->oMoveAngleYaw = o->oAngleToMario + 0x8000;

    if (coin_step(&collisionFlags) != 0)
        o->oVelY += 18.0f;
    if ((collisionFlags & 0x2) != 0)
        o->oAction = 3; /* bit 1 */

    if (is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 1000) == 0)
        o->oAction = 2;
}

void blue_coin_sliding_slow_down(void) {
    s16 collisionFlags;

    coin_step(&collisionFlags);

    if (is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 500) == 1)
        o->oAction = 1;

    if (!SM64AP_NoDespawn() && o->oTimer >= 151)
        o->oAction = 3;
}

void bhv_blue_coin_sliding_loop(void) {
    s16 collisionFlags;

    switch (o->oAction) {
        case 0:
            if (is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 500) == 1)
                o->oAction = 1;

            set_object_visibility(o, 3000);
            break;

        case 1:
            blue_coin_sliding_away_from_mario();
            break;

        case 2:
            blue_coin_sliding_slow_down();
            set_object_visibility(o, 3000);
            break;

        case 3:
            coin_step(&collisionFlags);
            if (!SM64AP_NoDespawn() && o->oTimer >= 61)
                o->oAction = 4;
            break;

        case 4:
            moving_coin_flicker();
            break;

        case 100:
            if (SM64AP_NoDespawn()) {
                moving_coin_collect_without_contact();
                break;
            }
            o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
            break;

        case 101:
            if (SM64AP_NoDespawn()) {
                moving_coin_collect_without_contact();
                break;
            }
            o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
            break;
    }

    if ((o->oInteractStatus & INT_STATUS_INTERACTED) != 0) /* bit 15 */
    {
        coin_collected();
        o->oInteractStatus = 0;
    }
}

void bhv_blue_coin_jumping_loop(void) {
    s16 collisionFlags;

    switch (o->oAction) {
        case 0:
            if (o->oTimer == 0) {
                cur_obj_become_intangible();
                o->oVelY = 50.0;
            }

            object_step();

            if (o->oTimer == 15) {
                cur_obj_become_tangible();
                o->oAction = 1;
            }
            break;

        case 1:
            blue_coin_sliding_away_from_mario();
            break;

        case 2:
            blue_coin_sliding_slow_down();
            set_object_visibility(o, 3000);
            break;

        case 3:
            coin_step(&collisionFlags);
            if (!SM64AP_NoDespawn() && o->oTimer >= 61)
                o->oAction = 4;
            break;

        case 4:
            moving_coin_flicker();
            break;
    }

    if ((o->oInteractStatus & INT_STATUS_INTERACTED) != 0) /* bit 15 */
    {
        coin_collected();
        o->oInteractStatus = 0;
    }
}
