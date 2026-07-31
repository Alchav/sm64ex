// mushroom_1up.c.inc

#include "../../sm64ap.h"

static s16 bhv_1up_ap_source_type(void) {
    if (cur_obj_has_behavior(bhv1Up)) {
        return SM64AP_1UP_SOURCE_OBJECT;
    }
    if (cur_obj_has_behavior(bhv1upSliding)) {
        return SM64AP_1UP_SOURCE_SLIDING;
    }
    if (cur_obj_has_behavior(bhv1upJumpOnApproach)) {
        return SM64AP_1UP_SOURCE_JUMP_ON_APPROACH;
    }
    if (cur_obj_has_behavior(bhvHidden1up)) {
        return SM64AP_1UP_SOURCE_HIDDEN;
    }
    if (cur_obj_has_behavior(bhvHidden1upInPole)) {
        return SM64AP_1UP_SOURCE_HIDDEN_POLE;
    }
    if (cur_obj_has_behavior(bhv1upWalking)) {
        return SM64AP_1UP_SOURCE_WALKING;
    }
    if (cur_obj_has_behavior(bhv1upRunningAway)) {
        return SM64AP_1UP_SOURCE_RUNNING_AWAY;
    }

    return -1;
}

static void bhv_1up_assign_ap_location(void) {
    s16 sourceType;

    if (o->o1UpApLocationId != 0) {
        return;
    }

    sourceType = bhv_1up_ap_source_type();
    if (sourceType < 0) {
        return;
    }

    o->o1UpApLocationId = SM64AP_ResolveOneUpLocation(
        gCurrLevelNum, gCurrAreaIndex, sourceType, o->oBehParams2ndByte,
        (s16) o->oPosX, (s16) o->oPosY, (s16) o->oPosZ);
}

void bhv_1up_interact(void) {
    UNUSED s32 sp1C;

    bhv_1up_assign_ap_location();
    if (SM64AP_ShouldSuppressOneUp(o->o1UpApLocationId)) {
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        return;
    }

    if (obj_check_if_collided_with_object(o, gMarioObject) == 1) {
        play_sound(SOUND_GENERAL_COLLECT_1UP, gDefaultSoundArgs);
        if (!SM64AP_CollectOneUp(o->o1UpApLocationId)) {
            gMarioState->numLives++;
        }
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
    }
}

static void bhv_1up_collect_without_contact(void) {
    bhv_1up_assign_ap_location();
    if (SM64AP_ShouldSuppressOneUp(o->o1UpApLocationId)) {
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        return;
    }

    play_sound(SOUND_GENERAL_COLLECT_1UP, gDefaultSoundArgs);
    if (!SM64AP_CollectOneUp(o->o1UpApLocationId)) {
        gMarioState->numLives++;
    }
    o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
}

static bool bhv_1up_collect_on_no_despawn_floor(s16 collisionFlags) {
    if (!SM64AP_NoDespawn()) {
        return false;
    }

    if (sObjFloor != NULL && sObjFloor->type == SURFACE_DEATH_PLANE
        && o->oPosY < o->oFloorHeight + 2048.0f) {
        bhv_1up_collect_without_contact();
        return true;
    }

    if (sObjFloor == NULL && o->oPosY <= -10000.0f) {
        bhv_1up_collect_without_contact();
        return true;
    }

    if (!(collisionFlags & OBJ_COL_FLAG_GROUNDED)) {
        return false;
    }

    if (sObjFloor != NULL) {
        if (SURFACE_IS_LETHAL_QUICKSAND(sObjFloor->type)) {
            bhv_1up_collect_without_contact();
            return true;
        }

        switch (sObjFloor->type) {
            case SURFACE_BURNING:
            case SURFACE_DEATH_PLANE:
                bhv_1up_collect_without_contact();
                return true;
            default:
                break;
        }
    }

    return false;
}

void bhv_1up_common_init(void) {
    bhv_1up_assign_ap_location();
    if (SM64AP_ShouldSuppressOneUp(o->o1UpApLocationId)) {
        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        return;
    }

    o->oMoveAnglePitch = -0x4000;
    o->oGravity = 3.0f;
    o->oFriction = 1.0f;
    o->oBuoyancy = 1.0f;
}

void bhv_1up_init(void) {
    bhv_1up_common_init();
    if (o->oBehParams2ndByte == 1 || o->oBehParams2ndByte == 2) {
        if (!SM64AP_ShouldSpawnBowserStageOneUp(gCurrLevelNum, o->oBehParams2ndByte, save_file_get_flags())) {
            o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
        }
    }
}

void one_up_loop_in_air(void) {
    if (o->oTimer < 5) {
        o->oVelY = 40.0f;
    } else {
        o->oAngleVelPitch = -0x1000;
        o->oMoveAnglePitch += o->oAngleVelPitch;
        o->oVelY = coss(o->oMoveAnglePitch) * 30.0f + 2.0f;
        o->oForwardVel = -sins(o->oMoveAnglePitch) * 30.0f;
    }
}

void pole_1up_move_towards_mario(void) {
    f32 sp34 = gMarioObject->header.gfx.pos[0] - o->oPosX;
    f32 sp30 = gMarioObject->header.gfx.pos[1] + 120.0f - o->oPosY;
    f32 sp2C = gMarioObject->header.gfx.pos[2] - o->oPosZ;
    s16 sp2A = atan2s(sqrtf(sqr(sp34) + sqr(sp2C)), sp30);

    obj_turn_toward_object(o, gMarioObject, 16, 0x1000);
    o->oMoveAnglePitch = approach_s16_symmetric(o->oMoveAnglePitch, sp2A, 0x1000);
    o->oVelY = sins(o->oMoveAnglePitch) * 30.0f;
    o->oForwardVel = coss(o->oMoveAnglePitch) * 30.0f;
    bhv_1up_interact();
}

void one_up_move_away_from_mario(s16 sp1A) {
    if (bhv_1up_collect_on_no_despawn_floor(sp1A)) {
        return;
    }

    o->oForwardVel = 8.0f;
    o->oMoveAngleYaw = o->oAngleToMario + 0x8000;
    bhv_1up_interact();
    if (!SM64AP_NoDespawn() && (sp1A & 0x02))
        o->oAction = 2;

    if (!SM64AP_NoDespawn()
        && !is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 3000))
        o->oAction = 2;
}

void bhv_1up_walking_loop(void) {
    s16 collisionFlags = object_step();

    if (bhv_1up_collect_on_no_despawn_floor(collisionFlags)) {
        return;
    }

    switch (o->oAction) {
        case 0:
            if (o->oTimer >= 18)
                spawn_object(o, MODEL_NONE, bhvSparkleSpawn);

            if (o->oTimer == 0)
                play_sound(SOUND_GENERAL2_1UP_APPEAR, gDefaultSoundArgs);

            one_up_loop_in_air();

            if (o->oTimer == 37) {
                cur_obj_become_tangible();
                o->oAction = 1;
                o->oForwardVel = 2.0f;
            }
            break;

        case 1:
            if (!SM64AP_NoDespawn() && o->oTimer > 300)
                o->oAction = 2;

            bhv_1up_interact();
            break;

        case 2:
            if (!SM64AP_NoDespawn()) {
                obj_flicker_and_disappear(o, 30);
            }
            bhv_1up_interact();
            break;
    }

    set_object_visibility(o, 3000);
}

void bhv_1up_running_away_loop(void) {
    s16 sp26;

    sp26 = object_step();
    switch (o->oAction) {
        case 0:
            if (o->oTimer >= 18)
                spawn_object(o, MODEL_NONE, bhvSparkleSpawn);

            if (o->oTimer == 0)
                play_sound(SOUND_GENERAL2_1UP_APPEAR, gDefaultSoundArgs);

            one_up_loop_in_air();

            if (o->oTimer == 37) {
                cur_obj_become_tangible();
                o->oAction = 1;
                o->oForwardVel = 8.0f;
            }
            break;

        case 1:
            spawn_object(o, MODEL_NONE, bhvSparkleSpawn);
            one_up_move_away_from_mario(sp26);
            break;

        case 2:
            if (!SM64AP_NoDespawn()) {
                obj_flicker_and_disappear(o, 30);
            }
            bhv_1up_interact();
            break;
    }

    set_object_visibility(o, 3000);
}

void sliding_1up_move(void) {
    s16 sp1E;

    sp1E = object_step();
    if (bhv_1up_collect_on_no_despawn_floor(sp1E)) {
        return;
    }

    if (sp1E & 0x01) {
        o->oForwardVel += 25.0f;
        o->oVelY = 0;
    } else {
        o->oForwardVel *= 0.98;
    }

    if (o->oForwardVel > 40.0)
        o->oForwardVel = 40.0f;

    if (!SM64AP_NoDespawn()
        && !is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 5000))
        o->oAction = 2;
}

void bhv_1up_sliding_loop(void) {
    switch (o->oAction) {
        case 0:
            set_object_visibility(o, 3000);
            if (is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 1000))
                o->oAction = 1;
            break;

        case 1:
            sliding_1up_move();
            break;

        case 2:
            if (!SM64AP_NoDespawn()) {
                obj_flicker_and_disappear(o, 30);
            }
            bhv_1up_interact();
            break;
    }

    bhv_1up_interact();
    spawn_object(o, MODEL_NONE, bhvSparkleSpawn);
}

void bhv_1up_loop(void) {
    bhv_1up_interact();
    set_object_visibility(o, 3000);
}

void bhv_1up_jump_on_approach_loop(void) {
    s16 sp26;

    switch (o->oAction) {
        case 0:
            if (is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 1000)) {
                o->oVelY = 40.0f;
                o->oAction = 1;
            }
            break;

        case 1:
            sp26 = object_step();
            one_up_move_away_from_mario(sp26);
            spawn_object(o, MODEL_NONE, bhvSparkleSpawn);
            break;

        case 2:
            sp26 = object_step();
            if (bhv_1up_collect_on_no_despawn_floor(sp26)) {
                break;
            }
            bhv_1up_interact();
            if (!SM64AP_NoDespawn()) {
                obj_flicker_and_disappear(o, 30);
            }
            break;
    }

    set_object_visibility(o, 3000);
}

void bhv_1up_hidden_loop(void) {
    s16 sp26;
    switch (o->oAction) {
        case 0:
            o->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
            if (o->o1UpHiddenUnkF4 == o->oBehParams2ndByte) {
                o->oVelY = 40.0f;
                o->oAction = 3;
                o->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
                play_sound(SOUND_GENERAL2_1UP_APPEAR, gDefaultSoundArgs);
            }
            break;

        case 1:
            sp26 = object_step();
            one_up_move_away_from_mario(sp26);
            spawn_object(o, MODEL_NONE, bhvSparkleSpawn);
            break;

        case 2:
            sp26 = object_step();
            if (bhv_1up_collect_on_no_despawn_floor(sp26)) {
                break;
            }
            bhv_1up_interact();
            if (!SM64AP_NoDespawn()) {
                obj_flicker_and_disappear(o, 30);
            }
            break;

        case 3:
            sp26 = object_step();
            if (o->oTimer >= 18)
                spawn_object(o, MODEL_NONE, bhvSparkleSpawn);

            one_up_loop_in_air();

            if (o->oTimer == 37) {
                cur_obj_become_tangible();
                o->oAction = 1;
                o->oForwardVel = 8.0f;
            }
            break;
    }
}

void bhv_1up_hidden_trigger_loop(void) {
    struct Object *sp1C;
    struct Object *sparkle;

    if (!(o->oActiveParticleFlags & ACTIVE_PARTICLE_SPARKLES) && (o->oTimer % 15) == 0) {
        o->oActiveParticleFlags |= ACTIVE_PARTICLE_SPARKLES;
        sparkle = spawn_object_at_origin(o, 0, MODEL_GREEN_SPARKLES, bhvSparkleParticleSpawner);
        obj_copy_pos_and_angle(sparkle, o);
    }

    if (obj_check_if_collided_with_object(o, gMarioObject) == 1) {
        sp1C = cur_obj_nearest_object_with_behavior(bhvHidden1up);
        if (sp1C != NULL)
            sp1C->o1UpHiddenUnkF4++;

        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
    }
}

void bhv_1up_hidden_in_pole_loop(void) {
    UNUSED s16 sp26;
    switch (o->oAction) {
        case 0:
            o->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
            if (o->o1UpHiddenUnkF4 == o->oBehParams2ndByte) {
                o->oVelY = 40.0f;
                o->oAction = 3;
                o->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
                play_sound(SOUND_GENERAL2_1UP_APPEAR, gDefaultSoundArgs);
            }
            break;

        case 1:
            pole_1up_move_towards_mario();
            sp26 = object_step();
            bhv_1up_collect_on_no_despawn_floor(sp26);
            break;

        case 3:
            sp26 = object_step();
            if (bhv_1up_collect_on_no_despawn_floor(sp26)) {
                break;
            }
            if (o->oTimer >= 18)
                spawn_object(o, MODEL_NONE, bhvSparkleSpawn);

            one_up_loop_in_air();

            if (o->oTimer == 37) {
                cur_obj_become_tangible();
                o->oAction = 1;
                o->oForwardVel = 10.0f;
            }
            break;
    }
}

void bhv_1up_hidden_in_pole_trigger_loop(void) {
    struct Object *sp1C;

	if (!(o->oActiveParticleFlags & ACTIVE_PARTICLE_SPARKLES) && (o->oTimer % 15) == 0) {
    o->oActiveParticleFlags |= ACTIVE_PARTICLE_SPARKLES;
    obj_copy_pos_and_angle(spawn_object_at_origin(o, 0, MODEL_GREEN_SPARKLES, bhvSparkleParticleSpawner), o);
	}

    if (obj_check_if_collided_with_object(o, gMarioObject) == 1) {
        sp1C = cur_obj_nearest_object_with_behavior(bhvHidden1upInPole);
        if (sp1C != NULL) {
            sp1C->o1UpHiddenUnkF4++;
            ;
        }

        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
    }
}

void bhv_1up_hidden_in_pole_spawner_loop(void) {
    s8 sp2F;
    s32 oneUpLocId;
    struct Object *oneUp;

    if (is_point_within_radius_of_mario(o->oPosX, o->oPosY, o->oPosZ, 700)) {
        oneUpLocId = SM64AP_ResolveOneUpLocation(
            gCurrLevelNum, gCurrAreaIndex, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0,
            (s16) o->oPosX, (s16) o->oPosY, (s16) o->oPosZ);

        if (!SM64AP_ShouldSuppressOneUp(oneUpLocId)) {
            oneUp = spawn_object_relative(2, 0, 50, 0, o, MODEL_1UP, bhvHidden1upInPole);
            oneUp->o1UpApLocationId = oneUpLocId;
            for (sp2F = 0; sp2F < 2; sp2F++) {
                spawn_object_relative(0, 0, sp2F * -200, 0, o, MODEL_NONE, bhvHidden1upInPoleTrigger);
            }
        }

        o->activeFlags = ACTIVE_FLAG_DEACTIVATED;
    }
}
