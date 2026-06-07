/**
 * Main loop of the hour and minute hands of the Tick Tock Clock painting.
 */

#include "sm64ap.h"

static s16 ttc_speed_setting_from_roll_angle(u16 rollAngle) {
    // The angle counts down from 0xffff to 0, so 11 o'clock is a small value and 1 o'clock is large.
    if (rollAngle < 0xAAA)
        return TTC_SPEED_STOPPED;
    else if (rollAngle < 0x6aa4)
        return TTC_SPEED_FAST;
    else if (rollAngle < 0x954C)
        return TTC_SPEED_RANDOM;
    else if (rollAngle < 0xf546)
        return TTC_SPEED_SLOW;
    else
        return TTC_SPEED_STOPPED;
}

void bhv_rotating_clock_arm_loop(void) {
    if (o->oBehParams != SM64AP_CourseToTTC()) {
        obj_mark_for_deletion(o);
        return;
    } else {
        SM64AP_SetClockToTTCAction(&(o->oAction));
    }
    
    struct Surface *marioSurface;
    u16 rollAngle = o->oFaceAngleRoll;
    s16 speedSetting = ttc_speed_setting_from_roll_angle(rollAngle);

    if (cur_obj_has_behavior(bhvClockMinuteHand)) {
        SM64AP_SetTTCEntranceSpeed(speedSetting);
    }

    o->oFloorHeight =
        find_floor(gMarioObject->oPosX, gMarioObject->oPosY, gMarioObject->oPosZ, &marioSurface);

    // Seems to make sure Mario is on a default surface & 4 frames pass before
    //   allowing him to change the Tick Tock Clock speed setting.
    // Probably a safety check for when you leave the level through the painting
    //   to make sure the setting isn't accidentally locked in as you fly out.
    if (o->oAction == 0) {
        if (marioSurface->type == SURFACE_DEFAULT)
            if (o->oTimer >= 4)
                o->oAction++;
    } else if (o->oAction == 5) {
        // If Mario is touching the Tick Tock Clock painting...
        if (1) {
            // And this is the minute hand...
            if (cur_obj_has_behavior(bhvClockMinuteHand))
                gTTCSpeedSetting = speedSetting;

            // Increment the action to stop animating the hands.
            o->oAction++;
        } else {
        }
    }

    // Only rotate the hands until Mario enters the painting.
    if (o->oAction < 2)
        cur_obj_rotate_face_angle_using_vel();
}
