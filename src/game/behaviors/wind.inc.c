// wind.c.inc

struct VerticalWindDebugTriangle {
    s16 x1;
    s16 z1;
    s16 x2;
    s16 z2;
    s16 x3;
    s16 z3;
};

static const struct VerticalWindDebugTriangle sCcmVerticalWind[] = {
    { -716, 5837, -716, 6246, 922, 6246 },
    { -716, 5837, 922, 6246, 922, 5837 },
};

static const struct VerticalWindDebugTriangle sTtmVerticalWind[] = {
    { -3071, 3107, -5119, -2626, -5119, 3107 },
    { -3890, -4060, -1637, -7132, -5119, -2626 },
    { -5119, -2626, -3071, -2421, -3890, -4060 },
    { -3890, -4060, -1228, -5903, -1637, -7132 },
    { 4301, -6517, -1637, -7132, -1228, -5903 },
    { -1637, -7132, -5324, -4469, -5119, -2626 },
    { -5119, -2626, -3071, 3107, -3071, -2421 },
    { 4301, -6517, -1228, -5903, 3072, -5289 },
    { 4301, -6517, 5325, -3036, 6758, -4469 },
    { 4301, -6517, 4710, -3650, 5325, -3036 },
    { 4301, -6517, 4506, -3650, 4710, -3650 },
    { 4301, -6517, 3072, -5289, 4506, -3650 },
};

static const struct VerticalWindDebugTriangle sThiHugeVerticalWind[] = {
    { 7782, 205, 5632, -2866, 5632, 205 },
    { 7782, 205, 7782, -2866, 5632, -2866 },
};

static const struct VerticalWindDebugTriangle sThiTinyVerticalWind[] = {
    { 2335, 61, 2335, -859, 1690, -859 },
    { 2335, 61, 1690, -859, 1690, 61 },
};

void spawn_vertical_wind_debug_particles(s16 level, s16 area) {
    const struct VerticalWindDebugTriangle *triangles = NULL;
    s32 triangleCount = 0;
    f32 floorY = -8089.0f;
    s32 i;

    if (level == LEVEL_CCM && area == 1) {
        triangles = sCcmVerticalWind;
        triangleCount = ARRAY_COUNT(sCcmVerticalWind);
    } else if (level == LEVEL_TTM && area == 1) {
        triangles = sTtmVerticalWind;
        triangleCount = ARRAY_COUNT(sTtmVerticalWind);
    } else if (level == LEVEL_THI && area == 1) {
        triangles = sThiHugeVerticalWind;
        triangleCount = ARRAY_COUNT(sThiHugeVerticalWind);
        floorY = -7065.0f;
    } else if (level == LEVEL_THI && area == 2) {
        triangles = sThiTinyVerticalWind;
        triangleCount = ARRAY_COUNT(sThiTinyVerticalWind);
        floorY = -3788.0f;
    }

    if (triangles == NULL) {
        return;
    }

    for (i = 0; i < 4; i++) {
        const struct VerticalWindDebugTriangle *triangle = &triangles[random_u16() % triangleCount];
        f32 u = random_float();
        f32 v = random_float();
        f32 minY = MAX(floorY + 100.0f, -4400.0f);
        f32 x;
        f32 y;
        f32 z;
        f32 detectedFloorY;
        struct Surface *detectedFloor;
        struct Object *wind;

        if (u + v > 1.0f) {
            u = 1.0f - u;
            v = 1.0f - v;
        }

        x = triangle->x1 + u * (triangle->x2 - triangle->x1) + v * (triangle->x3 - triangle->x1);
        y = minY + random_float() * (400.0f - minY);
        z = triangle->z1 + u * (triangle->z2 - triangle->z1) + v * (triangle->z3 - triangle->z1);
        detectedFloorY = find_floor(x, y, z, &detectedFloor);

        if (detectedFloor == NULL || detectedFloor->type != SURFACE_VERTICAL_WIND
            || y <= detectedFloorY || y <= -4500.0f || y >= 500.0f) {
            continue;
        }

        wind = spawn_object_abs_with_rot(gMarioObject, 0, MODEL_MIST, bhvWind, x, y, z, 0, 0, 0);
        wind->oMoveAnglePitch = 1;
        wind->oBehParams2ndByte = 1;
    }
}

void spawn_wind_particles(s16 pitch, s16 yaw) {
    s32 i;
    for (i = 0; i < 3; i++) {
        struct Object *wind = spawn_object(o, MODEL_MIST, bhvWind);
        wind->oMoveAngleYaw = yaw;
        wind->oMoveAnglePitch = pitch;
    }
}

void bhv_wind_loop(void) {
    s16 sp2E = 500;
    f32 sp28 = 1.0f;
    if (o->oTimer == 0) {
        o->oOpacity = o->oBehParams2ndByte == 1 ? 180 : 100;
        if (o->oBehParams2ndByte == 1) {
            o->oVelY = 0.0f;
            o->oForwardVel = 0.0f;
            sp28 = 1.5f;
        } else if (o->oMoveAnglePitch == 0) {
            obj_translate_xz_random(o, 900.0f);
            o->oPosX += sins(o->oMoveAngleYaw + 0x8000) * sp2E; // NOP as Pitch is 0
            o->oPosY += 80.0f + random_f32_around_zero(200.0f);
            o->oPosZ += coss(o->oMoveAngleYaw + 0x8000) * sp2E; // -coss(a) * sp2E
            o->oMoveAngleYaw += random_f32_around_zero(4000.0f);
            o->oForwardVel = random_float() * 70.0f + 50.0f;
        } else {
            obj_translate_xz_random(o, 600.0f);
            o->oPosY -= sp2E - 200; // 300
            o->oVelY = random_float() * 30.0f + 50.0f;
            o->oMoveAngleYaw = random_u16();
            o->oForwardVel = 10.0f;
        }
        obj_set_billboard(o);
        cur_obj_scale(sp28);
    }
    if (o->oTimer > (o->oBehParams2ndByte == 1 ? 20 : 8))
        obj_mark_for_deletion(o);
    o->oFaceAnglePitch += 4000.0f + 2000.0f * random_float();
    o->oFaceAngleYaw += 4000.0f + 2000.0f * random_float();
    cur_obj_move_using_fvel_and_gravity();
}
