#include "sm64ap.h"
#include "Archipelago.h"

extern "C" {
    #include "sm64.h"
    #include "game/area.h"
    #include "game/game_init.h"
    #include "game/print.h"
    #include "game/save_file.h"
    #include "behavior_data.h"
    #include "gfx_dimensions.h"
    #include "level_table.h"
    #include "course_table.h"
    #include "model_ids.h"
    #include "seq_ids.h"
    #include "engine/behavior_script.h"
    #include "engine/surface_collision.h"
    #include "game/level_update.h"
    #include "game/ingame_menu.h"
    #include "game/object_list_processor.h"
    #include "game/segment2.h"
    #include "pc/cheats.h"
    #include "pc/configfile.h"
    #include "object_constants.h"
    #include "object_fields.h"
    #include "dialog_ids.h"
    #include "sm64ap_visual.h"

    void SM64AP_SetMarioShirtColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioOverallsColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioGlovesColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioShoesColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioSkinColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioHairColor(u8 r, u8 g, u8 b);
    void SM64AP_ResetMarioSideburnColor(void);
    void SM64AP_SetMarioSideburnColor(u8 r, u8 g, u8 b);
    void SM64AP_SetMarioHatColor(u8 r, u8 g, u8 b);
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
#include <climits>
#include <queue>
#include <cctype>
#include <utility>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <mutex>
#include <limits>
#include <atomic>
#include <cmath>

// APCpp's JSON reader and writer are shared by its network thread. Game-thread messages are
// serialized here before entering APCpp so coin collection cannot race incoming messages.
extern void APSend(std::string request);

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
bool sm64_have_wmotr_cannon = false;
bool sm64_have_yoshi = false;
bool sm64_have_bitfs = false;
bool sm64_have_hat = false;
bool sm64_have_vcutm_entrance = false;
bool sm64_have_rr_level_unlock = false;
bool sm64_have_wmotr_level_unlock = false;
bool sm64_1up_checks_enabled = false;
bool sm64_buddy_checks_enabled = true;
bool sm64_bowser_stage_1up_item_behavior = false;
bool sm64_have_bowser_stage_1ups = false;
bool sm64_have_bitdw_1ups = false;
bool sm64_have_bitfs_1ups = false;
bool sm64_hat_restore_with_animation_pending = false;
bool sm64_hat_restore_without_animation_pending = false;
bool sm64_easy_butterflies = false;
bool sm64_no_despawn = false;
bool sm64_mips_skip_enabled = false;
static std::atomic<int> sm64_live_object_reconcile_frames { 0 };
static constexpr int SM64AP_LIVE_OBJECT_RECONCILE_DELAY = 3;
bool sm64_have_wingcap = false;
bool sm64_have_metalcap = false;
bool sm64_have_vanishcap = false;
int sm64_cap_length_items[3] = { 0, 0, 0 };
int sm64_cap_length_item_counts[3] = { 0, 0, 0 };
int sm64_bowser_arena_bombs[3] = { 0, 0, 0 };
int sm64_bowser_hit_requirements[3] = { 1, 1, 3 };
int sm64_bowser_in_the_sky_stage_collapse_hits = 2;

struct SM64APSignHint {
    std::string text;
    int64_t location;
    int entrance;
    int locationPlayer;
};

static void SM64AP_SendSerializedRequest(const std::string &request);

static const int sm64_shuffled_entrance_ids[] = {
    91, 241, 121, 51, 41, 71, 221, 81, 231, 101,
    111, 112, 113, 361, 132, 131, 141, 142, 143, 144,
    151, 211, 271, 201, 171, 291, 281, 181, 191, 311,
};

static std::map<int, SM64APSignHint> sm64_sign_hints;
static std::mutex sm64_sign_hint_mutex;
static std::vector<u8> sm64_sign_dialog_text = { DIALOG_CHAR_TERMINATOR };
static struct DialogEntry sm64_sign_dialog_entry = { 1, 4, 30, 200, sm64_sign_dialog_text.data() };
static s16 sm64_active_sign_dialog = -1;

struct SM64APPermanentCoinRecord {
    u8 course;
    u8 value;
};

struct SM64APCoinOutputCatalogEntry {
    u64 physicalSource;
    u8 physicalSlot;
    int locationId;
};

struct SM64APCoinCompletionCatalogEntry {
    u64 physicalSource;
    u8 requiredOutputCount;
    int locationId;
};

static const SM64APCoinOutputCatalogEntry sm64_coin_output_catalog[] = {
    { 0, 0, 0 },
#define COIN_OUTPUT(source, slot, location) { source, slot, location },
#define COIN_COMPLETION(source, count, location)
#include "sm64ap_coin_check_catalog.inc"
#undef COIN_COMPLETION
#undef COIN_OUTPUT
};

static const SM64APCoinCompletionCatalogEntry sm64_coin_completion_catalog[] = {
    { 0, 0, 0 },
#define COIN_OUTPUT(source, slot, location)
#define COIN_COMPLETION(source, count, location) { source, count, location },
#include "sm64ap_coin_check_catalog.inc"
#undef COIN_COMPLETION
#undef COIN_OUTPUT
};

static std::map<std::pair<u64, u8>, SM64APPermanentCoinRecord> sm64_permanent_coins;
static std::map<std::pair<u64, u8>, SM64APPermanentCoinRecord> sm64_permanent_coin_updates;
static std::map<std::pair<u64, u8>, SM64APPermanentCoinRecord> sm64_pending_permanent_coins;
static std::set<std::pair<u64, u8>> sm64_uncollected_coin_tombstones;
static std::mutex sm64_permanent_coin_mutex;
static bool sm64_pending_permanent_coin_snapshot = false;
static bool sm64_permanent_coin_storage_initialized = false;
static bool sm64_permanent_coin_reconcile_requested = false;
static std::string sm64_permanent_coin_ledger_key;
static std::set<int> sm64_checked_coin_output_locations;
static std::set<std::pair<u64, u8>> sm64_pending_coin_output_checks;
static std::set<u64> sm64_pending_coin_completion_checks;
static bool sm64_finished_bowser_storage_received = false;
static bool sm64_moat_storage_received = false;
static bool sm64_permanent_coin_storage_received = false;
static bool sm64_save_flags_storage_received = false;
static bool sm64_cannon_flags_storage_received = false;
static int sm64_coin_scores_storage_received = 0;
static u32 sm64_server_save_flags = 0;
static u32 sm64_server_cannon_flags = 0;
static int sm64_server_coin_high_scores[COURSE_STAGES_COUNT] = {};
static int sm64_title_connection_wait_frames = 0;
static void SM64AP_LoadPermanentCoins(const std::string &rawLedger);
static void SM64AP_SendCoinOutputCheck(const std::pair<u64, u8> &physicalKey);
static void SM64AP_SendCoinCompletionCheck(u64 physicalSource);

struct SM64APUncollectTrapState {
    int ordinal;
    int candidate;
    int winner;
    int frames;
    bool eventReceived;
    std::string claimKey;
    std::string eventKey;
};

struct SM64APUncollectTrapEvent {
    int ordinal;
    u64 source;
    u8 slot;
    u8 course;
    u8 value;
};

static int sm64_received_uncollect_coin_traps = 0;
static std::queue<int> sm64_pending_uncollect_coin_traps;
static std::map<int, SM64APUncollectTrapState> sm64_uncollect_trap_states;
static std::queue<SM64APUncollectTrapEvent> sm64_pending_uncollect_trap_events;
static constexpr int SM64AP_UNCOLLECT_TRAP_ELECTION_FRAMES = 90;
bool sm64_show_global_cap_display = false;
int sm64_moat_state = 0;
bool sm64_have_cannon[15];
bool sm64_have_painting[NUM_PAINTING_LOCKS];
bool sm64_have_thi_tiny_painting = false;
int sm64_completion_type = 0;
std::bitset<SM64AP_NUM_ABILITIES> sm64_have_abilities;
std::bitset<SM64AP_NUM_LEVEL_MOVE_AREAS * SM64AP_NUM_LEVEL_MOVES> sm64_have_level_moves;
std::bitset<SM64AP_NUM_FEATURES> sm64_have_features;
std::bitset<SM64AP_NUM_LEVEL_CAPS> sm64_have_level_caps;
std::bitset<SM64AP_NUM_OBJECT_ITEMS> sm64_have_object_items;
std::bitset<SM64AP_NUM_ONE_UP_CATEGORIES> sm64_have_one_up_global_items;
std::bitset<SM64AP_NUM_ONE_UP_LEVEL_ITEMS> sm64_have_one_up_level_items;
static bool sm64_have_global_signs = false;
static std::bitset<SM64AP_NUM_SIGN_LEVEL_ITEMS> sm64_have_level_signs;

struct SM64APSignLevelItem {
    s16 level;
    const char *name;
};

static constexpr SM64APSignLevelItem SM64AP_SIGN_LEVEL_ITEMS[SM64AP_NUM_SIGN_LEVEL_ITEMS] = {
    { LEVEL_CASTLE, "CASTLE SIGNS" },
    { LEVEL_BOB, "BOB SIGNS" },
    { LEVEL_WF, "WF SIGNS" },
    { LEVEL_JRB, "JRB SIGNS" },
    { LEVEL_CCM, "CCM SIGNS" },
    { LEVEL_BBH, "BBH SIGNS" },
    { LEVEL_HMC, "HMC SIGNS" },
    { LEVEL_LLL, "LLL SIGNS" },
    { LEVEL_SSL, "SSL SIGNS" },
    { LEVEL_DDD, "DDD SIGNS" },
    { LEVEL_SL, "SL SIGNS" },
    { LEVEL_WDW, "WDW SIGNS" },
    { LEVEL_TTM, "TTM SIGNS" },
    { LEVEL_THI, "THI SIGNS" },
    { LEVEL_PSS, "PSS SIGNS" },
    { LEVEL_COTMC, "COTMC SIGNS" },
    { LEVEL_BITDW, "BITDW SIGNS" },
};

struct SM64APCoinGlobalItem {
    int source;
    const char *name;
};

struct SM64APCoinLevelItem {
    int source;
    s16 level;
    const char *name;
};

#define COIN_GLOBAL(source, name) { source, name },
static constexpr SM64APCoinGlobalItem sm64_coin_global_items[] = {
#include "sm64ap_coin_items.inc"
};
#undef COIN_GLOBAL

#define COIN_LEVEL(source, level, name) { source, level, name },
static constexpr SM64APCoinLevelItem sm64_coin_level_items[] = {
#include "sm64ap_coin_items.inc"
};
#undef COIN_LEVEL

static constexpr int SM64AP_NUM_COIN_GLOBAL_ITEMS =
    sizeof(sm64_coin_global_items) / sizeof(sm64_coin_global_items[0]);
static constexpr int SM64AP_NUM_COIN_LEVEL_ITEMS =
    sizeof(sm64_coin_level_items) / sizeof(sm64_coin_level_items[0]);

std::bitset<SM64AP_NUM_COIN_GLOBAL_ITEMS> sm64_have_coin_global_items;
std::bitset<SM64AP_NUM_COIN_LEVEL_ITEMS> sm64_have_coin_level_items;

enum SM64APEnemyUnlockSource {
    SM64AP_ENEMY_UNLOCK_BOWSER,
    SM64AP_ENEMY_UNLOCK_CHAIN_CHOMP,
    SM64AP_ENEMY_UNLOCK_WIGGLER,
    SM64AP_ENEMY_UNLOCK_TWEESTER,
    SM64AP_ENEMY_UNLOCK_KLEPTO,
    SM64AP_ENEMY_UNLOCK_AMP,
    SM64AP_ENEMY_UNLOCK_MAD_PIANO,
    SM64AP_ENEMY_UNLOCK_HAUNTED_CHAIR,
    SM64AP_ENEMY_UNLOCK_SUSHI_SHARK,
    SM64AP_ENEMY_UNLOCK_BUBBA,
    SM64AP_ENEMY_UNLOCK_TOX_BOX,
    SM64AP_ENEMY_UNLOCK_BOWLING_BALL,
    SM64AP_ENEMY_UNLOCK_WATER_BOMB,
    SM64AP_ENEMY_UNLOCK_BOULDER,
    SM64AP_ENEMY_UNLOCK_FIRE_HAZARD,
    SM64AP_ENEMY_UNLOCK_BOUNCING_FIREBALL,
    SM64AP_ENEMY_UNLOCK_SPINDEL,
    SM64AP_ENEMY_UNLOCK_FALLING_PILLAR,
    SM64AP_ENEMY_UNLOCK_FIRE_SPITTER,
    SM64AP_ENEMY_UNLOCK_BUB,
};

struct SM64APEnemyUnlockItem {
    int64_t id;
    int source;
    s16 level;
    const char *name;
};

#define ENEMY_GLOBAL(id, source, name) { id, source, -1, name },
#define ENEMY_LEVEL(id, source, level, name) { id, source, level, name },
static constexpr SM64APEnemyUnlockItem sm64_enemy_unlock_items[] = {
#include "sm64ap_enemy_items.inc"
};
#undef ENEMY_LEVEL
#undef ENEMY_GLOBAL

static constexpr int SM64AP_NUM_ENEMY_UNLOCK_ITEMS =
    sizeof(sm64_enemy_unlock_items) / sizeof(sm64_enemy_unlock_items[0]);
std::bitset<SM64AP_NUM_ENEMY_UNLOCK_ITEMS> sm64_have_enemy_unlock_items;

std::bitset<SM64AP_NUM_COIN_CHECKS> sm64_sent_coin_checks;
std::bitset<SM64AP_NUM_1UP_CHECKS> sm64_sent_1up_checks;
std::bitset<SM64AP_NUM_BLOCKSANITY_CHECKS> sm64_sent_blocksanity_checks;
std::set<int> sm64_sent_box_checks;
int* sm64_clockaction = nullptr;
int sm64_cost_firstbowserdoor = 8;
int sm64_cost_basementdoor = 30;
int sm64_cost_secondfloordoor = 50;
int sm64_cost_endlessstairs = 70;
int sm64_cost_mips1 = 15;
int sm64_cost_mips2 = 50;
int msg_frame_duration = 90;
int cur_msg_frame_duration = msg_frame_duration;
std::queue<int64_t> delayed_queue;

std::map<int,int> map_entrances;
std::map<int,int> map_sub_area_entrances;
static bool sm64_ccm_slide_exit_arrival_pending = false;

struct SM64APReturnPoint {
    s16 level;
    s8 area;
    s16 node;
    int sourceId;
    s16 entranceLevel;
    s16 pos[3];
    s16 yaw;
    u32 starSpawnType;
    bool normalCastleEntrance;
    bool overridePosition;
    bool reverseStarFacing;
};

struct SM64APPendingReturnSpawn {
    s16 pos[3];
    s16 yaw;
    u32 spawnType;
    bool active;
    bool overridePosition;
    bool moveAwayFromWarp;
    bool reverseFacing;
};

static std::vector<SM64APReturnPoint> sm64_return_stack;
static SM64APPendingReturnSpawn sm64_pending_return_spawn = {};

std::map<int,int> map_boxid_locid;

int sm64_wdw_entrance_variant = 0;
int sm64_ttc_entrance_variant = SM64AP_ENTRANCE_TTC_STOPPED;
int sm64_music_shuffle_mode = 0;
std::map<int,int> map_music;
int sm64_skybox_shuffle_mode = 0;
std::map<int,int> map_skybox;
static int sm64_loaded_skybox = -1;
std::map<int,int> map_start_inventory;
int sm64_coin_star_requirements[15] = {
    100, 100, 100, 100, 100,
    100, 100, 100, 100, 100,
    100, 100, 100, 100, 100,
};

static constexpr int SM64AP_MUSIC_SHUFFLE_OFF = 0;
static constexpr int SM64AP_MUSIC_SHUFFLE_MAP = 1;
static constexpr int SM64AP_MUSIC_SHUFFLE_RANDOM_ON_LOAD = 2;
static constexpr int SM64AP_SKYBOX_SHUFFLE_OFF = 0;
static constexpr int SM64AP_SKYBOX_SHUFFLE_MAP = 1;
static constexpr int SM64AP_SKYBOX_SHUFFLE_RANDOM_ON_LOAD = 2;
static constexpr int SM64AP_NUM_COIN_STAR_REQUIREMENTS = 15;
static constexpr int SM64AP_NUM_COIN_CHECK_COURSES = 24;
static constexpr int SM64AP_DEFAULT_COIN_STAR_REQUIREMENT = 100;
static constexpr int SM64AP_COIN_CHECK_MAX_COUNTS[SM64AP_NUM_COIN_CHECK_COURSES] = {
    146, 141, 104, 154, 151,
    139, 133, 136, 106, 127,
    152, 137, 191, 128, 146,
    80, 56, 56, 63, 27,
    47, 80, 80, 76,
};

static bool SM64AP_CanReportProgress() {
    return gCurrDemoInput == nullptr && gCurrCreditsEntry == nullptr;
}

static int sm64ap_last_location_check_id = 0;

static constexpr int SM64AP_COIN_CHECK_OFFSETS[SM64AP_NUM_COIN_CHECK_COURSES] = {
    0, 146, 287, 391, 545,
    696, 835, 968, 1104, 1210,
    1337, 1489, 1626, 1817, 1945,
    SM64AP_NUM_MAIN_COIN_CHECKS,
    SM64AP_NUM_MAIN_COIN_CHECKS + 80,
    SM64AP_NUM_MAIN_COIN_CHECKS + 136,
    SM64AP_NUM_MAIN_COIN_CHECKS + 192,
    SM64AP_NUM_MAIN_COIN_CHECKS + 255,
    SM64AP_NUM_MAIN_COIN_CHECKS + 282,
    SM64AP_NUM_MAIN_COIN_CHECKS + 329,
    SM64AP_NUM_MAIN_COIN_CHECKS + 409,
    SM64AP_NUM_MAIN_COIN_CHECKS + 489,
};

struct SM64APBlocksanitySource {
    s16 level;
    s16 area;
    s32 behParams;
    s16 x;
    s16 y;
    s16 z;
};

static constexpr SM64APBlocksanitySource SM64AP_BLOCKSANITY_SOURCES[SM64AP_NUM_BLOCKSANITY_CHECKS] = {
    { LEVEL_BBH, 1, 0x00020000, -1960, 300, -120 },
    { LEVEL_BBH, 1, 0x00020000, 1268, 1050, 1860 },
    { LEVEL_BBH, 1, (0x14040000 + LEVEL_BBH * 10 + 1), 660, 3200, 1160 },
    { LEVEL_BBH, 1, 0x00060000, 700, 80, -2800 },
    { LEVEL_BBH, 1, 0x00020000, 460, 2140, -560 },
    { LEVEL_BITDW, 1, 0x00010000, -6420, -2900, 3880 },
    { LEVEL_BITDW, 1, 0x00050000, -5120, 1460, -2140 },
    { LEVEL_BITDW, 1, (0x14040000 + LEVEL_BITDW * 10 + 1), -4860, 1380, -300 },
    { LEVEL_BITDW, 1, (0x14040000 + LEVEL_BITDW * 10 + 2), -2420, -1140, 3700 },
    { LEVEL_BITFS, 1, (0x14040000 + LEVEL_BITFS * 10 + 1), -7400, 1500, 0 },
    { LEVEL_BITFS, 1, 0x00060000, -5340, 4000, 100 },
    { LEVEL_BITFS, 1, (0x14040000 + LEVEL_BITFS * 10 + 2), 2440, 5520, 140 },
    { LEVEL_BITFS, 1, 0x00050000, 7220, -1800, 260 },
    { LEVEL_BITS, 1, (0x14040000 + LEVEL_BITS * 10 + 1), 4100, -1050, -1800 },
    { LEVEL_BOB, 1, 0x00000000, -6710, 1300, -2170 },
    { LEVEL_BOB, 1, 0x00000000, 400, 350, 6500 },
    { LEVEL_BOB, 1, 0x00000000, 3789, 3340, 1818 },
    { LEVEL_BOB, 1, 0x000B0000, 5540, 3350, 1200 },
    { LEVEL_CASTLE_GROUNDS, 1, 0x00000000, 13, 3476, -5646 },
    { LEVEL_CCM, 1, (0x14040000 + LEVEL_CCM * 10 + 1), -4887, -1300, -4003 },
    { LEVEL_CCM, 1, (0x14040000 + LEVEL_CCM * 10 + 2), -1557, -205, 1794 },
    { LEVEL_CCM, 2, (0x14040000 + LEVEL_CCM * 10 + 3), -5600, -4500, -6644 },
    { LEVEL_COTMC, 1, 0x00010000, -360, 300, -200 },
    { LEVEL_COTMC, 1, (0x14040000 + LEVEL_COTMC * 10 + 1), -20, 180, 2060 },
    { LEVEL_COTMC, 1, 0x00010000, 300, 620, -5280 },
    { LEVEL_DDD, 2, 0x00010000, 6800, 500, -850 },
    { LEVEL_DDD, 2, 0x00020000, 6800, 500, -150 },
    { LEVEL_HMC, 1, 0x00010000, -6924, 2440, 7364 },
    { LEVEL_HMC, 1, (0x14040000 + LEVEL_HMC * 10 + 1), -4960, 2700, 80 },
    { LEVEL_HMC, 1, 0x00010000, -3000, -2250, -6400 },
    { LEVEL_HMC, 1, (0x14040000 + LEVEL_HMC * 10 + 2), -2700, 2100, -6400 },
    { LEVEL_HMC, 1, 0x00010000, 1939, -600, -2920 },
    { LEVEL_HMC, 1, 0x00010000, 5100, -600, -4500 },
    { LEVEL_HMC, 1, 0x00010000, 5860, -550, -739 },
    { LEVEL_JRB, 1, 0x00010000, -7160, 1340, 2580 },
    { LEVEL_JRB, 1, 0x00050000, -5800, 1340, -750 },
    { LEVEL_JRB, 1, 0x00010000, 279, -2600, -7340 },
    { LEVEL_JRB, 1, 0x04080000, 1540, 2160, 2130 },
    { LEVEL_JRB, 1, 0x00010000, 2077, 1832, 7465 },
    { LEVEL_JRB, 2, 0x00080000, 0, 1600, 3000 },
    { LEVEL_LLL, 1, 0x00000000, -5900, 460, 6400 },
    { LEVEL_LLL, 1, 0x00030000, 1050, 550, 6200 },
    { LEVEL_PSS, 1, 0x00080000, -6385, -4200, 5770 },
    { LEVEL_RR, 1, (0x14040000 + LEVEL_RR * 10 + 1), -6750, 2600, -50 },
    { LEVEL_RR, 1, (0x14040000 + LEVEL_RR * 10 + 2), -4844, -4240, 6622 },
    { LEVEL_RR, 1, (0x14040000 + LEVEL_RR * 10 + 3), -3428, 6770, -5128 },
    { LEVEL_RR, 1, 0x000E0000, 5000, 4100, 4440 },
    { LEVEL_SL, 1, 0x00030000, -5450, 1300, 5900 },
    { LEVEL_SL, 1, 0x000C0000, -4700, 1300, 5850 },
    { LEVEL_SL, 1, (0x14040000 + LEVEL_SL * 10 + 1), -3380, 1360, -4140 },
    { LEVEL_SL, 2, 0x00050000, -720, 300, -1740 },
    { LEVEL_SL, 2, (0x14040000 + LEVEL_SL * 10 + 2), -120, 300, -1740 },
    { LEVEL_SL, 2, 0x00020000, 1660, 300, -1720 },
    { LEVEL_SSL, 1, 0x00000000, -3000, 500, 800 },
    { LEVEL_SSL, 1, (0x14040000 + LEVEL_SSL * 10 + 1), -1200, 500, 800 },
    { LEVEL_SSL, 1, 0x00030000, 5840, 940, 2500 },
    { LEVEL_SSL, 1, 0x00000000, 5860, 940, 4180 },
    { LEVEL_SSL, 1, 0x00000000, 6900, 350, -5400 },
    { LEVEL_SSL, 2, (0x14040000 + LEVEL_SSL * 10 + 2), -3536, 252, -3705 },
    { LEVEL_SSL, 2, (0x14040000 + LEVEL_SSL * 10 + 3), -1242, 252, -3957 },
    { LEVEL_THI, 1, (0x14040000 + LEVEL_THI * 10 + 2), -5712, -2190, 1100 },
    { LEVEL_THI, 1, 0x000A0000, 2600, 3500, -2400 },
    { LEVEL_THI, 1, (0x14040000 + LEVEL_THI * 10 + 3), 6022, -1722, -633 },
    { LEVEL_THI, 2, (0x14040000 + LEVEL_THI * 10 + 1), -1866, -400, 311 },
    { LEVEL_THI, 2, 0x00050000, 1849, -325, -183 },
    { LEVEL_TOTWC, 1, 0x00000000, 0, -1760, -600 },
    { LEVEL_TTC, 1, 0x00050000, -1160, 2920, -840 },
    { LEVEL_TTC, 1, 0x00050000, -1140, -3720, -1620 },
    { LEVEL_TTC, 1, (0x14040000 + LEVEL_TTC * 10 + 2), -1101, 6316, -685 },
    { LEVEL_TTC, 1, 0x00060000, -780, 6316, -1020 },
    { LEVEL_TTC, 1, 0x00060000, -400, 3600, 1880 },
    { LEVEL_TTC, 1, 0x00050000, -40, 4160, -1280 },
    { LEVEL_TTC, 1, 0x00060000, 0, 4783, 0 },
    { LEVEL_TTC, 1, 0x00060000, 280, -4920, 1660 },
    { LEVEL_TTC, 1, 0x00050000, 520, 300, 1500 },
    { LEVEL_TTC, 1, 0x00050000, 840, -2200, 860 },
    { LEVEL_TTC, 1, 0x00050000, 1240, 300, 840 },
    { LEVEL_TTC, 1, (0x14040000 + LEVEL_TTC * 10 + 1), 1883, 4150, 550 },
    { LEVEL_TTC, 1, 0x00060000, 2350, 5600, 2350 },
    { LEVEL_TTM, 1, (0x14040000 + LEVEL_TTM * 10 + 1), 3261, -2553, -4092 },
    { LEVEL_VCUTM, 1, 0x00020000, -6020, -2976, 1240 },
    { LEVEL_VCUTM, 1, (0x14040000 + LEVEL_VCUTM * 10 + 1), -3434, 2951, -3076 },
    { LEVEL_VCUTM, 1, 0x00050000, -2145, -2160, -5963 },
    { LEVEL_VCUTM, 1, 0x00020000, 3980, 300, -6220 },
    { LEVEL_WDW, 1, 0x00060000, -3760, 700, 4120 },
    { LEVEL_WDW, 1, 0x00080000, -2200, 2600, 3500 },
    { LEVEL_WDW, 1, 0x00050000, -2200, 3060, -3700 },
    { LEVEL_WDW, 1, 0x00060000, -2075, 3050, -524 },
    { LEVEL_WDW, 1, 0x00060000, 943, 3880, -1779 },
    { LEVEL_WDW, 1, 0x000A0000, 1550, 4350, 100 },
    { LEVEL_WDW, 1, 0x00050000, 3388, 1600, 1155 },
    { LEVEL_WDW, 2, 0x00020000, -1779, -2240, 3644 },
    { LEVEL_WDW, 2, 0x00010000, -770, 80, 770 },
    { LEVEL_WDW, 2, 0x00020000, 1300, -2260, 3740 },
    { LEVEL_WDW, 2, (0x14040000 + LEVEL_WDW * 10 + 2), 1655, -2160, -1293 },
    { LEVEL_WF, 1, 0x00010000, 2750, 1370, -3400 },
    { LEVEL_WMOTR, 1, 0x00000000, -3200, 4880, -4040 },
    { LEVEL_WMOTR, 1, 0x00000000, -2760, 2320, -4080 },
    { LEVEL_WMOTR, 1, (0x14040000 + LEVEL_WMOTR * 10 + 1), -2744, 4899, -4439 },
    { LEVEL_WMOTR, 1, 0x00000000, -400, 1960, -120 },
    { LEVEL_WMOTR, 1, 0x00000000, -240, -1080, 4520 },
    { LEVEL_WMOTR, 1, 0x00000000, 3600, -2480, 5440 },
    { LEVEL_WMOTR, 1, 0x00000000, 3960, 520, 440 },
};

static_assert(sizeof(SM64AP_BLOCKSANITY_SOURCES) / sizeof(SM64AP_BLOCKSANITY_SOURCES[0])
              == SM64AP_NUM_BLOCKSANITY_CHECKS,
              "Blocksanity source count must match location count");

struct SM64APOneUpSource {
    s16 level;
    s16 area;
    s16 sourceType;
    s16 sourceParam;
    s16 x;
    s16 y;
    s16 z;
};

static constexpr SM64APOneUpSource SM64AP_1UP_SOURCES[SM64AP_NUM_1UP_CHECKS] = {
    { LEVEL_BBH, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -3040, 1120, 5460 },
    { LEVEL_BITDW, 1, SM64AP_1UP_SOURCE_OBJECT, 1, 33, 1900, 333 },
    { LEVEL_BITDW, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 610, 1045, -167 },
    { LEVEL_BITDW, 1, SM64AP_1UP_SOURCE_OBJECT, 1, -485, 1054, -167 },
    { LEVEL_BITDW, 1, SM64AP_1UP_SOURCE_OBJECT, 2, 1100, 2080, 363 },
    { LEVEL_BITFS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -4320, -2640, -500 },
    { LEVEL_BITFS, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, 3880, -1140, 260 },
    { LEVEL_BITFS, 1, SM64AP_1UP_SOURCE_HIDDEN, 3, -6460, 2760, 320 },
    { LEVEL_BITFS, 1, SM64AP_1UP_SOURCE_OBJECT, 2, 1198, 5478, 103 },
    { LEVEL_BITFS, 1, SM64AP_1UP_SOURCE_OBJECT, 2, -174, -2840, -138 },
    { LEVEL_BITS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 1380, -1740, -660 },
    { LEVEL_BITS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -1399, 2750, -1159 },
    { LEVEL_BITS, 1, SM64AP_1UP_SOURCE_HIDDEN, 3, -6640, 2280, -890 },
    { LEVEL_BITS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -259, 6059, -3759 },
    { LEVEL_BITS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -1751, -1246, -805 },
    { LEVEL_BOB, 1, SM64AP_1UP_SOURCE_HIDDEN, 4, -6060, 1060, -5340 },
    { LEVEL_BOB, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -2531, 0, -4201 },
    { LEVEL_BOB, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, 5444, 1400, 6016 },
    { LEVEL_CASTLE_GROUNDS, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, -6270, 975, -2145 },
    { LEVEL_CASTLE_GROUNDS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -440, 3180, -5000 },
    { LEVEL_CASTLE_GROUNDS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 0, 3180, -5200 },
    { LEVEL_CASTLE_GROUNDS, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 440, 3180, -5000 },
    { LEVEL_CASTLE_GROUNDS, 1, SM64AP_1UP_SOURCE_HIDDEN, 2, 0, 510, -1170 },
    { LEVEL_CASTLE_GROUNDS, 1, SM64AP_1UP_SOURCE_BUTTERFLY, 4, -6240, 295, 320 },
    { LEVEL_CASTLE_GROUNDS, 1, SM64AP_1UP_SOURCE_BUTTERFLY, 4, 6330, 710, -3760 },
    { LEVEL_CASTLE, 1, SM64AP_1UP_SOURCE_HIDDEN, 1, 2036, 800, -1673 },
    { LEVEL_CASTLE, 3, SM64AP_1UP_SOURCE_HIDDEN, 4, 2861, -2508, -515 },
    { LEVEL_CCM, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, -5200, -1345, 2995 },
    { LEVEL_CCM, 2, SM64AP_1UP_SOURCE_SLIDING, 0, -4890, 1452, 552 },
    { LEVEL_CCM, 2, SM64AP_1UP_SOURCE_SLIDING, 0, -6369, -1538, 3726 },
    { LEVEL_COTMC, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 900, 260, -3620 },
    { LEVEL_DDD, 1, SM64AP_1UP_SOURCE_HIDDEN, 1, -4760, -5080, 580 },
    { LEVEL_JRB, 1, SM64AP_1UP_SOURCE_HIDDEN, 1, 5140, -4380, 0 },
    { LEVEL_JRB, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 670, 3000, 3315 },
    { LEVEL_LLL, 1, SM64AP_1UP_SOURCE_HIDDEN, 4, -5100, 540, -4070 },
    { LEVEL_LLL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 0, 307, -2085 },
    { LEVEL_LLL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 6326, 686, -6580 },
    { LEVEL_LLL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 0, 46, -7400 },
    { LEVEL_LLL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -3133, 230, -2126 },
    { LEVEL_LLL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -2333, 653, 886 },
    { LEVEL_LLL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -6780, 275, -6766 },
    { LEVEL_LLL, 2, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, 1078, 4170, -2270 },
    { LEVEL_PSS, 1, SM64AP_1UP_SOURCE_HIDDEN, 4, -6380, -4500, 5980 },
    { LEVEL_PSS, 1, SM64AP_1UP_SOURCE_SLIDING, 0, 1847, -961, 3863 },
    { LEVEL_RR, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 6666, -1000, 6533 },
    { LEVEL_RR, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 5040, 2100, 280 },
    { LEVEL_RR, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, 3542, 4892, -2371 },
    { LEVEL_RR, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -165, 3543, -2352 },
    { LEVEL_RR, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -735, 2860, -150 },
    { LEVEL_RR, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, -2175, 2365, -60 },
    { LEVEL_SA, 1, SM64AP_1UP_SOURCE_HIDDEN, 1, 0, -3800, 0 },
    { LEVEL_SL, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, 0, 5420, 0 },
    { LEVEL_SL, 2, SM64AP_1UP_SOURCE_OBJECT, 0, 1700, 20, -100 },
    { LEVEL_SSL, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, -6000, 600, -4800 },
    { LEVEL_SSL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -250, 0, 4200 },
    { LEVEL_SSL, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 5757, 230, 5761 },
    { LEVEL_SSL, 2, SM64AP_1UP_SOURCE_HIDDEN, 4, 1940, -81, -1360 },
    { LEVEL_SSL, 2, SM64AP_1UP_SOURCE_OBJECT, 0, -3350, 980, -1240 },
    { LEVEL_SSL, 2, SM64AP_1UP_SOURCE_OBJECT, 0, 2870, 1050, -2640 },
    { LEVEL_THI, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, 4800, -110, 2250 },
    { LEVEL_THI, 1, SM64AP_1UP_SOURCE_HIDDEN, 2, -777, -1544, 1233 },
    { LEVEL_THI, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -6000, -3566, -1320 },
    { LEVEL_THI, 1, SM64AP_1UP_SOURCE_BUTTERFLY, 0, -3111, -511, 2400 },
    { LEVEL_THI, 1, SM64AP_1UP_SOURCE_BUTTERFLY, 0, 4844, -533, 2266 },
    { LEVEL_THI, 2, SM64AP_1UP_SOURCE_BUTTERFLY, 0, -1693, -890, 1746 },
    { LEVEL_THI, 3, SM64AP_1UP_SOURCE_OBJECT, 0, -1920, 1540, -1040 },
    { LEVEL_TTC, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, -1080, 90, 1575 },
    { LEVEL_TTC, 1, SM64AP_1UP_SOURCE_HIDDEN, 3, 657, 1368, 1879 },
    { LEVEL_TTM, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -3713, -4130, 3530 },
    { LEVEL_TTM, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -1010, -705, -3385 },
    { LEVEL_TTM, 1, SM64AP_1UP_SOURCE_OBJECT, 0, 1530, 620, 1670 },
    { LEVEL_TTM, 1, SM64AP_1UP_SOURCE_BUTTERFLY, 0, -606, 1186, -1290 },
    { LEVEL_TTM, 2, SM64AP_1UP_SOURCE_HIDDEN, 4, 6936, 4800, 6654 },
    { LEVEL_TTM, 2, SM64AP_1UP_SOURCE_OBJECT, 0, 6754, 4800, 5963 },
    { LEVEL_TTM, 2, SM64AP_1UP_SOURCE_SLIDING, 0, 1764, 2943, 1480 },
    { LEVEL_TTM, 3, SM64AP_1UP_SOURCE_SLIDING, 0, -7128, 1807, 2285 },
    { LEVEL_VCUTM, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -4711, 1594, -2532 },
    { LEVEL_VCUTM, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -5952, -393, -1141 },
    { LEVEL_VCUTM, 1, SM64AP_1UP_SOURCE_HIDDEN, 3, 4460, 0, -4700 },
    { LEVEL_WDW, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -1129, 3857, 1404 },
    { LEVEL_WDW, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -882, 4018, 1164 },
    { LEVEL_WDW, 2, SM64AP_1UP_SOURCE_HIDDEN, 4, -772, -2180, 772 },
    { LEVEL_WF, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, -2500, 4560, -256 },
    { LEVEL_WF, 1, SM64AP_1UP_SOURCE_HIDDEN, 2, -250, 2650, 2400 },
    { LEVEL_WF, 1, SM64AP_1UP_SOURCE_BUTTERFLY, 4, 4574, 300, 1130 },
    { LEVEL_WMOTR, 1, SM64AP_1UP_SOURCE_HIDDEN_POLE, 0, 3995, -1850, 5478 },
    { LEVEL_WMOTR, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -3630, -430, 3180 },
    { LEVEL_WMOTR, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -3260, 3370, -3945 },
    { LEVEL_WF, 1, SM64AP_1UP_SOURCE_OBJECT, 0, -384, 3584, 6 },
    { LEVEL_HMC, 1, SM64AP_1UP_SOURCE_MONTY_MOLES, 0, 0, 0, 0 },
    { LEVEL_TTM, 1, SM64AP_1UP_SOURCE_MONTY_MOLES, 0, 0, 0, 0 },
    { LEVEL_HMC, 1, SM64AP_1UP_SOURCE_MONTY_MOLES, 1, 0, 0, 0 },
    { LEVEL_TTM, 1, SM64AP_1UP_SOURCE_MONTY_MOLES, 1, 0, 0, 0 },
};

struct SM64APOneUpLevelItem {
    s16 level;
    s16 category;
    const char *name;
};

static constexpr SM64APOneUpLevelItem SM64AP_ONE_UP_LEVEL_ITEMS[SM64AP_NUM_ONE_UP_LEVEL_ITEMS] = {
    { LEVEL_BBH, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "BBH FREE 1UPS" },
    { LEVEL_BITDW, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "BITDW FREE 1UPS" },
    { LEVEL_BITFS, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "BITFS FREE 1UPS" },
    { LEVEL_BITS, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "BITS FREE 1UPS" },
    { LEVEL_BOB, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "BOB FREE 1UPS" },
    { LEVEL_CASTLE, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "CASTLE FREE 1UPS" },
    { LEVEL_CCM, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "CCM FREE 1UPS" },
    { LEVEL_COTMC, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "COTMC FREE 1UPS" },
    { LEVEL_JRB, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "JRB FREE 1UPS" },
    { LEVEL_LLL, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "LLL FREE 1UPS" },
    { LEVEL_PSS, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "PSS FREE 1UPS" },
    { LEVEL_RR, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "RR FREE 1UPS" },
    { LEVEL_SL, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "SL FREE 1UPS" },
    { LEVEL_SSL, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "SSL FREE 1UPS" },
    { LEVEL_THI, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "THI FREE 1UPS" },
    { LEVEL_TTM, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "TTM FREE 1UPS" },
    { LEVEL_VCUTM, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "VCUTM FREE 1UPS" },
    { LEVEL_WDW, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "WDW FREE 1UPS" },
    { LEVEL_WF, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "WF FREE 1UPS" },
    { LEVEL_WMOTR, SM64AP_ONE_UP_CATEGORY_FREESTANDING, "WMOTR FREE 1UPS" },
    { LEVEL_BITFS, SM64AP_ONE_UP_CATEGORY_TRIGGER, "BITFS TRIGGER 1UPS" },
    { LEVEL_BITS, SM64AP_ONE_UP_CATEGORY_TRIGGER, "BITS TRIGGER 1UPS" },
    { LEVEL_BOB, SM64AP_ONE_UP_CATEGORY_TRIGGER, "BOB TRIGGER 1UPS" },
    { LEVEL_CASTLE, SM64AP_ONE_UP_CATEGORY_TRIGGER, "CASTLE TRIGGER 1UPS" },
    { LEVEL_CCM, SM64AP_ONE_UP_CATEGORY_TRIGGER, "CCM TRIGGER 1UPS" },
    { LEVEL_DDD, SM64AP_ONE_UP_CATEGORY_TRIGGER, "DDD TRIGGER 1UPS" },
    { LEVEL_HMC, SM64AP_ONE_UP_CATEGORY_TRIGGER, "HMC TRIGGER 1UPS" },
    { LEVEL_JRB, SM64AP_ONE_UP_CATEGORY_TRIGGER, "JRB TRIGGER 1UPS" },
    { LEVEL_LLL, SM64AP_ONE_UP_CATEGORY_TRIGGER, "LLL TRIGGER 1UPS" },
    { LEVEL_PSS, SM64AP_ONE_UP_CATEGORY_TRIGGER, "PSS TRIGGER 1UPS" },
    { LEVEL_RR, SM64AP_ONE_UP_CATEGORY_TRIGGER, "RR TRIGGER 1UPS" },
    { LEVEL_SA, SM64AP_ONE_UP_CATEGORY_TRIGGER, "SA TRIGGER 1UPS" },
    { LEVEL_SL, SM64AP_ONE_UP_CATEGORY_TRIGGER, "SL TRIGGER 1UPS" },
    { LEVEL_SSL, SM64AP_ONE_UP_CATEGORY_TRIGGER, "SSL TRIGGER 1UPS" },
    { LEVEL_THI, SM64AP_ONE_UP_CATEGORY_TRIGGER, "THI TRIGGER 1UPS" },
    { LEVEL_TTC, SM64AP_ONE_UP_CATEGORY_TRIGGER, "TTC TRIGGER 1UPS" },
    { LEVEL_TTM, SM64AP_ONE_UP_CATEGORY_TRIGGER, "TTM TRIGGER 1UPS" },
    { LEVEL_VCUTM, SM64AP_ONE_UP_CATEGORY_TRIGGER, "VCUTM TRIGGER 1UPS" },
    { LEVEL_WDW, SM64AP_ONE_UP_CATEGORY_TRIGGER, "WDW TRIGGER 1UPS" },
    { LEVEL_WF, SM64AP_ONE_UP_CATEGORY_TRIGGER, "WF TRIGGER 1UPS" },
    { LEVEL_WMOTR, SM64AP_ONE_UP_CATEGORY_TRIGGER, "WMOTR TRIGGER 1UPS" },
    { LEVEL_BBH, SM64AP_ONE_UP_CATEGORY_BLOCK, "BBH 1UP BLOCKS" },
    { LEVEL_BITDW, SM64AP_ONE_UP_CATEGORY_BLOCK, "BITDW 1UP BLOCKS" },
    { LEVEL_BITFS, SM64AP_ONE_UP_CATEGORY_BLOCK, "BITFS 1UP BLOCKS" },
    { LEVEL_BITS, SM64AP_ONE_UP_CATEGORY_BLOCK, "BITS 1UP BLOCKS" },
    { LEVEL_CCM, SM64AP_ONE_UP_CATEGORY_BLOCK, "CCM 1UP BLOCKS" },
    { LEVEL_COTMC, SM64AP_ONE_UP_CATEGORY_BLOCK, "COTMC 1UP BLOCKS" },
    { LEVEL_HMC, SM64AP_ONE_UP_CATEGORY_BLOCK, "HMC 1UP BLOCKS" },
    { LEVEL_RR, SM64AP_ONE_UP_CATEGORY_BLOCK, "RR 1UP BLOCKS" },
    { LEVEL_SL, SM64AP_ONE_UP_CATEGORY_BLOCK, "SL 1UP BLOCKS" },
    { LEVEL_SSL, SM64AP_ONE_UP_CATEGORY_BLOCK, "SSL 1UP BLOCKS" },
    { LEVEL_THI, SM64AP_ONE_UP_CATEGORY_BLOCK, "THI 1UP BLOCKS" },
    { LEVEL_TTC, SM64AP_ONE_UP_CATEGORY_BLOCK, "TTC 1UP BLOCKS" },
    { LEVEL_TTM, SM64AP_ONE_UP_CATEGORY_BLOCK, "TTM 1UP BLOCKS" },
    { LEVEL_VCUTM, SM64AP_ONE_UP_CATEGORY_BLOCK, "VCUTM 1UP BLOCKS" },
    { LEVEL_WDW, SM64AP_ONE_UP_CATEGORY_BLOCK, "WDW 1UP BLOCKS" },
    { LEVEL_WMOTR, SM64AP_ONE_UP_CATEGORY_BLOCK, "WMOTR 1UP BLOCKS" },
    { LEVEL_CASTLE, SM64AP_ONE_UP_CATEGORY_BUTTERFLY, "CASTLE BUTTERFLIES" },
    { LEVEL_WF, SM64AP_ONE_UP_CATEGORY_BUTTERFLY, "WF BUTTERFLIES" },
    { LEVEL_THI, SM64AP_ONE_UP_CATEGORY_BUTTERFLY, "THI BUTTERFLIES" },
    { LEVEL_TTM, SM64AP_ONE_UP_CATEGORY_BUTTERFLY, "TTM BUTTERFLIES" },
};

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

static int SM64AP_CoinCheckOffsetFromLocationId(int locId);

void SM64AP_RecvItem(int64_t idx, bool notify) {
    AP_EnableQueueItemRecvMsgs(true);

    if (idx == SM64AP_ID_GLOBAL_SIGNS) {
        sm64_have_global_signs = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

    if (idx >= SM64AP_SIGN_LEVEL_ITEM_OFFSET && idx <= SM64AP_SIGN_LEVEL_ITEM_END) {
        sm64_have_level_signs[idx - SM64AP_SIGN_LEVEL_ITEM_OFFSET] = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

    if (idx >= SM64AP_ONE_UP_GLOBAL_ITEM_OFFSET && idx < SM64AP_ONE_UP_LEVEL_ITEM_OFFSET) {
        int category = idx - SM64AP_ONE_UP_GLOBAL_ITEM_OFFSET;
        sm64_have_one_up_global_items[category] = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

    if (idx == SM64AP_ID_GLOBAL_BUTTERFLIES) {
        sm64_have_one_up_global_items[SM64AP_ONE_UP_CATEGORY_BUTTERFLY] = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

    if (idx >= SM64AP_ONE_UP_LEVEL_ITEM_OFFSET && idx <= SM64AP_ONE_UP_LEVEL_ITEM_END) {
        sm64_have_one_up_level_items[idx - SM64AP_ONE_UP_LEVEL_ITEM_OFFSET] = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

    if (idx >= SM64AP_COIN_GLOBAL_ITEM_OFFSET
        && idx < SM64AP_COIN_GLOBAL_ITEM_OFFSET + SM64AP_NUM_COIN_GLOBAL_ITEMS) {
        sm64_have_coin_global_items[idx - SM64AP_COIN_GLOBAL_ITEM_OFFSET] = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

    if (idx >= SM64AP_COIN_LEVEL_ITEM_OFFSET
        && idx < SM64AP_COIN_LEVEL_ITEM_OFFSET + SM64AP_NUM_COIN_LEVEL_ITEMS) {
        sm64_have_coin_level_items[idx - SM64AP_COIN_LEVEL_ITEM_OFFSET] = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

    for (int i = 0; i < SM64AP_NUM_ENEMY_UNLOCK_ITEMS; i++) {
        if (idx == sm64_enemy_unlock_items[i].id) {
            sm64_have_enemy_unlock_items[i] = true;
            SM64AP_RequestLiveObjectReconcile();
            return;
        }
    }

    if (idx >= SM64AP_ID_OBJECT_ITEM(0)
        && idx <= SM64AP_ID_OBJECT_ITEM(SM64AP_NUM_CONTIGUOUS_OBJECT_ITEMS - 1)
        && idx != SM64AP_ID_BITFS) {
        sm64_have_object_items[idx - SM64AP_OBJECT_ITEM_OFFSET] = true;
        SM64AP_RequestLiveObjectReconcile();
        return;
    }

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
        case SM64AP_ID_PROGRESSIVE_BOWSER_ARENA_BOMB:
            SM64AP_IncrementClamped(sm64_bowser_arena_bombs[0], 4);
            SM64AP_IncrementClamped(sm64_bowser_arena_bombs[1], 4);
            SM64AP_IncrementClamped(sm64_bowser_arena_bombs[2], 5);
            break;
        case SM64AP_ID_BITDW_PROGRESSIVE_BOWSER_ARENA_BOMB:
            SM64AP_IncrementClamped(sm64_bowser_arena_bombs[0], 4);
            break;
        case SM64AP_ID_BITFS_PROGRESSIVE_BOWSER_ARENA_BOMB:
            SM64AP_IncrementClamped(sm64_bowser_arena_bombs[1], 4);
            break;
        case SM64AP_ID_BITS_PROGRESSIVE_BOWSER_ARENA_BOMB:
            SM64AP_IncrementClamped(sm64_bowser_arena_bombs[2], 5);
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
        case SM64AP_ID_WMOTR_CANNON:
            sm64_have_wmotr_cannon = true;
            break;
        case SM64AP_ID_YOSHI:
            sm64_have_yoshi = true;
            break;
        case SM64AP_ID_BITFS:
            sm64_have_bitfs = true;
            break;
        case SM64AP_ID_VCUTM_ENTRANCE:
            sm64_have_vcutm_entrance = true;
            break;
        case SM64AP_ID_RR_LEVEL_UNLOCK:
            sm64_have_rr_level_unlock = true;
            break;
        case SM64AP_ID_WMOTR_LEVEL_UNLOCK:
            sm64_have_wmotr_level_unlock = true;
            break;
        case SM64AP_ID_BOWSER_STAGE_1UPS:
            sm64_have_bowser_stage_1ups = true;
            break;
        case SM64AP_ID_BITDW_1UPS:
            sm64_have_bitdw_1ups = true;
            break;
        case SM64AP_ID_BITFS_1UPS:
            sm64_have_bitfs_1ups = true;
            break;
        case SM64AP_ID_THI_HUGE_PAINTING:
            sm64_have_painting[COURSE_THI - 1] = true;
            break;
        case SM64AP_ID_HAT:
            if (!sm64_have_hat) {
                sm64_have_hat = true;
                if (notify) {
                    sm64_hat_restore_with_animation_pending = true;
                } else {
                    sm64_hat_restore_without_animation_pending = true;
                }
            }
            break;
        case SM64AP_ID_JRB_PURPLE_SWITCHES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_JRB_PURPLE_SWITCHES] = true;
            break;
        case SM64AP_ID_DDD_PURPLE_SWITCHES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_DDD_PURPLE_SWITCHES] = true;
            break;
        case SM64AP_ID_TTM_PURPLE_SWITCHES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_TTM_PURPLE_SWITCHES] = true;
            break;
        case SM64AP_ID_THI_PURPLE_SWITCHES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_THI_PURPLE_SWITCHES] = true;
            break;
        case SM64AP_ID_CCM_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_CCM_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_SSL_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_SSL_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_SL_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_SL_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_WDW_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_WDW_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_TTM_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_TTM_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_THI_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_THI_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_RR_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_RR_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_WMOTR_BOBOMB_BUDDY:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_WMOTR_BOBOMB_BUDDY] = true;
            break;
        case SM64AP_ID_GLOBAL_BOBOMB_BUDDIES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_GLOBAL_BOBOMB_BUDDIES] = true;
            break;
        case SM64AP_ID_JRB_TREASURE_CHESTS:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_JRB_TREASURE_CHESTS] = true;
            break;
        case SM64AP_ID_DDD_TREASURE_CHESTS:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_DDD_TREASURE_CHESTS] = true;
            break;
        case SM64AP_ID_GLOBAL_TREASURE_CHESTS:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_GLOBAL_TREASURE_CHESTS] = true;
            break;
        case SM64AP_ID_BITDW_WARP_PIPES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_BITDW_WARP_PIPES] = true;
            break;
        case SM64AP_ID_BITFS_WARP_PIPES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_BITFS_WARP_PIPES] = true;
            break;
        case SM64AP_ID_BITS_WARP_PIPES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_BITS_WARP_PIPES] = true;
            break;
        case SM64AP_ID_GLOBAL_WARP_PIPES:
            sm64_have_object_items[SM64AP_OBJECT_ITEM_GLOBAL_WARP_PIPES] = true;
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
        case SM64AP_ID_PROGRESSIVE_WING_CAP_LENGTH:
            sm64_cap_length_items[0]++;
            break;
        case SM64AP_ID_PROGRESSIVE_METAL_CAP_LENGTH:
            sm64_cap_length_items[1]++;
            break;
        case SM64AP_ID_PROGRESSIVE_VANISH_CAP_LENGTH:
            sm64_cap_length_items[2]++;
            break;
        case SM64AP_ID_LEVEL_CAP(0) ... SM64AP_ID_LEVEL_CAP(SM64AP_NUM_LEVEL_CAPS - 1):
            sm64_have_level_caps[idx - SM64AP_LEVEL_CAP_OFFSET] = true;
            break;
        case SM64AP_ID_CANNONUNLOCK(0) ... SM64AP_ID_CANNONUNLOCK(15-1):
            sm64_have_cannon[idx-(SM64AP_ID_CANNONUNLOCK(0))] = true;
            break;
        case SM64AP_ID_PAINTINGUNLOCK(0) ... SM64AP_ID_PAINTINGUNLOCK(NUM_PAINTING_LOCKS-1):
            // We don't have a painting unlock for BoB, so (0) will never appear; index 1 corresponds to WF, and so on
            if (idx == SM64AP_ID_THI_TINY_PAINTING) {
                sm64_have_thi_tiny_painting = true;
            } else {
                sm64_have_painting[idx-(SM64AP_ID_PAINTINGUNLOCK(0))] = true;
            }
            break;
        case SM64AP_ID_ABILITY(0):
            sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET+1] = sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET];
            sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET] = true;
            break;
        case SM64AP_ID_ABILITY(1) ... SM64AP_ID_ABILITY(SM64AP_NUM_ABILITIES-1):
            sm64_have_abilities[idx-SM64AP_ABILITY_OFFSET] = true;
            break;
        case SM64AP_ID_LEVEL_MOVE(0, 0) ... SM64AP_ID_LEVEL_MOVE_END:
            sm64_have_level_moves[idx - SM64AP_LEVEL_MOVE_OFFSET] = true;
            break;
        case SM64AP_ID_SECRET_LEVEL_MOVE(SM64AP_LEVEL_MOVE_AREA_BITDW, 0) ... SM64AP_ID_SECRET_LEVEL_MOVE_END:
            sm64_have_level_moves[
                SM64AP_LEVEL_MOVE_AREA_BITDW * SM64AP_NUM_LEVEL_MOVES
                + idx - SM64AP_SECRET_LEVEL_MOVE_OFFSET] = true;
            break;
        case SM64AP_ID_MISC_LEVEL_MOVE(0) ... SM64AP_ID_MISC_LEVEL_MOVE(SM64AP_NUM_LEVEL_MOVES - 1):
            sm64_have_level_moves[
                SM64AP_LEVEL_MOVE_AREA_MISC * SM64AP_NUM_LEVEL_MOVES
                + idx - SM64AP_MISC_LEVEL_MOVE_OFFSET] = true;
            break;
        case SM64AP_ID_1_HEALTH_PIP ... SM64AP_ID_GUST_TRAP:
            if(!notify) break;
            delayed_queue.push(idx);
            break;
        case SM64AP_ID_UNCOLLECT_COIN_TRAP:
            sm64_received_uncollect_coin_traps++;
            if (notify) {
                std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
                sm64_pending_uncollect_coin_traps.push(sm64_received_uncollect_coin_traps);
            }
            break;
        case SM64AP_ID_FEATURE(0) ... SM64AP_ID_FEATURE(SM64AP_NUM_FEATURES-1):
            sm64_have_features[idx-(SM64AP_ID_FEATURE(0))] = true;
            break;
    }
    SM64AP_RequestLiveObjectReconcile();
}

void SM64AP_CheckLocation(int64_t loc_id) {
    if (loc_id >= 4000000 && loc_id <= INT_MAX) {
        int coinLocation = static_cast<int>(loc_id);
        sm64_checked_coin_output_locations.insert(coinLocation);
    }

    if (loc_id < SM64AP_ID_OFFSET || loc_id >= SM64AP_ID_OFFSET + SM64AP_NUM_LOCS) {
        return;
    }

    sm64_locations[loc_id - SM64AP_ID_OFFSET] = true;

    int coinOffset = SM64AP_CoinCheckOffsetFromLocationId(loc_id);
    if (coinOffset >= 0) {
        sm64_sent_coin_checks[coinOffset] = true;
    }

    int oneUpOffset = loc_id - SM64AP_LOCATIONID_1UP_CHECK_START;
    if (oneUpOffset >= 0 && oneUpOffset < SM64AP_NUM_1UP_CHECKS) {
        sm64_sent_1up_checks[oneUpOffset] = true;
    }

    int blocksanityOffset = loc_id - SM64AP_LOCATIONID_BLOCKSANITY_START;
    if (blocksanityOffset >= 0 && blocksanityOffset < SM64AP_NUM_BLOCKSANITY_CHECKS) {
        sm64_sent_blocksanity_checks[blocksanityOffset] = true;
    }

    SM64AP_RequestLiveObjectReconcile();
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

static int SM64AP_CoinFormationSource(u32 behParam) {
    u8 flags = beh_param_second_byte(behParam);

    if (flags & COIN_FORMATION_FLAG_ARROW) {
        return SM64AP_COIN_SOURCE_ARROW;
    }
    if (flags & COIN_FORMATION_FLAG_RING) {
        if (flags & COIN_FORMATION_FLAG_VERTICAL) {
            return SM64AP_COIN_SOURCE_VERTICAL_RING;
        }
        return SM64AP_COIN_SOURCE_HORIZONTAL_RING;
    }
    if (flags & COIN_FORMATION_FLAG_VERTICAL) {
        return SM64AP_COIN_SOURCE_VERTICAL_LINE;
    }
    return SM64AP_COIN_SOURCE_HORIZONTAL_LINE;
}

static int SM64AP_PlacedCoinSource(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvYellowCoin) || behavior_is(behavior, bhvOneCoin)) {
        return SM64AP_COIN_SOURCE_YELLOW_COIN;
    }
    if (behavior_is(behavior, bhvRedCoin)) {
        return SM64AP_COIN_SOURCE_RED_COIN;
    }
    if (behavior_is(behavior, bhvMovingBlueCoin) || behavior_is(behavior, bhvBlueCoinSliding)) {
        return SM64AP_COIN_SOURCE_MOVING_BLUE_COIN;
    }
    if (behavior_is(behavior, bhvBlueCoinSwitch) || behavior_is(behavior, bhvHiddenBlueCoin)) {
        return SM64AP_COIN_SOURCE_BLUE_COIN_SWITCH;
    }
    if (behavior_is(behavior, bhvCoinFormation)) {
        return SM64AP_CoinFormationSource(behParam);
    }
    return -1;
}

static int SM64AP_CoinObjectSource(u32 behParam, const void *behavior) {
    u8 content = beh_param_second_byte(behParam);

    if (behavior_is(behavior, bhvExclamationBox)) {
        if ((behParam >> 16) == 0x1404) {
            return -1;
        }
        switch (content) {
            case 4:
                return SM64AP_COIN_SOURCE_ONE_COIN_BOX;
            case 5:
                return SM64AP_COIN_SOURCE_THREE_COIN_BOX;
            case 6:
                return SM64AP_COIN_SOURCE_TEN_COIN_BOX;
        }
    }
    if (behavior_is(behavior, bhvBreakableBox) && content == 1) {
        return SM64AP_COIN_SOURCE_BREAKABLE_COIN_BOX;
    }
    if (behavior_is(behavior, bhvBreakableBoxSmall)) {
        return SM64AP_COIN_SOURCE_SMALL_BREAKABLE_BOX;
    }
    if (behavior_is(behavior, bhvJumpingBox)) {
        return SM64AP_COIN_SOURCE_JUMPING_BOX;
    }
    if (behavior_is(behavior, bhvWoodenPost)) {
        return SM64AP_COIN_SOURCE_WOODEN_POST;
    }
    if (behavior_is(behavior, bhvLllBowserPuzzle)) {
        return SM64AP_COIN_SOURCE_BOWSER_PUZZLE;
    }
    return -1;
}

static int SM64AP_EnemyCoinSource(const void *behavior) {
    if (behavior_is(behavior, bhvBobomb)) return SM64AP_COIN_SOURCE_BOBOMB;
    if (behavior_is(behavior, bhvGhostHuntBigBoo)
        || behavior_is(behavior, bhvMerryGoRoundBigBoo)
        || behavior_is(behavior, bhvBalconyBigBoo)) return SM64AP_COIN_SOURCE_BIG_BOO;
    if (behavior_is(behavior, bhvBoo) || behavior_is(behavior, bhvGhostHuntBoo)
        || behavior_is(behavior, bhvMerryGoRoundBoo)
        || behavior_is(behavior, bhvMerryGoRoundBooManager)
        || behavior_is(behavior, bhvBooInCastle)
        || behavior_is(behavior, bhvBooWithCage)) return SM64AP_COIN_SOURCE_BOO;
    if (behavior_is(behavior, bhvSmallBully)) return SM64AP_COIN_SOURCE_BULLY;
    if (behavior_is(behavior, bhvChuckya)) return SM64AP_COIN_SOURCE_CHUCKYA;
    if (behavior_is(behavior, bhvEnemyLakitu)) return SM64AP_COIN_SOURCE_ENEMY_LAKITU;
    if (behavior_is(behavior, bhvEyerokBoss) || behavior_is(behavior, bhvEyerokHand)) {
        return SM64AP_COIN_SOURCE_EYEROK;
    }
    if (behavior_is(behavior, bhvFirePiranhaPlant)) return SM64AP_COIN_SOURCE_FIRE_PIRANHA_PLANT;
    if (behavior_is(behavior, bhvFlyGuy)) return SM64AP_COIN_SOURCE_FLY_GUY;
    if (behavior_is(behavior, bhvFlyingBookend) || behavior_is(behavior, bhvBookendSpawn)) {
        return SM64AP_COIN_SOURCE_FLYING_BOOKEND;
    }
    if (behavior_is(behavior, bhvGoomba) || behavior_is(behavior, bhvGoombaTripletSpawner)) {
        return SM64AP_COIN_SOURCE_GOOMBA;
    }
    if (behavior_is(behavior, bhvKoopa)) return SM64AP_COIN_SOURCE_KOOPA_TROOPA;
    if (behavior_is(behavior, bhvMoneybag) || behavior_is(behavior, bhvMoneybagHidden)) {
        return SM64AP_COIN_SOURCE_MONEYBAG;
    }
    if (behavior_is(behavior, bhvMrBlizzard)) return SM64AP_COIN_SOURCE_MR_BLIZZARD;
    if (behavior_is(behavior, bhvMrI)) return SM64AP_COIN_SOURCE_MR_I;
    if (behavior_is(behavior, bhvMontyMole) || behavior_is(behavior, bhvMontyMoleHole)) {
        return SM64AP_COIN_SOURCE_MONTY_MOLE;
    }
    if (behavior_is(behavior, bhvBigBully)) return SM64AP_COIN_SOURCE_BIG_BULLY;
    if (behavior_is(behavior, bhvPiranhaPlant)) return SM64AP_COIN_SOURCE_PIRANHA_PLANT;
    if (behavior_is(behavior, bhvPokey) || behavior_is(behavior, bhvPokeyBodyPart)) {
        return SM64AP_COIN_SOURCE_POKEY;
    }
    if (behavior_is(behavior, bhvScuttlebug) || behavior_is(behavior, bhvScuttlebugSpawn)) {
        return SM64AP_COIN_SOURCE_SCUTTLEBUG;
    }
    if (behavior_is(behavior, bhvSkeeter)) return SM64AP_COIN_SOURCE_SKEETER;
    if (behavior_is(behavior, bhvSnufit)) return SM64AP_COIN_SOURCE_SNUFIT;
    if (behavior_is(behavior, bhvSpindrift)) return SM64AP_COIN_SOURCE_SPINDRIFT;
    if (behavior_is(behavior, bhvSwoop)) return SM64AP_COIN_SOURCE_SWOOP;
    if (behavior_is(behavior, bhvSmallWhomp)) return SM64AP_COIN_SOURCE_WHOMP;
    if (behavior_is(behavior, bhvThwomp) || behavior_is(behavior, bhvThwomp2)
        || behavior_is(behavior, bhvGrindel)
        || behavior_is(behavior, bhvHorizontalGrindel)) return SM64AP_COIN_SOURCE_THWOMP;
    if (behavior_is(behavior, bhvHeaveHo)) return SM64AP_COIN_SOURCE_HEAVE_HO;
    return -1;
}

bool SM64AP_HaveFeature(int feature) {
    return feature >= 0 && feature < SM64AP_NUM_FEATURES && sm64_have_features[feature];
}

bool SM64AP_HaveObjectItem(int item) {
    return item >= 0 && item < SM64AP_NUM_OBJECT_ITEMS && sm64_have_object_items[item];
}

bool SM64AP_HaveCoinSource(int source, s16 level) {
    if (source == SM64AP_COIN_SOURCE_YELLOW_COIN && level == LEVEL_CASTLE_GROUNDS) {
        level = LEVEL_CASTLE;
    }

    for (int i = 0; i < SM64AP_NUM_COIN_GLOBAL_ITEMS; i++) {
        if (sm64_coin_global_items[i].source == source && sm64_have_coin_global_items[i]) {
            return true;
        }
    }

    for (int i = 0; i < SM64AP_NUM_COIN_LEVEL_ITEMS; i++) {
        if (sm64_coin_level_items[i].source == source
            && (sm64_coin_level_items[i].level == level || sm64_coin_level_items[i].level == -2)
            && sm64_have_coin_level_items[i]) {
            return true;
        }
    }

    return false;
}

static s16 SM64AP_NormalizeEnemyUnlockLevel(s16 level) {
    switch (level) {
        case LEVEL_BOWSER_1:
            return LEVEL_BITDW;
        case LEVEL_BOWSER_2:
            return LEVEL_BITFS;
        case LEVEL_BOWSER_3:
            return LEVEL_BITS;
        default:
            return level;
    }
}

static bool SM64AP_HaveEnemyUnlock(int source, s16 level) {
    level = SM64AP_NormalizeEnemyUnlockLevel(level);
    bool appliesToLevel = false;
    bool haveGlobal = false;

    for (int i = 0; i < SM64AP_NUM_ENEMY_UNLOCK_ITEMS; i++) {
        const SM64APEnemyUnlockItem &item = sm64_enemy_unlock_items[i];
        if (item.source != source) {
            continue;
        }
        if (item.level == -1) {
            haveGlobal |= sm64_have_enemy_unlock_items[i];
        } else if (item.level == level) {
            appliesToLevel = true;
            if (sm64_have_enemy_unlock_items[i]) {
                return true;
            }
        }
    }

    return !appliesToLevel || haveGlobal;
}

bool SM64AP_HaveBowser(s16 level) {
    return SM64AP_HaveEnemyUnlock(SM64AP_ENEMY_UNLOCK_BOWSER, level);
}

static bool SM64AP_IsEnemyCoinSource(int source) {
    return (source >= SM64AP_COIN_SOURCE_BOBOMB && source <= SM64AP_COIN_SOURCE_WHOMP)
        || source == SM64AP_COIN_SOURCE_BIG_BOO
        || source == SM64AP_COIN_SOURCE_THWOMP
        || source == SM64AP_COIN_SOURCE_HEAVE_HO;
}

static const SM64APCoinLevelItem *SM64AP_LevelCoinUnlockAt(s16 level, bool enemies, int index) {
    for (int i = 0; i < SM64AP_NUM_COIN_LEVEL_ITEMS; i++) {
        const SM64APCoinLevelItem &item = sm64_coin_level_items[i];
        if (item.source < 0 || item.level != level
            || SM64AP_IsEnemyCoinSource(item.source) != enemies) {
            continue;
        }
        if (index-- == 0) {
            return &item;
        }
    }
    return nullptr;
}

static const SM64APEnemyUnlockItem *SM64AP_LevelEnemyUnlockAt(s16 level, int index) {
    level = SM64AP_NormalizeEnemyUnlockLevel(level);
    for (int i = 0; i < SM64AP_NUM_ENEMY_UNLOCK_ITEMS; i++) {
        const SM64APEnemyUnlockItem &item = sm64_enemy_unlock_items[i];
        if (item.level != level) {
            continue;
        }
        if (index-- == 0) {
            return &item;
        }
    }
    return nullptr;
}

int SM64AP_LevelCoinUnlockCount(s16 level, bool enemies) {
    int count = 0;
    while (SM64AP_LevelCoinUnlockAt(level, enemies, count) != nullptr) {
        count++;
    }
    if (enemies) {
        int enemyIndex = 0;
        while (SM64AP_LevelEnemyUnlockAt(level, enemyIndex++) != nullptr) {
            count++;
        }
    }
    return count;
}

const char *SM64AP_LevelCoinUnlockName(s16 level, bool enemies, int index) {
    const SM64APCoinLevelItem *item = SM64AP_LevelCoinUnlockAt(level, enemies, index);
    if (item == nullptr) {
        if (!enemies) {
            return "";
        }
        int coinEnemyCount = 0;
        while (SM64AP_LevelCoinUnlockAt(level, true, coinEnemyCount) != nullptr) {
            coinEnemyCount++;
        }
        const SM64APEnemyUnlockItem *enemyItem =
            SM64AP_LevelEnemyUnlockAt(level, index - coinEnemyCount);
        if (enemyItem == nullptr) {
            return "";
        }
        switch (enemyItem->source) {
            case SM64AP_ENEMY_UNLOCK_BOWSER:             return "BOWSER";
            case SM64AP_ENEMY_UNLOCK_CHAIN_CHOMP:        return "CHAIN CHOMP";
            case SM64AP_ENEMY_UNLOCK_WIGGLER:            return "WIGGLER";
            case SM64AP_ENEMY_UNLOCK_TWEESTER:           return "TWEESTERS";
            case SM64AP_ENEMY_UNLOCK_KLEPTO:             return "KLEPTO";
            case SM64AP_ENEMY_UNLOCK_AMP:
                return (level == LEVEL_SL || level == LEVEL_VCUTM) ? "AMP" : "AMPS";
            case SM64AP_ENEMY_UNLOCK_MAD_PIANO:          return "MAD PIANO";
            case SM64AP_ENEMY_UNLOCK_HAUNTED_CHAIR:      return "CHAIRS";
            case SM64AP_ENEMY_UNLOCK_SUSHI_SHARK:        return "SUSHI SHARKS";
            case SM64AP_ENEMY_UNLOCK_BUBBA:              return "BUBBAS";
            case SM64AP_ENEMY_UNLOCK_TOX_BOX:            return "TOX BOXES";
            case SM64AP_ENEMY_UNLOCK_BOWLING_BALL:       return "BOWLING BALLS";
            case SM64AP_ENEMY_UNLOCK_WATER_BOMB:         return "WATER BOMBS";
            case SM64AP_ENEMY_UNLOCK_BOULDER:            return "BOULDERS";
            case SM64AP_ENEMY_UNLOCK_FIRE_HAZARD:        return "FIRE HAZARDS";
            case SM64AP_ENEMY_UNLOCK_BOUNCING_FIREBALL:  return "BOUNCE FIRE";
            case SM64AP_ENEMY_UNLOCK_SPINDEL:            return "SPINDEL";
            case SM64AP_ENEMY_UNLOCK_FALLING_PILLAR:     return "FALL PILLARS";
            case SM64AP_ENEMY_UNLOCK_FIRE_SPITTER:
                return (level == LEVEL_BITFS || level == LEVEL_DDD) ? "FIRE SPITTER" : "FIRE SPITTERS";
            case SM64AP_ENEMY_UNLOCK_BUB:                return "BUBS";
            default:                                     return "";
        }
    }
    if (item->name == nullptr) {
        return "";
    }

    switch (item->source) {
        case SM64AP_COIN_SOURCE_YELLOW_COIN:         return "S YELLOW COINS";
        case SM64AP_COIN_SOURCE_RED_COIN:            return "RED COINS";
        case SM64AP_COIN_SOURCE_MOVING_BLUE_COIN:    return "SINGLE B COIN";
        case SM64AP_COIN_SOURCE_BLUE_COIN_SWITCH:    return "BLUE COIN BLK";
        case SM64AP_COIN_SOURCE_HORIZONTAL_LINE:     return "H COIN LINE";
        case SM64AP_COIN_SOURCE_HORIZONTAL_RING:     return "H COIN RING";
        case SM64AP_COIN_SOURCE_ARROW:               return "COIN ARROWS";
        case SM64AP_COIN_SOURCE_VERTICAL_LINE:       return "V COIN LINE";
        case SM64AP_COIN_SOURCE_VERTICAL_RING:       return "V COIN RING";
        case SM64AP_COIN_SOURCE_BREAKABLE_COIN_BOX:  return "COIN BOXES";
        case SM64AP_COIN_SOURCE_SMALL_BREAKABLE_BOX: return "CORK BOXES";
        case SM64AP_COIN_SOURCE_JUMPING_BOX:         return "CRAZY BOXES";
        case SM64AP_COIN_SOURCE_WOODEN_POST:         return "WOOD POSTS";
        case SM64AP_COIN_SOURCE_BOWSER_PUZZLE:       return "BOWSER PUZZLE";
        case SM64AP_COIN_SOURCE_THREE_COIN_BOX:      return "3 COIN BLOCK";
        case SM64AP_COIN_SOURCE_TEN_COIN_BOX:        return "10 COIN BLOCK";
        case SM64AP_COIN_SOURCE_BOBOMB:              return level == LEVEL_BITFS ? "BOB OMB" : "BOB OMBS";
        case SM64AP_COIN_SOURCE_BOO:                 return "BOOS";
        case SM64AP_COIN_SOURCE_BULLY:               return "BULLIES";
        case SM64AP_COIN_SOURCE_CHUCKYA:             return "CHUCKYA";
        case SM64AP_COIN_SOURCE_ENEMY_LAKITU:        return level == LEVEL_RR ? "LAKITUS" : "LAKITU";
        case SM64AP_COIN_SOURCE_EYEROK:              return "EYEROK";
        case SM64AP_COIN_SOURCE_FIRE_PIRANHA_PLANT:  return "FIRE PIRANHAS";
        case SM64AP_COIN_SOURCE_FLY_GUY:
            return (level == LEVEL_SSL || level == LEVEL_THI) ? "FLY GUYS" : "FLY GUY";
        case SM64AP_COIN_SOURCE_FLYING_BOOKEND:      return "BOOKENDS";
        case SM64AP_COIN_SOURCE_GOOMBA:              return level == LEVEL_RR ? "GOOMBA" : "GOOMBAS";
        case SM64AP_COIN_SOURCE_KOOPA_TROOPA:
            return level == LEVEL_THI ? "KOOPA TROOPAS" : "KOOPA TROOPA";
        case SM64AP_COIN_SOURCE_MONEYBAG:            return "MONEYBAGS";
        case SM64AP_COIN_SOURCE_MR_BLIZZARD:         return "MR BLIZZARDS";
        case SM64AP_COIN_SOURCE_MR_I:                return "MR IS";
        case SM64AP_COIN_SOURCE_SCUTTLEBUG:          return "SCUTTLEBUGS";
        case SM64AP_COIN_SOURCE_PIRANHA_PLANT:       return "PIRANHA PLANTS";
        case SM64AP_COIN_SOURCE_POKEY:               return "POKEYS";
        case SM64AP_COIN_SOURCE_SKEETER:             return "SKEETERS";
        case SM64AP_COIN_SOURCE_SNUFIT:              return "SNUFITS";
        case SM64AP_COIN_SOURCE_SPINDRIFT:           return "SPINDRIFTS";
        case SM64AP_COIN_SOURCE_SWOOP:               return "SWOOPS";
        case SM64AP_COIN_SOURCE_WHOMP:               return "WHOMPS";
        case SM64AP_COIN_SOURCE_MONTY_MOLE:          return "MONTY MOLES";
        case SM64AP_COIN_SOURCE_BIG_BULLY:
            return level == LEVEL_LLL ? "BIG BULLIES" : "CHILL BULLY";
        case SM64AP_COIN_SOURCE_BIG_BOO:             return "BIG BOOS";
        case SM64AP_COIN_SOURCE_THWOMP:
            if (SM64AP_NormalizeEnemyUnlockLevel(level) == LEVEL_SSL) return "GRINDEL";
            return level == LEVEL_WF ? "THWOMPS" : "THWOMP";
        case SM64AP_COIN_SOURCE_HEAVE_HO:            return level == LEVEL_WDW ? "HEAVE HOS" : "HEAVE HO";
        default:                                     return "";
    }
}

bool SM64AP_LevelCoinUnlockEnabled(s16 level, bool enemies, int index) {
    const SM64APCoinLevelItem *item = SM64AP_LevelCoinUnlockAt(level, enemies, index);
    if (item != nullptr) {
        return SM64AP_HaveCoinSource(item->source, level);
    }
    if (!enemies) {
        return false;
    }
    int coinEnemyCount = 0;
    while (SM64AP_LevelCoinUnlockAt(level, true, coinEnemyCount) != nullptr) {
        coinEnemyCount++;
    }
    const SM64APEnemyUnlockItem *enemyItem =
        SM64AP_LevelEnemyUnlockAt(level, index - coinEnemyCount);
    return enemyItem != nullptr && SM64AP_HaveEnemyUnlock(enemyItem->source, level);
}

static int SM64AP_LevelSpecificObjectItemForLevel(int item, s16 level) {
    switch (item) {
        case SM64AP_OBJECT_ITEM_CHECKERBOARD_PLATFORMS:
            switch (level) {
                case LEVEL_BOB:
                    return SM64AP_OBJECT_ITEM_BOB_CHECKERBOARD_PLATFORMS;
                case LEVEL_WF:
                    return SM64AP_OBJECT_ITEM_WF_CHECKERBOARD_PLATFORMS;
                case LEVEL_LLL:
                    return SM64AP_OBJECT_ITEM_LLL_CHECKERBOARD_PLATFORMS;
                case LEVEL_HMC:
                    return SM64AP_OBJECT_ITEM_HMC_CHECKERBOARD_PLATFORMS;
                case LEVEL_VCUTM:
                    return SM64AP_OBJECT_ITEM_VCUTM_CHECKERBOARD_PLATFORMS;
            }
            break;

        case SM64AP_OBJECT_ITEM_ROLLING_LOGS:
            switch (level) {
                case LEVEL_LLL:
                    return SM64AP_OBJECT_ITEM_LLL_ROLLING_LOGS;
                case LEVEL_TTM:
                    return SM64AP_OBJECT_ITEM_TTM_ROLLING_LOGS;
            }
            break;

        case SM64AP_OBJECT_ITEM_PURPLE_SWITCHES:
            switch (level) {
                case LEVEL_BOB:
                    return SM64AP_OBJECT_ITEM_BOB_PURPLE_SWITCHES;
                case LEVEL_JRB:
                    return SM64AP_OBJECT_ITEM_JRB_PURPLE_SWITCHES;
                case LEVEL_HMC:
                    return SM64AP_OBJECT_ITEM_HMC_PURPLE_SWITCHES;
                case LEVEL_DDD:
                    return SM64AP_OBJECT_ITEM_DDD_PURPLE_SWITCHES;
                case LEVEL_WDW:
                    return SM64AP_OBJECT_ITEM_WDW_PURPLE_SWITCHES;
                case LEVEL_TTM:
                    return SM64AP_OBJECT_ITEM_TTM_PURPLE_SWITCHES;
                case LEVEL_THI:
                    return SM64AP_OBJECT_ITEM_THI_PURPLE_SWITCHES;
                case LEVEL_RR:
                    return SM64AP_OBJECT_ITEM_RR_PURPLE_SWITCHES;
                case LEVEL_BITDW:
                    return SM64AP_OBJECT_ITEM_BITDW_PURPLE_SWITCHES;
                case LEVEL_BITS:
                    return SM64AP_OBJECT_ITEM_BITS_PURPLE_SWITCHES;
            }
            break;

        case SM64AP_OBJECT_ITEM_THI_WARP_PIPES:
            switch (level) {
                case LEVEL_THI:
                    return SM64AP_OBJECT_ITEM_THI_WARP_PIPES;
                case LEVEL_BITDW:
                    return SM64AP_OBJECT_ITEM_BITDW_WARP_PIPES;
                case LEVEL_BITFS:
                    return SM64AP_OBJECT_ITEM_BITFS_WARP_PIPES;
                case LEVEL_BITS:
                    return SM64AP_OBJECT_ITEM_BITS_WARP_PIPES;
            }
            break;
    }

    return -1;
}

static int SM64AP_LevelForCourseIndex(int courseIdx) {
    switch (courseIdx) {
        case COURSE_BOB - 1:
            return LEVEL_BOB;
        case COURSE_WF - 1:
            return LEVEL_WF;
        case COURSE_JRB - 1:
            return LEVEL_JRB;
        case COURSE_CCM - 1:
            return LEVEL_CCM;
        case COURSE_BBH - 1:
            return LEVEL_BBH;
        case COURSE_HMC - 1:
            return LEVEL_HMC;
        case COURSE_LLL - 1:
            return LEVEL_LLL;
        case COURSE_SSL - 1:
            return LEVEL_SSL;
        case COURSE_DDD - 1:
            return LEVEL_DDD;
        case COURSE_SL - 1:
            return LEVEL_SL;
        case COURSE_WDW - 1:
            return LEVEL_WDW;
        case COURSE_TTM - 1:
            return LEVEL_TTM;
        case COURSE_THI - 1:
            return LEVEL_THI;
        case COURSE_TTC - 1:
            return LEVEL_TTC;
        case COURSE_RR - 1:
            return LEVEL_RR;
    }

    return 0;
}

bool SM64AP_HaveObjectItemForLevel(int item, s16 level) {
    if (item == SM64AP_OBJECT_ITEM_THI_WARP_PIPES
        && SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_GLOBAL_WARP_PIPES)) {
        return true;
    }
    return SM64AP_HaveObjectItem(item)
        || SM64AP_HaveObjectItem(SM64AP_LevelSpecificObjectItemForLevel(item, level));
}

static s16 SM64AP_NormalizeSignLevel(s16 level) {
    if (level == LEVEL_CASTLE_GROUNDS || level == LEVEL_CASTLE_COURTYARD) {
        return LEVEL_CASTLE;
    }
    return level;
}

bool SM64AP_HaveSigns(s16 level) {
    if (sm64_have_global_signs) {
        return true;
    }
    level = SM64AP_NormalizeSignLevel(level);
    for (int i = 0; i < SM64AP_NUM_SIGN_LEVEL_ITEMS; i++) {
        if (SM64AP_SIGN_LEVEL_ITEMS[i].level == level) {
            return sm64_have_level_signs[i];
        }
    }
    return true;
}

bool SM64AP_LevelHasSignUnlock(s16 level) {
    level = SM64AP_NormalizeSignLevel(level);
    for (int i = 0; i < SM64AP_NUM_SIGN_LEVEL_ITEMS; i++) {
        if (SM64AP_SIGN_LEVEL_ITEMS[i].level == level) {
            return true;
        }
    }
    return false;
}

bool SM64AP_HaveObjectItemForCourse(int item, int courseIdx) {
    int level = SM64AP_LevelForCourseIndex(courseIdx);

    if (level != 0) {
        return SM64AP_HaveObjectItemForLevel(item, level);
    }

    return SM64AP_HaveObjectItem(item);
}

bool SM64AP_ShouldSpawnBowserStageOneUp(s16 level, s16 param, u32 saveFlags) {
    if (!sm64_bowser_stage_1up_item_behavior) {
        if (param == 1) {
            return SM64AP_HaveKey1()
                || (saveFlags & SAVE_FLAG_UNLOCKED_BASEMENT_DOOR) != 0;
        }
        if (param == 2) {
            return SM64AP_HaveKey2()
                || (saveFlags & SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR) != 0;
        }
        return true;
    }

    switch (level) {
        case LEVEL_BITDW:
            return sm64_have_bowser_stage_1ups || sm64_have_bitdw_1ups;
        case LEVEL_BITFS:
            return sm64_have_bowser_stage_1ups || sm64_have_bitfs_1ups;
    }

    return true;
}

bool SM64AP_HaveBITFS() {
    return sm64_have_bitfs;
}

bool SM64AP_HaveHat() {
    return sm64_have_hat;
}

bool SM64AP_HaveBowserStageExtraOneUps(s16 level) {
    if (!sm64_bowser_stage_1up_item_behavior) {
        return true;
    }
    if (level == LEVEL_BITDW) {
        return sm64_have_bowser_stage_1ups || sm64_have_bitdw_1ups;
    }
    if (level == LEVEL_BITFS) {
        return sm64_have_bowser_stage_1ups || sm64_have_bitfs_1ups;
    }
    return true;
}

bool SM64AP_HaveVcutmEntrance() {
    return sm64_have_vcutm_entrance;
}

bool SM64AP_HatRestoreWithAnimationPending() {
    return sm64_hat_restore_with_animation_pending;
}

bool SM64AP_HatRestoreWithoutAnimationPending() {
    return sm64_hat_restore_without_animation_pending;
}

void SM64AP_HatRestoreComplete() {
    sm64_hat_restore_with_animation_pending = false;
    sm64_hat_restore_without_animation_pending = false;
}

bool SM64AP_CollectedCourseStar(int courseIdx, int starIdx) {
    return courseIdx >= 0 && starIdx >= 0 && starIdx < 7
        && (SM64AP_CourseStarFlags(courseIdx) & (1 << starIdx));
}

u32 SM64AP_ServerSaveFlags() {
    return sm64_server_save_flags;
}

static void SM64AP_UpdateServerBits(const std::string &suffix, const char *operation, u32 value) {
    int operand = static_cast<int>(value);
    int defaultValue = 0;
    AP_SetServerDataRequest request;
    AP_DataStorageOperation update = { operation, &operand };
    request.key = AP_GetPrivateServerDataPrefix() + suffix;
    request.operations = { update };
    request.default_value = &defaultValue;
    request.type = AP_DataType::Int;
    request.want_reply = true;
    AP_SetServerData(&request);
}

static void SM64AP_SetServerInt(const std::string &key, const char *operation, int value) {
    int defaultValue = 0;
    AP_SetServerDataRequest request;
    AP_DataStorageOperation update = { operation, &value };
    request.key = key;
    request.operations = { update };
    request.default_value = &defaultValue;
    request.type = AP_DataType::Int;
    request.want_reply = true;
    AP_SetServerData(&request);
}

void SM64AP_SetServerSaveFlags(u32 flags) {
    sm64_server_save_flags |= flags;
    SM64AP_UpdateServerBits("SaveFlags", "or", flags);
}

void SM64AP_ClearServerSaveFlags(u32 flags) {
    sm64_server_save_flags &= ~flags;
    SM64AP_UpdateServerBits("SaveFlags", "and", ~flags);
}

u32 SM64AP_ServerCannonFlags() {
    return sm64_server_cannon_flags;
}

void SM64AP_SetServerCannonFlag(int course) {
    if (course < 0 || course >= 32) return;
    u32 flag = 1U << course;
    sm64_server_cannon_flags |= flag;
    SM64AP_UpdateServerBits("CannonFlags", "or", flag);
}

int SM64AP_ServerCoinHighScore(int course) {
    return course >= 0 && course < COURSE_STAGES_COUNT
        ? std::max(sm64_server_coin_high_scores[course], 0) : 0;
}

void SM64AP_SetServerCoinHighScore(int course, int score) {
    if (course < 0 || course >= COURSE_STAGES_COUNT
        || score <= sm64_server_coin_high_scores[course]) return;
    sm64_server_coin_high_scores[course] = score;
    AP_SetServerDataRequest request;
    AP_DataStorageOperation update = { "max", &score };
    int defaultValue = 0;
    request.key = AP_GetPrivateServerDataPrefix() + "CoinHighScore;" + std::to_string(course);
    request.operations = { update };
    request.default_value = &defaultValue;
    request.type = AP_DataType::Int;
    request.want_reply = true;
    AP_SetServerData(&request);
}

static bool SM64AP_IsKoopaTheQuick(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvKoopaRaceEndpoint)) {
        return true;
    }

    if (!behavior_is(behavior, bhvKoopa)) {
        return false;
    }

    switch (beh_param_second_byte(behParam)) {
        case KOOPA_BP_KOOPA_THE_QUICK_BOB:
        case KOOPA_BP_KOOPA_THE_QUICK_THI:
            return true;
        default:
            return false;
    }
}

bool SM64AP_HaveBobombBuddy(s16 level) {
    if (SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_GLOBAL_BOBOMB_BUDDIES)) {
        return true;
    }
    switch (level) {
        case LEVEL_BOB:
            return SM64AP_HaveFeature(SM64AP_FEATURE_BOB_BOBOMB_BUDDY);
        case LEVEL_WF:
            return SM64AP_HaveFeature(SM64AP_FEATURE_WF_BOBOMB_BUDDY);
        case LEVEL_JRB:
            return SM64AP_HaveFeature(SM64AP_FEATURE_JRB_BOBOMB_BUDDY);
        case LEVEL_CCM:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_CCM_BOBOMB_BUDDY);
        case LEVEL_SSL:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_SSL_BOBOMB_BUDDY);
        case LEVEL_SL:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_SL_BOBOMB_BUDDY);
        case LEVEL_WDW:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_WDW_BOBOMB_BUDDY);
        case LEVEL_TTM:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_TTM_BOBOMB_BUDDY);
        case LEVEL_THI:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_THI_BOBOMB_BUDDY);
        case LEVEL_RR:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_RR_BOBOMB_BUDDY);
        case LEVEL_WMOTR:
            return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_WMOTR_BOBOMB_BUDDY);
        default:
            return true;
    }
}

bool SM64AP_HaveTreasureChests(s16 level) {
    if (SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_GLOBAL_TREASURE_CHESTS)) {
        return true;
    }
    if (level == LEVEL_JRB) {
        return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_JRB_TREASURE_CHESTS);
    }
    if (level == LEVEL_DDD) {
        return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_DDD_TREASURE_CHESTS);
    }
    return true;
}

static int SM64AP_EnemyUnlockSource(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvBowser)) return SM64AP_ENEMY_UNLOCK_BOWSER;
    if (behavior_is(behavior, bhvChainChomp)) return SM64AP_ENEMY_UNLOCK_CHAIN_CHOMP;
    if (behavior_is(behavior, bhvWigglerHead)) return SM64AP_ENEMY_UNLOCK_WIGGLER;
    if (behavior_is(behavior, bhvTweester)) return SM64AP_ENEMY_UNLOCK_TWEESTER;
    if (behavior_is(behavior, bhvKlepto) && beh_param_second_byte(behParam) == 0) {
        return SM64AP_ENEMY_UNLOCK_KLEPTO;
    }
    if (behavior_is(behavior, bhvHomingAmp) || behavior_is(behavior, bhvCirclingAmp)) {
        return SM64AP_ENEMY_UNLOCK_AMP;
    }
    if (behavior_is(behavior, bhvMadPiano)) return SM64AP_ENEMY_UNLOCK_MAD_PIANO;
    if (behavior_is(behavior, bhvHauntedChair)) return SM64AP_ENEMY_UNLOCK_HAUNTED_CHAIR;
    if (behavior_is(behavior, bhvSushiShark)) return SM64AP_ENEMY_UNLOCK_SUSHI_SHARK;
    if (behavior_is(behavior, bhvBubba)) return SM64AP_ENEMY_UNLOCK_BUBBA;
    if (behavior_is(behavior, bhvToxBox)) return SM64AP_ENEMY_UNLOCK_TOX_BOX;
    if (behavior_is(behavior, bhvBobBowlingBallSpawner)
        || behavior_is(behavior, bhvTtmBowlingBallSpawner)
        || behavior_is(behavior, bhvThiBowlingBallSpawner)
        || behavior_is(behavior, bhvPitBowlingBall)) {
        return SM64AP_ENEMY_UNLOCK_BOWLING_BALL;
    }
    if (behavior_is(behavior, bhvWaterBombSpawner)
        || behavior_is(behavior, bhvWaterBombCannon)) {
        return SM64AP_ENEMY_UNLOCK_WATER_BOMB;
    }
    if (behavior_is(behavior, bhvBigBoulderGenerator)) return SM64AP_ENEMY_UNLOCK_BOULDER;
    if (behavior_is(behavior, bhvFlamethrower)
        || behavior_is(behavior, bhvLllRotatingBlockWithFireBars)) {
        return SM64AP_ENEMY_UNLOCK_FIRE_HAZARD;
    }
    if (behavior_is(behavior, bhvBouncingFireball)) {
        return SM64AP_ENEMY_UNLOCK_BOUNCING_FIREBALL;
    }
    if (behavior_is(behavior, bhvSpindel)) return SM64AP_ENEMY_UNLOCK_SPINDEL;
    if (behavior_is(behavior, bhvFallingPillar)) return SM64AP_ENEMY_UNLOCK_FALLING_PILLAR;
    if (behavior_is(behavior, bhvFireSpitter)) return SM64AP_ENEMY_UNLOCK_FIRE_SPITTER;
    if (behavior_is(behavior, bhvFish2) || behavior_is(behavior, bhvFish3)) {
        return SM64AP_ENEMY_UNLOCK_BUB;
    }
    return -1;
}

static bool SM64AP_ShouldSpawnBobObject(s16 x, s16 y, s16 z, u32 behParam, const void *behavior) {
    bool haveBobombBuddy = SM64AP_HaveBobombBuddy(LEVEL_BOB);
    bool haveBobCannon = SM64AP_HaveCannon(AP_COURSE_BOB);

    if (behavior_is(behavior, bhvKingBobomb)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_BOB_KING_BOBOMB);
    }
    if (SM64AP_IsKoopaTheQuick(behParam, behavior)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_BOB_KOOPA_THE_QUICK);
    }
    if (behavior_is(behavior, bhvBobombBuddyOpensCannon)) {
        return haveBobombBuddy;
    }
    if (behavior_is(behavior, bhvCannonClosed)) {
        return true;
    }
    if (behavior_is(behavior, bhvWaterBombCannon)
        && x == -5694 && y == 128 && z == 5600) {
        return !haveBobCannon;
    }
    if (behavior_is(behavior, bhvBobombBuddy)) {
        return haveBobombBuddy;
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
        return SM64AP_HaveBobombBuddy(LEVEL_WF);
    }
    if (behavior_is(behavior, bhvHoot)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_WF_HOOT);
    }
    return true;
}

static bool SM64AP_ShouldSpawnCcmObject(const void *behavior) {
    if (behavior_is(behavior, bhvSmallPenguin)) {
        return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_CCM_BABY_PENGUINS);
    }
    if (behavior_is(behavior, bhvSnowmansBottom)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_CCM_SNOWMANS_BODY);
    }
    if (behavior_is(behavior, bhvSnowmansHead)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_CCM_SNOWMANS_BODY);
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
        return SM64AP_HaveBobombBuddy(LEVEL_JRB);
    }
    if (behavior_is(behavior, bhvTreasureChestsJrb)
        || behavior_is(behavior, bhvTreasureChestsShip)) {
        return SM64AP_HaveTreasureChests(LEVEL_JRB);
    }
    if (behavior_is(behavior, bhvJetStream)
        || (behavior_is(behavior, bhvStar) && beh_param_star(behParam) == 5)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_JRB_JET_STREAM);
    }
    if (behavior_is(behavior, bhvUnagi)) {
        bool star2Collected = SM64AP_CollectedCourseStar(AP_COURSE_JRB, 1);
        if (beh_param_second_byte(behParam) == 0) {
            return SM64AP_HaveFeature(SM64AP_FEATURE_JRB_SUNKEN_SHIP)
                && !SM64AP_HaveFeature(SM64AP_FEATURE_JRB_UNAGI);
        }
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
        return beh_param_second_byte(behParam) != 0
            ? SM64AP_HaveFeature(SM64AP_FEATURE_SSL_KLEPTO_STAR)
            : SM64AP_HaveEnemyUnlock(SM64AP_ENEMY_UNLOCK_KLEPTO, LEVEL_SSL);
    }
    return true;
}

static bool SM64AP_ShouldSpawnDddObject(u32 behParam, const void *behavior) {
    if (behavior_is(behavior, bhvTreasureChests)) {
        return SM64AP_HaveTreasureChests(LEVEL_DDD);
    }
    if (behavior_is(behavior, bhvMantaRay)) {
        return SM64AP_HaveFeature(SM64AP_FEATURE_DDD_MANTA_RAY);
    }
    if (behavior_is(behavior, bhvBowserSubDoor)) {
        return !SM64AP_HaveFeature(SM64AP_FEATURE_DDD_POLES);
    }
    if (behavior_is(behavior, bhvBowsersSub)) {
        return behParam == 0x000B0000 && SM64AP_HaveFeature(SM64AP_FEATURE_DDD_BOWSERS_SUB);
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

static int SM64AP_BowserArenaIndex(s16 level) {
    if (level == LEVEL_BOWSER_1) return 0;
    if (level == LEVEL_BOWSER_2) return 1;
    if (level == LEVEL_BOWSER_3) return 2;
    return -1;
}

static int SM64AP_BowserArenaBombIndex(s16 level, s16 x, s16 z) {
    static constexpr s16 bombPositions[3][5][2] = {
        {{ 2949, 0 }, { 0, -2949 }, { 0, 2949 }, { -2949, 0 }, { 0, 0 }},
        {{ 4, 3598 }, { 3584, 0 }, { 0, -3583 }, { -3583, 0 }, { 0, 0 }},
        {{ -2122, -2912 }, { -3362, 1121 }, { 0, 3584 }, { 3363, 1121 }, { 2123, -2912 }},
    };
    int arena = SM64AP_BowserArenaIndex(level);
    if (arena < 0) return -1;
    int bombCount = arena == 2 ? 5 : 4;
    for (int i = 0; i < bombCount; i++) {
        if (bombPositions[arena][i][0] == x && bombPositions[arena][i][1] == z) return i;
    }
    return -1;
}

static int SM64AP_OneUpPlacementSourceType(const void *behavior) {
    if (behavior_is(behavior, bhv1Up)) return SM64AP_1UP_SOURCE_OBJECT;
    if (behavior_is(behavior, bhv1upSliding)) return SM64AP_1UP_SOURCE_SLIDING;
    if (behavior_is(behavior, bhv1upJumpOnApproach)) return SM64AP_1UP_SOURCE_JUMP_ON_APPROACH;
    if (behavior_is(behavior, bhvHidden1up)
        || behavior_is(behavior, bhvHidden1upTrigger)) {
        return SM64AP_1UP_SOURCE_HIDDEN;
    }
    if (behavior_is(behavior, bhvHidden1upInPole)
        || behavior_is(behavior, bhvHidden1upInPoleTrigger)
        || behavior_is(behavior, bhvHidden1upInPoleSpawner)) {
        return SM64AP_1UP_SOURCE_HIDDEN_POLE;
    }
    if (behavior_is(behavior, bhv1upWalking)) return SM64AP_1UP_SOURCE_WALKING;
    if (behavior_is(behavior, bhv1upRunningAway)) return SM64AP_1UP_SOURCE_RUNNING_AWAY;
    return -1;
}

bool SM64AP_ShouldSpawnLevelObject(s16 level, s16, s16 model, s16 x, s16 y, s16 z, u32 behParam, const void *behavior) {
    if ((behavior_is(behavior, bhvMessagePanel) || behavior_is(behavior, bhvSignOnWall))
        && !SM64AP_HaveSigns(level)) {
        return false;
    }
    if (behavior_is(behavior, bhvToadMessage)) {
        return SM64AP_HaveToads();
    }

    if (behavior_is(behavior, bhvMips)) {
        return SM64AP_MipsSpawnTier() >= 0;
    }

    if (behavior_is(behavior, bhvYoshi)) {
        return SM64AP_HaveYoshi();
    }

    if (behavior_is(behavior, bhvHiddenAt120Stars)) {
        return !SM64AP_HaveCastleCannon();
    }

    if (behavior_is(behavior, bhvBobombBuddyOpensCannon)
        && !SM64AP_HaveBobombBuddy(level)) {
        return false;
    }

    if (behavior_is(behavior, bhv1Up)
        && (((behParam >> 16) & 0xFF) == 1 || ((behParam >> 16) & 0xFF) == 2)) {
        return SM64AP_ShouldSpawnBowserStageOneUp(
            level, (behParam >> 16) & 0xFF, save_file_get_flags());
    }

    int oneUpSource = SM64AP_OneUpPlacementSourceType(behavior);
    if (oneUpSource >= 0 && !SM64AP_HaveOneUpSource(level, oneUpSource)) {
        return false;
    }

    if (behavior_is(behavior, bhvBowserBomb)) {
        int arena = SM64AP_BowserArenaIndex(level);
        int bomb = SM64AP_BowserArenaBombIndex(level, x, z);
        if (arena >= 0 && bomb >= 0) return bomb < sm64_bowser_arena_bombs[arena];
    }

    int enemyUnlockSource = SM64AP_EnemyUnlockSource(behParam, behavior);
    if (enemyUnlockSource >= 0 && !SM64AP_HaveEnemyUnlock(enemyUnlockSource, level)) {
        return false;
    }

    if (behavior_is(behavior, bhvHiddenRedCoinStar)
        || behavior_is(behavior, bhvBowserCourseRedCoinStar)) {
        int courseIndex = gCurrCourseNum - COURSE_MIN;
        int starIndex = (behParam >> 24) & 0xFF;
        if (!SM64AP_HaveCoinSource(SM64AP_COIN_SOURCE_RED_COIN, level)
            || SM64AP_CollectedCourseStar(courseIndex, starIndex)) {
            return false;
        }
    }

    if (level == LEVEL_BITDW && behavior_is(behavior, bhvRedCoin)
        && ((x == -3100 && y == -2900 && z == 4520)
            || (x == -7810 && y == -3100 && z == 4900))
        && (!SM64AP_HaveCoinSource(SM64AP_COIN_SOURCE_RED_COIN, level)
            || !SM64AP_HaveObjectItemForLevel(SM64AP_OBJECT_ITEM_PURPLE_SWITCHES, level))) {
        return false;
    }

    if (behavior_is(behavior, bhvGhostHuntBigBoo)
        && (!SM64AP_HaveCoinSource(SM64AP_COIN_SOURCE_BOO, level)
            || !SM64AP_HaveCoinSource(SM64AP_COIN_SOURCE_BIG_BOO, level))) {
        return false;
    }

    int coinSource = SM64AP_PlacedCoinSource(behParam, behavior);
    if (coinSource >= 0 && !SM64AP_HaveCoinSource(coinSource, level)) {
        return false;
    }

    int coinObjectSource = SM64AP_CoinObjectSource(behParam, behavior);
    // Locked exclamation boxes remain as intangible outlines until their unlock arrives.
    if (coinObjectSource >= 0
        && !behavior_is(behavior, bhvExclamationBox)
        && !SM64AP_HaveCoinSource(coinObjectSource, level)) {
        return false;
    }

    int enemySource = SM64AP_EnemyCoinSource(behavior);
    if (behavior_is(behavior, bhvCourtyardBooTriplet)) {
        return SM64AP_HaveCoinSource(SM64AP_COIN_SOURCE_BOO, LEVEL_CASTLE_COURTYARD);
    }
    if (behavior_is(behavior, bhvBigBullyWithMinions)
        && !SM64AP_HaveCoinSource(SM64AP_COIN_SOURCE_BULLY, level)) {
        return false;
    }
    if (behavior_is(behavior, bhvBooInCastle) || behavior_is(behavior, bhvBooWithCage)) {
        return SM64AP_HaveBBH();
    }
    if (enemySource >= 0
        && !SM64AP_IsKoopaTheQuick(behParam, behavior)
        && !SM64AP_HaveCoinSource(enemySource, level)) {
        return false;
    }

    if (SM64AP_IsPurpleSwitchObject(model, behavior)) {
        return SM64AP_HaveObjectItemForLevel(SM64AP_OBJECT_ITEM_PURPLE_SWITCHES, level);
    }

    if (SM64AP_IsCheckerboardPlatformObject(level, model, behavior)) {
        return SM64AP_HaveObjectItemForLevel(SM64AP_OBJECT_ITEM_CHECKERBOARD_PLATFORMS, level);
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
                return SM64AP_HaveObjectItemForLevel(SM64AP_OBJECT_ITEM_ROLLING_LOGS, level);
            }
            return true;
        case LEVEL_SSL:
            return SM64AP_ShouldSpawnSslObject(behParam, behavior);
        case LEVEL_THI:
            if (behavior_is(behavior, bhvWarpPipe)) {
                return SM64AP_HaveObjectItemForLevel(SM64AP_OBJECT_ITEM_THI_WARP_PIPES, level);
            }
            if (SM64AP_IsKoopaTheQuick(behParam, behavior)) {
                return SM64AP_HaveFeature(SM64AP_FEATURE_THI_KOOPA_THE_QUICK);
            }
            return true;
        case LEVEL_BITDW:
        case LEVEL_BITFS:
        case LEVEL_BITS:
            if (behavior_is(behavior, bhvWarpPipe)) {
                return SM64AP_HaveObjectItemForLevel(SM64AP_OBJECT_ITEM_THI_WARP_PIPES, level);
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
        case LEVEL_WDW:
            if (behavior_is(behavior, bhvWaterLevelDiamond)) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_WDW_WATER_LEVEL_DIAMONDS);
            }
            return true;
        case LEVEL_RR:
            if (behavior_is(behavior, bhvPlatformOnTrack) && model == MODEL_RR_FLYING_CARPET) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_RR_CARPETS);
            }
            return true;
        case LEVEL_TTM:
            if (behavior_is(behavior, bhvTtmRollingLog)) {
                return SM64AP_HaveObjectItemForLevel(SM64AP_OBJECT_ITEM_ROLLING_LOGS, level);
            }
            if (behavior_is(behavior, bhvUkiki) || behavior_is(behavior, bhvUkikiCage)) {
                return SM64AP_HaveFeature(SM64AP_FEATURE_TTM_UKIKI);
            }
            return true;
        case LEVEL_TTC:
            if (behavior_is(behavior, bhvTTCSpinner) && model == MODEL_TTC_SPINNER) {
                return SM64AP_HaveObjectItem(SM64AP_OBJECT_ITEM_TTC_SPINNERS);
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
            return;
        case LEVEL_BITS:
        case LEVEL_BOWSER_3:
            *oldnode = 0x6B;
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

static void SM64AP_DiscoverEntrance(int sourceEntrance) {
    if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated) {
        return;
    }

    for (int bit = 0; bit < static_cast<int>(ARRAY_COUNT(sm64_shuffled_entrance_ids)); bit++) {
        if (sm64_shuffled_entrance_ids[bit] == sourceEntrance) {
            std::string key = "SM64SpicyFoundEntrances_" + std::to_string(AP_GetPlayerID());
            SM64AP_SetServerInt(key, "or", 1 << bit);
            return;
        }
    }
}

static void SM64AP_DiscoverSubAreaEntrance(int sourceId) {
    if (AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated
        || sourceId <= 0 || sourceId >= 64) {
        return;
    }

    const bool high = sourceId >= 32;
    const int bit = high ? sourceId - 32 : sourceId - 1;
    std::string key = high
        ? "SM64SpicyFoundSubAreaEntrancesHigh_"
        : "SM64SpicyFoundSubAreaEntrancesLow_";
    key += std::to_string(AP_GetPlayerID());
    SM64AP_SetServerInt(key, "or", 1 << bit);
}

static void SM64AP_DiscoverRandomizedSource(int sourceId) {
    if (sourceId > 0 && sourceId < 64) {
        SM64AP_DiscoverSubAreaEntrance(sourceId);
    } else {
        SM64AP_DiscoverEntrance(sourceId);
    }
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

static void SM64AP_ApplySubAreaDestination(
    int destination, s16* destLevel, s16* destArea, s16* destWarpNode, s32* warpArg
) {
    int variant = (destination >> 24) & 0x0F;
    int level = (destination >> 16) & 0xFF;

    SM64AP_SetWDWEntranceVariant(0);
    *destLevel = level;
    *destArea = (destination >> 8) & 0xFF;
    *destWarpNode = destination & 0xFF;
    *warpArg = (destination >> 28) & 0x0F;
    sm64_ccm_slide_exit_arrival_pending =
        level == LEVEL_CCM && *destArea == 1 && *destWarpNode == 0x14;

    if (level == LEVEL_WDW && variant != 0) {
        SM64AP_SetWDWEntranceVariant(variant);
    } else if (level == LEVEL_TTC && variant != 0) {
        SM64AP_SetTTCEntranceVariantSpeed(variant);
    }
}

bool SM64AP_ConsumeCCMSlideExitArrival(s16 level, s8 area) {
    bool shouldSpawnStar = sm64_ccm_slide_exit_arrival_pending
        && level == LEVEL_CCM && area == 1;
    sm64_ccm_slide_exit_arrival_pending = false;
    return shouldSpawnStar;
}

static int SM64AP_PhysicalSubAreaSource(s16 level, s8 area, s16 warpNode) {
    switch (level) {
        case LEVEL_CCM:
            if (area == 1 && warpNode == 0x1E) return 1;
            if (area == 2 && warpNode == 0x14) return 21;
            break;
        case LEVEL_SL:
            if (area == 1 && warpNode == 0x0C) return 2;
            if (area == 2 && warpNode == 0x0B) return 22;
            break;
        case LEVEL_TTM:
            if (area == 1 && warpNode >= 0x00 && warpNode <= 0x02) return 3;
            if (area == 4 && warpNode == 0x0A) return 23;
            break;
        case LEVEL_THI:
            if (area == 1 && warpNode == 0x0C) return 4;
            if (area == 1 && warpNode == 0x0D) return 10;
            if (area == 3 && warpNode == 0x0C) return 24;
            break;
        case LEVEL_HMC:
            if (area == 1 && warpNode == 0x0B) return 5;
            break;
        case LEVEL_JRB:
            if (area == 1 && warpNode == 0xF3) return 6;
            break;
        case LEVEL_LLL:
            if (area == 1 && warpNode == 0x0B) return 7;
            break;
        case LEVEL_SSL:
            if (area == 1 && warpNode == 0x14) return 8;
            if (area == 1 && warpNode == 0x1E) return 9;
            break;
        case LEVEL_BITDW:
            if (area == 1 && warpNode == 0x0B) return 11;
            break;
        case LEVEL_BITFS:
            if (area == 1 && warpNode == 0x0B) return 12;
            break;
        case LEVEL_BITS:
            if (area == 1 && warpNode == 0x0B) return 13;
            break;
        case LEVEL_PSS:
            if (area == 1 && warpNode == 0xF3) return 31;
            break;
        case LEVEL_TOTWC:
            if (area == 1 && warpNode == 0xF3) return 32;
            break;
        case LEVEL_VCUTM:
            if (area == 1 && warpNode == 0xF3) return 33;
            break;
        case LEVEL_COTMC:
            if (area == 1 && warpNode == 0xF3) return 34;
            break;
        case LEVEL_DDD:
            if (area == 2 && warpNode == 0xF3) return 35;
            break;
        case LEVEL_WMOTR:
            if (area == 1 && warpNode == 0xF3) return 36;
            break;
    }
    return 0;
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

static void SM64AP_PushNormalReturnPoint(s16 level, s8 area, int entranceLevel) {
    SM64APReturnPoint point = {};
    point.level = level;
    point.area = area;
    point.entranceLevel = entranceLevel;
    point.normalCastleEntrance = true;
    sm64_return_stack.push_back(point);
}

static s16 SM64AP_SubAreaReturnNode(s16 sourceWarpNode, bool hasSourceNode) {
    return hasSourceNode ? sourceWarpNode : 0x0A;
}

static void SM64AP_PushSubAreaReturnPoint(
    int sourceId, s16 level, s8 area, s16 sourceWarpNode
) {
    struct ObjectWarpNode *sourceNode = area_get_warp_node(sourceWarpNode);
    bool hasSourceNode = sourceNode != nullptr && sourceNode->object != nullptr;
    SM64APReturnPoint point = {};
    point.level = level;
    point.area = area;
    point.node = SM64AP_SubAreaReturnNode(sourceWarpNode, hasSourceNode);
    point.sourceId = sourceId;
    bool bowserArenaEntrance = sourceId >= 11 && sourceId <= 13;
    point.starSpawnType = bowserArenaEntrance ? MARIO_SPAWN_UNKNOWN_03
        : hasSourceNode ? get_mario_spawn_type(sourceNode->object)
                        : MARIO_SPAWN_SPIN_AIRBORNE;
    if (sourceId == 4) {
        // THI's Red Coin Cave uses a generic warp trigger, not a pipe.
        point.starSpawnType = MARIO_SPAWN_AIRBORNE_STAR_COLLECT;
    }
    point.reverseStarFacing = bowserArenaEntrance;
    point.overridePosition = !hasSourceNode || sourceId == 4 || sourceId == 8;
    if (sourceId == 4) {
        // The Huge Island cave entrance is itself a warp trigger. Place Mario
        // farther outside the cave so a return cannot immediately re-enter it.
        point.pos[0] = 410;
        point.pos[1] = -400;
        point.pos[2] = 1200;
        point.yaw = 0;
    } else if (sourceId == 8) {
        // The pyramid's side warp sits at the bottom of a slope and immediately
        // catches arrivals. Return to the flat ground above the entrance.
        point.pos[0] = -2048;
        point.pos[1] = 300;
        point.pos[2] = 768;
        point.yaw = 0;
    } else if (point.overridePosition) {
        point.yaw = gMarioState->faceAngle[1];
        constexpr float angleToRadians = 3.14159265358979323846f / 32768.0f;
        float yawRadians = point.yaw * angleToRadians;
        point.pos[0] = (s16) (gMarioState->pos[0] - 300.0f * std::sin(yawRadians));
        point.pos[1] = (s16) gMarioState->pos[1];
        point.pos[2] = (s16) (gMarioState->pos[2] - 300.0f * std::cos(yawRadians));
    }
    sm64_return_stack.push_back(point);
}

static void SM64AP_SetPendingReturnSpawn(
    const SM64APReturnPoint &point, u32 spawnType, bool overridePosition,
    bool moveAwayFromWarp = false, bool reverseFacing = false
) {
    sm64_pending_return_spawn.active = true;
    sm64_pending_return_spawn.overridePosition = overridePosition;
    sm64_pending_return_spawn.spawnType = spawnType;
    sm64_pending_return_spawn.moveAwayFromWarp = moveAwayFromWarp;
    sm64_pending_return_spawn.reverseFacing = reverseFacing;
    sm64_pending_return_spawn.yaw = point.yaw;
    for (int i = 0; i < 3; i++) {
        sm64_pending_return_spawn.pos[i] = point.pos[i];
    }
}

static int SM64AP_ResolveReturnStyle(
    bool isDeathWarp, int warpOp, int returnStyleOverride
) {
    if (returnStyleOverride != SM64AP_RETURN_STYLE_AUTO) {
        return returnStyleOverride;
    }
    if (isDeathWarp || warpOp == WARP_OP_DEATH) {
        return SM64AP_RETURN_STYLE_DEATH;
    }
    return SM64AP_RETURN_STYLE_STAR;
}

static bool SM64AP_TryReturnToPreviousEntrance(
    s16* destLevel, s16* destArea, s16* destWarpNode, s32* warpArg,
    bool isDeathWarp, int warpOp, int returnStyleOverride
) {
    bool isReturnWarp = returnStyleOverride != SM64AP_RETURN_STYLE_AUTO
        || isDeathWarp || warpOp == WARP_OP_DEATH || warpOp == WARP_OP_STAR_EXIT;
    if (!isReturnWarp || sm64_return_stack.empty()) {
        return false;
    }

    int returnStyle = SM64AP_ResolveReturnStyle(isDeathWarp, warpOp, returnStyleOverride);
    SM64APReturnPoint point = sm64_return_stack.back();
    sm64_return_stack.pop_back();
    *destLevel = point.level;
    *destArea = point.area;
    *warpArg = 0;

    if (point.normalCastleEntrance) {
        bool deathNode = returnStyle != SM64AP_RETURN_STYLE_STAR;
        int nodeWarpOp = returnStyle == SM64AP_RETURN_STYLE_STAR
            ? WARP_OP_STAR_EXIT : WARP_OP_NONE;
        setCourseNodeAndArea(point.entranceLevel, destWarpNode, deathNode, nodeWarpOp);
        if (returnStyle == SM64AP_RETURN_STYLE_GROUND) {
            SM64AP_SetPendingReturnSpawn(
                point, MARIO_SPAWN_INSTANT_ACTIVE, false, true);
        } else if (returnStyle == SM64AP_RETURN_STYLE_DEATH) {
            SM64AP_SetPendingReturnSpawn(point, MARIO_SPAWN_DEATH, false, true);
        } else if (returnStyle == SM64AP_RETURN_STYLE_STAR
                   && point.entranceLevel == LEVEL_BITS) {
            SM64AP_SetPendingReturnSpawn(point, MARIO_SPAWN_LAUNCH_STAR_COLLECT, false);
        }
        return true;
    }

    *destWarpNode = point.node;
    bool ccmChimneyReturn = point.sourceId == 1;
    u32 spawnType = MARIO_SPAWN_INSTANT_ACTIVE;
    if (returnStyle == SM64AP_RETURN_STYLE_DEATH) {
        spawnType = ccmChimneyReturn ? MARIO_SPAWN_LAUNCH_DEATH : MARIO_SPAWN_DEATH;
    } else if (returnStyle == SM64AP_RETURN_STYLE_GROUND && ccmChimneyReturn) {
        // An instant spawn remains inside the chimney warp and immediately
        // sends Mario back through it. Emerge upward as a normal pipe return.
        spawnType = point.starSpawnType;
    } else if (returnStyle == SM64AP_RETURN_STYLE_STAR) {
        spawnType = point.starSpawnType != 0
            ? point.starSpawnType : MARIO_SPAWN_SPIN_AIRBORNE;
    }
    SM64AP_SetPendingReturnSpawn(
        point, spawnType, point.overridePosition, false,
        returnStyle == SM64AP_RETURN_STYLE_STAR && point.reverseStarFacing);
    return true;
}

bool SM64AP_ApplyPendingReturnSpawn(s16* pos, s16* angle, u32* spawnType, s32* actionArg) {
    if (!sm64_pending_return_spawn.active) {
        return false;
    }
    if (sm64_pending_return_spawn.overridePosition) {
        for (int i = 0; i < 3; i++) {
            pos[i] = sm64_pending_return_spawn.pos[i];
        }
        angle[0] = 0;
        angle[1] = sm64_pending_return_spawn.yaw;
        angle[2] = 0;
    } else if (sm64_pending_return_spawn.moveAwayFromWarp) {
        constexpr float angleToRadians = 3.14159265358979323846f / 32768.0f;
        float yawRadians = angle[1] * angleToRadians;
        pos[0] = (s16) (pos[0] - 400.0f * std::sin(yawRadians));
        pos[2] = (s16) (pos[2] - 400.0f * std::cos(yawRadians));

        struct Surface *floor = nullptr;
        float floorHeight = find_floor(pos[0], pos[1] + 1000.0f, pos[2], &floor);
        if (floor != nullptr) {
            pos[1] = (s16) (floorHeight + 100.0f);
        }
    }
    if (sm64_pending_return_spawn.reverseFacing) {
        angle[1] += 0x8000;
    }
    *spawnType = sm64_pending_return_spawn.spawnType;
    *actionArg = 0;
    if (*spawnType != MARIO_SPAWN_PAINTING_STAR_COLLECT
        && *spawnType != MARIO_SPAWN_AIRBORNE_STAR_COLLECT
        && *spawnType != MARIO_SPAWN_LAUNCH_STAR_COLLECT) {
        gPauseExitCourseSkipDoneScreen = false;
    }
    sm64_pending_return_spawn.active = false;
    return true;
}

void SM64AP_ClearReturnStack() {
    sm64_return_stack.clear();
    sm64_pending_return_spawn.active = false;
}

void SM64AP_RedirectWarp(s16* curLevel, s16* destLevel, s8* curArea, s16* destArea,
                         s16* destWarpNode, bool isDeathWarp, int warpOp,
                         s32 sourceEntrance, s16 sourceWarpNode, s32* warpArg,
                         int returnStyleOverride) {
    // When warping, always lock the clock and reset var to avoid segfault if old clock val is not in new area
    SM64AP_SetClockToTTCState();
    if (*destWarpNode >= WARP_NODE_CREDITS_MIN
        || warpOp == WARP_OP_CREDITS_START || warpOp == WARP_OP_CREDITS_NEXT
        || warpOp == WARP_OP_CREDITS_END) {
        SM64AP_ClearReturnStack();
        return;
    }

    if (SM64AP_TryReturnToPreviousEntrance(
            destLevel, destArea, destWarpNode, warpArg,
            isDeathWarp, warpOp, returnStyleOverride)) {
        return;
    }

    int subAreaSource = SM64AP_PhysicalSubAreaSource(*curLevel, *curArea, sourceWarpNode);
    if (subAreaSource >= 1 && subAreaSource <= 13) {
        SM64AP_PushSubAreaReturnPoint(subAreaSource, *curLevel, *curArea, sourceWarpNode);
    }

    int normalSourceKey = 0;
    if ((*curLevel == LEVEL_CASTLE || *curLevel == LEVEL_CASTLE_COURTYARD
         || *curLevel == LEVEL_CASTLE_GROUNDS)
        && *destLevel != LEVEL_CASTLE && *destLevel != LEVEL_CASTLE_COURTYARD
        && *destLevel != LEVEL_CASTLE_GROUNDS) {
        normalSourceKey = SM64AP_SourceEntranceKey(*destLevel, *destArea, sourceEntrance);
        SM64AP_PushNormalReturnPoint(*curLevel, *curArea, normalSourceKey / 10);
        auto normalSubArea = map_sub_area_entrances.find(1000 + normalSourceKey);
        if (normalSubArea != map_sub_area_entrances.end()) {
            SM64AP_DiscoverEntrance(normalSourceKey);
            SM64AP_ApplySubAreaDestination(
                normalSubArea->second, destLevel, destArea, destWarpNode, warpArg);
            return;
        }

        int destination = SM64AP_GetMappedEntrance(normalSourceKey);
        SM64AP_DiscoverEntrance(normalSourceKey);
        SM64AP_ApplyEntranceDestination(destination, destLevel, destArea);
        *destWarpNode = 0x0A;
        return;
    }

    auto subArea = map_sub_area_entrances.find(subAreaSource);
    if (subAreaSource != 0 && subArea != map_sub_area_entrances.end()) {
        SM64AP_DiscoverSubAreaEntrance(subAreaSource);
        SM64AP_ApplySubAreaDestination(
            subArea->second, destLevel, destArea, destWarpNode, warpArg);
        return;
    }

    if (*destLevel == LEVEL_BOWSER_3 || *curLevel == LEVEL_BOWSER_3 ||
        *destLevel == LEVEL_BITS || *curLevel == LEVEL_BITS) return; // Dont play around with this one
    if (*curLevel == LEVEL_HMC && *destLevel == LEVEL_COTMC) {
        int sourceKey = SM64AP_SourceEntranceKey(*destLevel, *destArea, sourceEntrance);
        int destination = SM64AP_GetMappedEntrance(sourceKey);
        SM64AP_DiscoverEntrance(sourceKey);
        SM64AP_ApplyEntranceDestination(destination, destLevel, destArea);
        *destWarpNode = 0x0A;
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

void SM64AP_SetSubAreaMap(std::map<int,int> map) {
    map_sub_area_entrances = map;
}

static void SM64AP_ApplyStartInventory() {
    for (auto item : map_start_inventory) {
        for (int i = 0; i < item.second; i++) {
            SM64AP_RecvItem(item.first, false);
        }
    }
}

void SM64AP_SetStartInventory(std::map<int,int> map) {
    map_start_inventory = map;
    SM64AP_ApplyStartInventory();
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

void SM64AP_SetSkyboxShuffleMode(int mode) {
    sm64_skybox_shuffle_mode =
        mode == SM64AP_SKYBOX_SHUFFLE_MAP || mode == SM64AP_SKYBOX_SHUFFLE_RANDOM_ON_LOAD
        ? mode : SM64AP_SKYBOX_SHUFFLE_OFF;
}

void SM64AP_SetGlobalCapDisplay(int enabled) {
    sm64_show_global_cap_display = enabled != 0;
}

void SM64AP_SetOneUpChecks(int enabled) {
    sm64_1up_checks_enabled = enabled != 0;
}

static void SM64AP_SetWingCapLengthItemCount(int count) {
    sm64_cap_length_item_counts[0] = std::max(0, count);
}

static void SM64AP_SetMetalCapLengthItemCount(int count) {
    sm64_cap_length_item_counts[1] = std::max(0, count);
}

static void SM64AP_SetVanishCapLengthItemCount(int count) {
    sm64_cap_length_item_counts[2] = std::max(0, count);
}

u16 SM64AP_ScaleCapTimer(u32 capFlag, u16 baseTimer) {
    int capIndex;
    switch (capFlag) {
        case MARIO_WING_CAP: capIndex = 0; break;
        case MARIO_METAL_CAP: capIndex = 1; break;
        case MARIO_VANISH_CAP: capIndex = 2; break;
        default: return baseTimer;
    }

    int totalItems = sm64_cap_length_item_counts[capIndex];
    if (totalItems <= 0) {
        return baseTimer;
    }
    int receivedItems = std::min(sm64_cap_length_items[capIndex], totalItems);
    return static_cast<u16>(baseTimer + (baseTimer * receivedItems) / totalItems);
}

void SM64AP_SetBuddyChecks(int enabled) {
    sm64_buddy_checks_enabled = enabled != 0;
}

void SM64AP_SetBowserStageOneUpBehavior(int behavior) {
    sm64_bowser_stage_1up_item_behavior = behavior != 0;
}

void SM64AP_SetEasyButterflies(int enabled) {
    sm64_easy_butterflies = enabled != 0;
}

void SM64AP_SetNoDespawn(int enabled) {
    sm64_no_despawn = enabled != 0;
}

static void SM64AP_SetMipsSkipEnabled(int enabled) {
    sm64_mips_skip_enabled = enabled != 0;
    SM64AP_RequestLiveObjectReconcile();
}

static void SM64AP_SetBowserInTheDarkWorldHits(int hits) {
    sm64_bowser_hit_requirements[0] = hits;
}

static void SM64AP_SetBowserInTheFireSeaHits(int hits) {
    sm64_bowser_hit_requirements[1] = hits;
}

static void SM64AP_SetBowserInTheSkyHits(int hits) {
    sm64_bowser_hit_requirements[2] = hits;
}

static void SM64AP_SetBowserInTheSkyStageCollapseHits(int hits) {
    sm64_bowser_in_the_sky_stage_collapse_hits = hits;
}

int SM64AP_BowserHitRequirement(s16 level) {
    int arena = SM64AP_BowserArenaIndex(level);
    return arena >= 0 ? sm64_bowser_hit_requirements[arena] : 1;
}

int SM64AP_BowserInTheSkyStageCollapseHits() {
    return sm64_bowser_in_the_sky_stage_collapse_hits;
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

static bool SM64AP_ParseJsonString(
    const std::string &text, std::string::size_type &pos, std::string &value) {
    SM64AP_SkipJsonWhitespace(text, pos);
    if (pos >= text.size() || text[pos++] != '"') return false;
    value.clear();
    while (pos < text.size()) {
        char c = text[pos++];
        if (c == '"') return true;
        if (c != '\\') {
            value += c;
            continue;
        }
        if (pos >= text.size()) return false;
        c = text[pos++];
        switch (c) {
            case '"': case '\\': case '/': value += c; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            case 'u':
                if (pos + 4 > text.size()) return false;
                pos += 4;
                value += ' ';
                break;
            default: return false;
        }
    }
    return false;
}

static void SM64AP_SetSignHintData(std::string rawHints) {
    std::map<int, SM64APSignHint> parsed;
    std::string::size_type pos = 0;
    if (!SM64AP_ConsumeJsonChar(rawHints, pos, '{')) return;
    while (true) {
        SM64AP_SkipJsonWhitespace(rawHints, pos);
        if (pos < rawHints.size() && rawHints[pos] == '}') {
            pos++;
            break;
        }

        int key = 0;
        int location = 0;
        int entrance = 0;
        int locationPlayer = 0;
        std::string hint;
        if (!SM64AP_ParseJsonQuotedIntKey(rawHints, pos, key)
            || !SM64AP_ConsumeJsonChar(rawHints, pos, ':')
            || !SM64AP_ConsumeJsonChar(rawHints, pos, '[')
            || !SM64AP_ParseJsonString(rawHints, pos, hint)
            || !SM64AP_ConsumeJsonChar(rawHints, pos, ',')
            || !SM64AP_ParseJsonInt(rawHints, pos, location)) {
            return;
        }
        SM64AP_SkipJsonWhitespace(rawHints, pos);
        if (pos < rawHints.size() && rawHints[pos] == ',') {
            pos++;
            if (!SM64AP_ParseJsonInt(rawHints, pos, entrance)) return;
        }
        SM64AP_SkipJsonWhitespace(rawHints, pos);
        if (pos < rawHints.size() && rawHints[pos] == ',') {
            pos++;
            if (!SM64AP_ParseJsonInt(rawHints, pos, locationPlayer)) return;
        }
        if (!SM64AP_ConsumeJsonChar(rawHints, pos, ']')) return;
        parsed[key] = SM64APSignHint{ std::move(hint), location, entrance, locationPlayer };

        SM64AP_SkipJsonWhitespace(rawHints, pos);
        if (pos < rawHints.size() && rawHints[pos] == ',') {
            pos++;
        } else if (pos >= rawHints.size() || rawHints[pos] != '}') {
            return;
        }
    }
    SM64AP_SkipJsonWhitespace(rawHints, pos);
    if (pos != rawHints.size()) return;

    std::lock_guard<std::mutex> lock(sm64_sign_hint_mutex);
    sm64_sign_hints = std::move(parsed);
}

static int SM64AP_EncodeSignCharacter(char c) {
#if defined(VERSION_JP) || defined(VERSION_SH)
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
#endif
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 0x0A;
    if (c >= 'a' && c <= 'z') return c - 'a' + 0x24;
    switch (c) {
#if !defined(VERSION_JP) && !defined(VERSION_SH)
        case '\'': return 0x3E;
#endif
#if defined(VERSION_JP) || defined(VERSION_SH)
        case '.': return DIALOG_CHAR_PERIOD;
#else
        case '.': return 0x3F;
#endif
        case ',': return DIALOG_CHAR_COMMA;
        case '-': return 0x9F;
        case '(': return 0xE1;
        case ')': return 0xE3;
        case '&': return 0xE5;
        case '!': return 0xF2;
        case '%': return 0xF3;
        case '?': return 0xF4;
#if !defined(VERSION_JP) && !defined(VERSION_SH)
        case '/': return DIALOG_CHAR_SLASH;
#else
        case ':': return 0xE6;
#endif
        default: return -1;
    }
}

static void SM64AP_EncodeSignHint(const std::string &text) {
#if defined(VERSION_JP) || defined(VERSION_SH)
    constexpr size_t lineWidth = 12;
#else
    constexpr size_t lineWidth = 24;
#endif
    size_t lineLength = 0;
    size_t input = 0;
    bool openingQuote = true;
    sm64_sign_dialog_text.clear();
    sm64_sign_dialog_text.reserve(text.size() + text.size() / lineWidth + 2);

    while (input < text.size()) {
        while (input < text.size() && std::isspace(static_cast<unsigned char>(text[input]))) {
            if (text[input] == '\r' || text[input] == '\n') {
                if (text[input] == '\r' && input + 1 < text.size() && text[input + 1] == '\n') {
                    input++;
                }
                sm64_sign_dialog_text.push_back(DIALOG_CHAR_NEWLINE);
                lineLength = 0;
            }
            input++;
        }
        if (input >= text.size()) break;

        size_t wordEnd = input;
        while (wordEnd < text.size() && !std::isspace(static_cast<unsigned char>(text[wordEnd]))) {
            wordEnd++;
        }
        size_t wordLength = wordEnd - input;
        if (lineLength != 0 && lineLength + 1 + wordLength > lineWidth) {
            sm64_sign_dialog_text.push_back(DIALOG_CHAR_NEWLINE);
            lineLength = 0;
        } else if (lineLength != 0) {
            sm64_sign_dialog_text.push_back(DIALOG_CHAR_SPACE);
            lineLength++;
        }

        while (input < wordEnd) {
            char character = text[input++];
            int encoded;
            if (character == '"') {
                encoded = openingQuote ? 0xF5 : 0xF6;
                openingQuote = !openingQuote;
            } else {
                encoded = SM64AP_EncodeSignCharacter(character);
            }
            if (encoded < 0) continue;
            if (lineLength == lineWidth) {
                sm64_sign_dialog_text.push_back(DIALOG_CHAR_NEWLINE);
                lineLength = 0;
            }
            sm64_sign_dialog_text.push_back(static_cast<u8>(encoded));
            lineLength++;
        }
    }
    sm64_sign_dialog_text.push_back(DIALOG_CHAR_TERMINATOR);
    sm64_sign_dialog_entry.str = sm64_sign_dialog_text.data();
}

void SM64AP_ReadSign(s16 level, s16 dialog) {
    SM64APSignHint hint;
    {
        std::lock_guard<std::mutex> lock(sm64_sign_hint_mutex);
        auto entry = sm64_sign_hints.find(level * 256 + dialog);
        if (entry == sm64_sign_hints.end()) {
            sm64_active_sign_dialog = -1;
            return;
        }
        hint = entry->second;
    }

    SM64AP_EncodeSignHint(hint.text);
    sm64_active_sign_dialog = dialog;
    if (hint.location > 0 && hint.locationPlayer > 0
        && AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
        std::ostringstream request;
        request << "[{\"cmd\":\"CreateHints\",\"player\":" << hint.locationPlayer
                << ",\"locations\":[" << hint.location << "]}]";
        SM64AP_SendSerializedRequest(request.str());
    }
    if (hint.entrance > 0) {
        SM64AP_DiscoverRandomizedSource(hint.entrance);
    }
}

static void SM64AP_IgnoreLocationInfo(std::vector<AP_NetworkItem>) {}

struct DialogEntry *SM64AP_GetSignDialogEntry(s16 dialog, struct DialogEntry *vanilla) {
    return gMarioState != nullptr
        && gMarioState->action == ACT_READING_SIGN
        && dialog == sm64_active_sign_dialog
        ? &sm64_sign_dialog_entry : vanilla;
}

static bool SM64AP_ParseJsonIntMap(const std::string &rawMap, std::map<int,int> &parsedMap) {
    parsedMap.clear();

    std::string::size_type pos = 0;
    SM64AP_SkipJsonWhitespace(rawMap, pos);
    if (pos >= rawMap.size() || rawMap.compare(pos, 4, "null") == 0) {
        return true;
    }

    if (!SM64AP_ConsumeJsonChar(rawMap, pos, '{')) {
        return false;
    }

    SM64AP_SkipJsonWhitespace(rawMap, pos);
    if (pos < rawMap.size() && rawMap[pos] == '}') {
        pos++;
    } else {
        while (pos < rawMap.size()) {
            int key = 0;
            int value = 0;
            if (!SM64AP_ParseJsonQuotedIntKey(rawMap, pos, key)
                || !SM64AP_ConsumeJsonChar(rawMap, pos, ':')
                || !SM64AP_ParseJsonInt(rawMap, pos, value)) {
                return false;
            }

            parsedMap[key] = value;

            SM64AP_SkipJsonWhitespace(rawMap, pos);
            if (pos < rawMap.size() && rawMap[pos] == ',') {
                pos++;
                continue;
            }
            if (pos < rawMap.size() && rawMap[pos] == '}') {
                pos++;
                break;
            }
            return false;
        }
    }

    SM64AP_SkipJsonWhitespace(rawMap, pos);
    return pos == rawMap.size();
}

static void SM64AP_SetStartInventory(std::string rawMap) {
    std::map<int,int> parsedMap;
    if (!SM64AP_ParseJsonIntMap(rawMap, parsedMap)) {
        parsedMap.clear();
    }
    SM64AP_SetStartInventory(parsedMap);
}

static void SM64AP_SetCourseMap(std::string rawMap) {
    std::map<int,int> parsedMap;
    if (!SM64AP_ParseJsonIntMap(rawMap, parsedMap)) {
        parsedMap.clear();
    }
    SM64AP_SetCourseMap(parsedMap);
}

static void SM64AP_SetSubAreaMap(std::string rawMap) {
    std::map<int,int> parsedMap;
    if (!SM64AP_ParseJsonIntMap(rawMap, parsedMap)) {
        parsedMap.clear();
    }
    SM64AP_SetSubAreaMap(parsedMap);
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

static void SM64AP_SetSkyboxMap(std::string rawMap) {
    if (!SM64AP_ParseJsonIntMap(rawMap, map_skybox)) {
        map_skybox.clear();
    }
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
    SM64AP_SetMarioHatColor(255, 0, 0);
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
    u8 shirtColor[3];
    u8 hatColor[3] = { 255, 0, 0 };
    u8 skinColor[3] = { 254, 193, 121 };
    u8 hairColor[3] = { 115, 6, 0 };
    bool haveShirtColor = SM64AP_ReadMarioColor(rawColors, "shirt", shirtColor);

    if (haveShirtColor) {
        SM64AP_SetMarioShirtColor(shirtColor[0], shirtColor[1], shirtColor[2]);
    }
    if (SM64AP_ReadMarioColor(rawColors, "hat", color)) {
        std::copy_n(color, 3, hatColor);
        SM64AP_SetMarioHatColor(color[0], color[1], color[2]);
        SM64AP_SetMarioCapShirtColor(color[0], color[1], color[2]);
    } else if (haveShirtColor) {
        std::copy_n(shirtColor, 3, hatColor);
        SM64AP_SetMarioHatColor(shirtColor[0], shirtColor[1], shirtColor[2]);
        SM64AP_SetMarioCapShirtColor(shirtColor[0], shirtColor[1], shirtColor[2]);
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
        std::copy_n(color, 3, skinColor);
        SM64AP_SetMarioSkinColor(color[0], color[1], color[2]);
    }
    if (SM64AP_ReadMarioColor(rawColors, "hair", color)) {
        std::copy_n(color, 3, hairColor);
        SM64AP_SetMarioHairColor(color[0], color[1], color[2]);
        SM64AP_SetMarioSideburnColor(color[0], color[1], color[2]);
        SM64AP_SetMarioCapHairColor(color[0], color[1], color[2]);
    }
    SM64AP_SetMarioHeadColors(hatColor[0], hatColor[1], hatColor[2],
                              skinColor[0], skinColor[1], skinColor[2],
                              hairColor[0], hairColor[1], hairColor[2]);
}

void SM64AP_SetMoveRandoVec(int vec) {
    for (int i = 1; i < SM64AP_NUM_ABILITIES; i++) { // Start at 1, DJ bit is unnecessary
        sm64_have_abilities[i] = !std::bitset<SM64AP_NUM_ABILITIES>(vec).test(i) || sm64_have_abilities[i];
    }
}
void SM64AP_ResetItems() {
    {
        std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
        sm64_received_uncollect_coin_traps = 0;
        while (!sm64_pending_uncollect_coin_traps.empty()) {
            sm64_pending_uncollect_coin_traps.pop();
        }
        sm64_uncollect_trap_states.clear();
        while (!sm64_pending_uncollect_trap_events.empty()) {
            sm64_pending_uncollect_trap_events.pop();
        }
    }
    for (int i = 0; i < SM64AP_NUM_LOCS; i++) {
        sm64_locations[i] = false;
    }
    for (int i = 0; i < 15; i++) {
        sm64_have_cannon[i] = false;
    }
    for (int i = 0; i < NUM_PAINTING_LOCKS; i++) {
        sm64_have_painting[i] = false;
    }
    sm64_have_thi_tiny_painting = false;
    sm64_have_abilities.reset();
    sm64_have_level_moves.reset();
    sm64_have_features.reset();
    sm64_have_level_caps.reset();
    sm64_have_object_items.reset();
    sm64_have_coin_global_items.reset();
    sm64_have_coin_level_items.reset();
    sm64_have_enemy_unlock_items.reset();
    sm64_have_one_up_global_items.reset();
    sm64_have_one_up_level_items.reset();
    sm64_have_global_signs = false;
    sm64_have_level_signs.reset();
    sm64_sent_coin_checks.reset();
    sm64_sent_1up_checks.reset();
    sm64_sent_blocksanity_checks.reset();
    sm64_sent_box_checks.clear();
    sm64_have_first_floor_key = false;
    sm64_have_progressive_basement_keys = 0;
    sm64_have_progressive_upstairs_keys = 0;
    sm64_have_progressive_keys = 0;
    sm64_have_progressive_mips = 0;
    sm64_have_wing_cap_light = false;
    sm64_have_bbh = false;
    sm64_have_toads = false;
    sm64_have_castle_cannon = false;
    sm64_have_wmotr_cannon = false;
    sm64_have_yoshi = false;
    sm64_have_bitfs = false;
    sm64_have_hat = false;
    sm64_have_vcutm_entrance = false;
    sm64_have_rr_level_unlock = false;
    sm64_have_wmotr_level_unlock = false;
    sm64_1up_checks_enabled = false;
    sm64_buddy_checks_enabled = true;
    sm64_bowser_stage_1up_item_behavior = false;
    sm64_have_bowser_stage_1ups = false;
    sm64_have_bitdw_1ups = false;
    sm64_have_bitfs_1ups = false;
    sm64_hat_restore_with_animation_pending = false;
    sm64_hat_restore_without_animation_pending = false;
    sm64_have_wingcap = false;
    sm64_have_metalcap = false;
    sm64_have_vanishcap = false;
    std::fill_n(sm64_cap_length_items, 3, 0);
    for (int i = 0; i < 3; i++) {
        sm64_bowser_arena_bombs[i] = 0;
    }
    starsCollected = 0;

    SM64AP_RequestLiveObjectReconcile();
}

void SM64AP_SetReplyHandler(AP_SetReply reply) {
    if (reply.key == AP_GetPrivateServerDataPrefix() + "FinishedBowser") {
        sm64_finished_bowser_storage_received = true;
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
        sm64_moat_storage_received = true;
    } else if (reply.key == AP_GetPrivateServerDataPrefix() + "SaveFlags") {
        sm64_server_save_flags = *(int *) reply.value;
        sm64_save_flags_storage_received = true;
    } else if (reply.key == AP_GetPrivateServerDataPrefix() + "CannonFlags") {
        sm64_server_cannon_flags = *(int *) reply.value;
        sm64_cannon_flags_storage_received = true;
    } else if (reply.key == sm64_permanent_coin_ledger_key) {
        SM64AP_LoadPermanentCoins(*(std::string *) reply.value);
        sm64_permanent_coin_storage_received = true;
    } else {
        std::string coinScorePrefix = AP_GetPrivateServerDataPrefix() + "CoinHighScore;";
        if (reply.key.compare(0, coinScorePrefix.size(), coinScorePrefix) == 0) {
            int course = std::stoi(reply.key.substr(coinScorePrefix.size()));
            if (course >= 0 && course < COURSE_STAGES_COUNT) {
                if (sm64_server_coin_high_scores[course] < 0) sm64_coin_scores_storage_received++;
                int score = *(int *) reply.value;
                if (score < 0 || score > UINT8_MAX) {
                    score = 0;
                    SM64AP_SetServerInt(reply.key, "replace", score);
                }
                sm64_server_coin_high_scores[course] = score;
            }
            return;
        }
        std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
        for (auto &entry : sm64_uncollect_trap_states) {
            SM64APUncollectTrapState &state = entry.second;
            if (reply.key == state.claimKey) {
                state.winner = *(int *) reply.value;
                return;
            }
            if (reply.key == state.eventKey) {
                const std::string &raw = *(std::string *) reply.value;
                std::string::size_type pos = 0;
                int ordinal = 0;
                int slot = 0;
                int course = 0;
                int value = 0;
                u64 source = 0;
                if (raw == "{}"
                    || !SM64AP_ConsumeJsonChar(raw, pos, '{')
                    || raw.find("\"ordinal\"", pos) == std::string::npos) {
                    return;
                }

                auto parseField = [&raw, &pos](const char *name, int &field) {
                    std::string key = std::string("\"") + name + "\"";
                    pos = raw.find(key, pos);
                    if (pos == std::string::npos) {
                        return false;
                    }
                    pos += key.size();
                    return SM64AP_ConsumeJsonChar(raw, pos, ':')
                        && SM64AP_ParseJsonInt(raw, pos, field);
                };

                int sourceHigh = 0;
                int sourceLow = 0;
                pos = 0;
                if (!parseField("ordinal", ordinal)
                    || !parseField("source_high", sourceHigh)
                    || !parseField("source_low", sourceLow)
                    || !parseField("slot", slot)
                    || !parseField("course", course)
                    || !parseField("value", value)
                    || ordinal != state.ordinal
                    || slot < 0 || slot > 63
                    || course < 0 || course > COURSE_MAX
                    || value < 0 || value > 5) {
                    return;
                }
                source = (static_cast<u64>(static_cast<u32>(sourceHigh)) << 32)
                    | static_cast<u32>(sourceLow);
                sm64_pending_uncollect_trap_events.push({
                    ordinal, source, static_cast<u8>(slot),
                    static_cast<u8>(course), static_cast<u8>(value)
                });
                state.eventReceived = true;
                return;
            }
        }
    }
}

void SM64AP_GenericInit() {
    {
        std::lock_guard<std::mutex> lock(sm64_sign_hint_mutex);
        sm64_sign_hints.clear();
    }
    sm64_active_sign_dialog = -1;
    sm64_permanent_coins.clear();
    sm64_permanent_coin_updates.clear();
    sm64_pending_permanent_coins.clear();
    sm64_uncollected_coin_tombstones.clear();
    sm64_pending_permanent_coin_snapshot = false;
    sm64_permanent_coin_storage_initialized = false;
    sm64_permanent_coin_reconcile_requested = false;
    sm64_permanent_coin_ledger_key.clear();
    sm64_checked_coin_output_locations.clear();
    sm64_pending_coin_output_checks.clear();
    sm64_pending_coin_completion_checks.clear();
    sm64_finished_bowser_storage_received = false;
    sm64_moat_storage_received = false;
    sm64_permanent_coin_storage_received = false;
    sm64_save_flags_storage_received = false;
    sm64_cannon_flags_storage_received = false;
    sm64_coin_scores_storage_received = 0;
    sm64_server_save_flags = 0;
    sm64_server_cannon_flags = 0;
    std::fill_n(sm64_server_coin_high_scores, COURSE_STAGES_COUNT, -1);
    sm64_title_connection_wait_frames = 0;
    sm64_received_uncollect_coin_traps = 0;
    while (!sm64_pending_uncollect_coin_traps.empty()) {
        sm64_pending_uncollect_coin_traps.pop();
    }
    sm64_uncollect_trap_states.clear();
    while (!sm64_pending_uncollect_trap_events.empty()) {
        sm64_pending_uncollect_trap_events.pop();
    }
    AP_SetDeathLinkSupported(true);
    AP_SetItemClearCallback(&SM64AP_ResetItems);
    AP_SetLocationCheckedCallback(&SM64AP_CheckLocation);
    AP_SetItemRecvCallback(&SM64AP_RecvItem);
    AP_SetLocationInfoCallback(&SM64AP_IgnoreLocationInfo);
    AP_RegisterSetReplyCallback(&SM64AP_SetReplyHandler);
    AP_RegisterSlotDataIntCallback("FirstBowserDoorCost", &SM64AP_SetFirstBowserDoorCost);
    AP_RegisterSlotDataIntCallback("BasementDoorCost", &SM64AP_SetBasementDoorCost);
    AP_RegisterSlotDataIntCallback("SecondFloorDoorCost", &SM64AP_SetSecondFloorDoorCost);
    AP_RegisterSlotDataIntCallback("MIPS1Cost", &SM64AP_SetMIPS1Cost);
    AP_RegisterSlotDataIntCallback("MIPS2Cost", &SM64AP_SetMIPS2Cost);
    AP_RegisterSlotDataIntCallback("StarsToFinish", &SM64AP_SetStarsToFinish);
    AP_RegisterSlotDataIntCallback("CompletionType", &SM64AP_SetCompletionType);
    AP_RegisterSlotDataIntCallback("MoveRandoVec", &SM64AP_SetMoveRandoVec);
    AP_RegisterSlotDataIntCallback("GlobalCapItems", &SM64AP_SetGlobalCapDisplay);
    AP_RegisterSlotDataIntCallback("ShowGlobalCapDisplay", &SM64AP_SetGlobalCapDisplay);
    AP_RegisterSlotDataIntCallback("OneUpChecks", &SM64AP_SetOneUpChecks);
    AP_RegisterSlotDataIntCallback("WingCapLengthItemCount", &SM64AP_SetWingCapLengthItemCount);
    AP_RegisterSlotDataIntCallback("MetalCapLengthItemCount", &SM64AP_SetMetalCapLengthItemCount);
    AP_RegisterSlotDataIntCallback("VanishCapLengthItemCount", &SM64AP_SetVanishCapLengthItemCount);
    AP_RegisterSlotDataIntCallback("BuddyChecks", &SM64AP_SetBuddyChecks);
    AP_RegisterSlotDataIntCallback("BowserStage1UpBehavior", &SM64AP_SetBowserStageOneUpBehavior);
    AP_RegisterSlotDataIntCallback("EasyButterflies", &SM64AP_SetEasyButterflies);
    AP_RegisterSlotDataIntCallback("NoDespawn", &SM64AP_SetNoDespawn);
    AP_RegisterSlotDataIntCallback("MipsSkipEnabled", &SM64AP_SetMipsSkipEnabled);
    AP_RegisterSlotDataIntCallback("BowserInTheDarkWorldHits", &SM64AP_SetBowserInTheDarkWorldHits);
    AP_RegisterSlotDataIntCallback("BowserInTheFireSeaHits", &SM64AP_SetBowserInTheFireSeaHits);
    AP_RegisterSlotDataIntCallback("BowserInTheSkyHits", &SM64AP_SetBowserInTheSkyHits);
    AP_RegisterSlotDataIntCallback(
        "BowserInTheSkyStageCollapseHits", &SM64AP_SetBowserInTheSkyStageCollapseHits);
    AP_RegisterSlotDataRawCallback("AreaRando", static_cast<void (*)(std::string)>(&SM64AP_SetCourseMap));
    AP_RegisterSlotDataRawCallback(
        "SubAreaRando", static_cast<void (*)(std::string)>(&SM64AP_SetSubAreaMap));
    AP_RegisterSlotDataRawCallback("StartInventory", static_cast<void (*)(std::string)>(&SM64AP_SetStartInventory));
    AP_RegisterSlotDataIntCallback("MusicShuffleMode", &SM64AP_SetMusicShuffleMode);
    AP_RegisterSlotDataRawCallback("MusicMap", static_cast<void (*)(std::string)>(&SM64AP_SetMusicMap));
    AP_RegisterSlotDataIntCallback("SkyboxShuffleMode", &SM64AP_SetSkyboxShuffleMode);
    AP_RegisterSlotDataRawCallback("SkyboxMap", static_cast<void (*)(std::string)>(&SM64AP_SetSkyboxMap));
    AP_RegisterSlotDataRawCallback("MarioColors", &SM64AP_SetMarioColors);
    AP_RegisterSlotDataRawCallback("CoinStarRequirements", &SM64AP_SetCoinStarRequirements);
    AP_RegisterSlotDataRawCallback("SignHintData", &SM64AP_SetSignHintData);

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

int SM64AP_BoxLocationId(int id) {
    auto it = map_boxid_locid.find(id);

    if (it == map_boxid_locid.end()) {
        return 0;
    }

    return it->second;
}

static void SM64AP_SendSerializedRequest(const std::string &request) {
    if (request.empty() || request.front() != '[' || request.back() != ']') {
        fprintf(stderr, "SM64AP: Refusing to send malformed protocol request.\n");
        return;
    }
    APSend(request);
}

void SM64AP_SendByBoxID(int id) {
    int locId = SM64AP_BoxLocationId(id);

    if (locId != 0) {
        SM64AP_SendItem(locId);
    }
}

void SM64AP_SendItem(int idx) {
    if (!SM64AP_CanReportProgress()) {
        return;
    }

    sm64ap_last_location_check_id = idx;
    std::ostringstream request;
    request << "[{\"cmd\":\"LocationChecks\",\"locations\":[" << idx << "]}]";
    SM64AP_SendSerializedRequest(request.str());
}

int SM64AP_LastLocationCheckId() {
    return sm64ap_last_location_check_id;
}

// If an item exists on the stack, return it, otherwise 0
int64_t SM64AP_PopDelayedStack() {
    if(delayed_queue.empty()) return 0;
    int64_t item = delayed_queue.front();
    delayed_queue.pop();
    return item;
}

void SM64AP_FinishBowser(int i) {
    if (!SM64AP_CanReportProgress()) {
        return;
    }

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

static int SM64AP_CoinCheckCourseIndex(int courseNum) {
    if (courseNum >= COURSE_MIN && courseNum <= COURSE_STAGES_MAX) {
        return courseNum - COURSE_MIN;
    }

    switch (courseNum) {
        case COURSE_PSS:
            return 15;
        case COURSE_SA:
            return 16;
        case COURSE_WMOTR:
            return 17;
        case COURSE_TOTWC:
            return 18;
        case COURSE_VCUTM:
            return 19;
        case COURSE_COTMC:
            return 20;
        case COURSE_BITDW:
            return 21;
        case COURSE_BITFS:
            return 22;
        case COURSE_BITS:
            return 23;
        default:
            return -1;
    }
}

static int SM64AP_CoinCheckOffset(int courseIndex, int coinCount) {
    if (courseIndex < 0 || courseIndex >= SM64AP_NUM_COIN_CHECK_COURSES
        || coinCount <= 0 || coinCount > SM64AP_COIN_CHECK_MAX_COUNTS[courseIndex]) {
        return -1;
    }

    return SM64AP_COIN_CHECK_OFFSETS[courseIndex] + coinCount - 1;
}

static int SM64AP_CoinCheckLocationId(int courseIndex, int coinCount) {
    int offset = SM64AP_CoinCheckOffset(courseIndex, coinCount);

    if (offset < 0) {
        return 0;
    }

    if (offset < SM64AP_NUM_MAIN_COIN_CHECKS) {
        return SM64AP_LOCATIONID_COIN_CHECK_START + offset;
    }

    return SM64AP_LOCATIONID_SECRET_COIN_CHECK_START + (offset - SM64AP_NUM_MAIN_COIN_CHECKS);
}

static int SM64AP_CoinCheckOffsetFromLocationId(int locId) {
    int offset;

    if (locId >= SM64AP_LOCATIONID_COIN_CHECK_START && locId <= SM64AP_LOCATIONID_COIN_CHECK_END) {
        return locId - SM64AP_LOCATIONID_COIN_CHECK_START;
    }

    if (locId >= SM64AP_LOCATIONID_SECRET_COIN_CHECK_START
        && locId <= SM64AP_LOCATIONID_SECRET_COIN_CHECK_END) {
        offset = SM64AP_NUM_MAIN_COIN_CHECKS + locId - SM64AP_LOCATIONID_SECRET_COIN_CHECK_START;
        return offset < SM64AP_NUM_COIN_CHECKS ? offset : -1;
    }

    return -1;
}

bool SM64AP_CheckedLoc(int x) {
    if (x < SM64AP_ID_OFFSET || x >= SM64AP_ID_OFFSET + SM64AP_NUM_LOCS) {
        return false;
    }

    return sm64_locations[x - SM64AP_ID_OFFSET];
}

bool SM64AP_OneUpChecksEnabled() {
    return sm64_1up_checks_enabled;
}

bool SM64AP_BuddyChecksEnabled() {
    return sm64_buddy_checks_enabled;
}

static int SM64AP_OneUpCheckOffsetFromLocationId(int locId) {
    int offset = locId - SM64AP_LOCATIONID_1UP_CHECK_START;

    if (offset < 0 || offset >= SM64AP_NUM_1UP_CHECKS) {
        return -1;
    }

    return offset;
}

static bool SM64AP_IsOneUpBoxLocation(int locId) {
    for (const auto &boxLocation : map_boxid_locid) {
        if (boxLocation.second == locId) {
            return true;
        }
    }

    return false;
}

static s16 SM64AP_NormalizeOneUpLevel(s16 level) {
    return level == LEVEL_CASTLE_GROUNDS ? LEVEL_CASTLE : level;
}

static int SM64AP_OneUpCategoryForSourceType(s16 sourceType) {
    switch (sourceType) {
        case SM64AP_1UP_SOURCE_HIDDEN:
        case SM64AP_1UP_SOURCE_HIDDEN_POLE:
        case SM64AP_1UP_SOURCE_MONTY_MOLES:
            return SM64AP_ONE_UP_CATEGORY_TRIGGER;
        case SM64AP_1UP_SOURCE_BUTTERFLY:
            return SM64AP_ONE_UP_CATEGORY_BUTTERFLY;
        case SM64AP_1UP_SOURCE_BOX:
            return SM64AP_ONE_UP_CATEGORY_BLOCK;
        default:
            return SM64AP_ONE_UP_CATEGORY_FREESTANDING;
    }
}

bool SM64AP_HaveOneUpSource(s16 level, s16 sourceType) {
    // Monty Mole rewards are controlled by the Monty Moles unlock itself.
    if (sourceType == SM64AP_1UP_SOURCE_MONTY_MOLES) {
        return true;
    }

    if (!sm64_1up_checks_enabled) {
        return true;
    }

    int category = SM64AP_OneUpCategoryForSourceType(sourceType);
    level = SM64AP_NormalizeOneUpLevel(level);
    bool enabled = sm64_have_one_up_global_items[category];
    bool hasLevelItem = false;
    for (int i = 0; i < SM64AP_NUM_ONE_UP_LEVEL_ITEMS; i++) {
        if (SM64AP_ONE_UP_LEVEL_ITEMS[i].level == level
            && SM64AP_ONE_UP_LEVEL_ITEMS[i].category == category) {
            hasLevelItem = true;
            enabled = enabled || sm64_have_one_up_level_items[i];
            break;
        }
    }

    // Sources without a corresponding level item are not part of this unlock category.
    enabled = enabled || !hasLevelItem;
    return enabled;
}

static int SM64AP_LevelOneUpCategory(s16 level, s16 unlockIndex) {
    level = SM64AP_NormalizeOneUpLevel(level);
    int index = 0;
    for (int category = 0; category < SM64AP_NUM_ONE_UP_CATEGORIES; category++) {
        for (int item = 0; item < SM64AP_NUM_ONE_UP_LEVEL_ITEMS; item++) {
            if (SM64AP_ONE_UP_LEVEL_ITEMS[item].level == level
                && SM64AP_ONE_UP_LEVEL_ITEMS[item].category == category) {
                if (index == unlockIndex) {
                    return category;
                }
                index++;
                break;
            }
        }
    }
    return -1;
}

int SM64AP_LevelOneUpUnlockCount(s16 level) {
    int count = 0;
    while (SM64AP_LevelOneUpCategory(level, count) >= 0) {
        count++;
    }
    return count;
}

const char *SM64AP_LevelOneUpUnlockName(s16 level, s16 unlockIndex) {
    static constexpr const char *names[SM64AP_NUM_ONE_UP_CATEGORIES] = {
        "FREE 1UPS", "TRIGGER 1UPS", "1UP BLOCKS", "BUTTERFLIES"
    };
    int category = SM64AP_LevelOneUpCategory(level, unlockIndex);
    return category >= 0 ? names[category] : "";
}

bool SM64AP_LevelOneUpUnlockEnabled(s16 level, s16 unlockIndex) {
    int category = SM64AP_LevelOneUpCategory(level, unlockIndex);
    if (category < 0 || !sm64_1up_checks_enabled) {
        return true;
    }
    if (sm64_have_one_up_global_items[category]) {
        return true;
    }

    level = SM64AP_NormalizeOneUpLevel(level);
    for (int item = 0; item < SM64AP_NUM_ONE_UP_LEVEL_ITEMS; item++) {
        if (SM64AP_ONE_UP_LEVEL_ITEMS[item].level == level
            && SM64AP_ONE_UP_LEVEL_ITEMS[item].category == category) {
            return sm64_have_one_up_level_items[item];
        }
    }
    return true;
}

int SM64AP_BowserArenaBombCount(s16 level) {
    int arena = SM64AP_BowserArenaIndex(level);
    if (level == LEVEL_BITDW) arena = 0;
    if (level == LEVEL_BITFS) arena = 1;
    if (level == LEVEL_BITS) arena = 2;
    return arena >= 0 ? sm64_bowser_arena_bombs[arena] : -1;
}

static bool SM64AP_HaveOneUpForLocation(int locId) {
    int offset = SM64AP_OneUpCheckOffsetFromLocationId(locId);
    if (offset >= 0) {
        const SM64APOneUpSource &source = SM64AP_1UP_SOURCES[offset];
        return SM64AP_HaveOneUpSource(source.level, source.sourceType);
    }

    for (const auto &boxLocation : map_boxid_locid) {
        if (boxLocation.second == locId) {
            return SM64AP_HaveOneUpSource(
                static_cast<s16>(boxLocation.first / 10), SM64AP_1UP_SOURCE_BOX);
        }
    }

    return true;
}

int SM64AP_ResolveOneUpLocation(s16 level, s16 area, s16 sourceType, s16 sourceParam, s16 x, s16 y, s16 z) {
    for (int i = 0; i < SM64AP_NUM_1UP_CHECKS; i++) {
        const SM64APOneUpSource &source = SM64AP_1UP_SOURCES[i];
        if (source.level == level
            && source.area == area
            && source.sourceType == sourceType
            && source.sourceParam == sourceParam
            && source.x == x
            && source.y == y
            && source.z == z) {
            return SM64AP_LOCATIONID_1UP_CHECK_START + i;
        }
    }

    int bestIndex = -1;
    int bestYDelta = 0x7fffffff;
    for (int i = 0; i < SM64AP_NUM_1UP_CHECKS; i++) {
        const SM64APOneUpSource &source = SM64AP_1UP_SOURCES[i];
        if (source.level == level
            && source.area == area
            && source.sourceType == sourceType
            && source.sourceParam == sourceParam
            && source.x == x
            && source.z == z) {
            int yDelta = source.y - y;
            if (yDelta < 0) {
                yDelta = -yDelta;
            }
            if (yDelta < bestYDelta) {
                bestIndex = i;
                bestYDelta = yDelta;
            }
        }
    }

    if (bestIndex >= 0) {
        return SM64AP_LOCATIONID_1UP_CHECK_START + bestIndex;
    }

    return 0;
}

bool SM64AP_ShouldSuppressOneUp(int locId) {
    if (!SM64AP_HaveOneUpForLocation(locId)) {
        return true;
    }
    return false;
}

bool SM64AP_OneUpCollected(int locId) {
    int offset = SM64AP_OneUpCheckOffsetFromLocationId(locId);

    if (!sm64_1up_checks_enabled) {
        return false;
    }
    if (SM64AP_IsOneUpBoxLocation(locId)) {
        return sm64_sent_box_checks.count(locId) != 0 || SM64AP_CheckedLoc(locId);
    }
    return offset >= 0 && (sm64_sent_1up_checks[offset] || SM64AP_CheckedLoc(locId));
}

bool SM64AP_CollectOneUp(int locId) {
    int offset = SM64AP_OneUpCheckOffsetFromLocationId(locId);

    if (SM64AP_IsOneUpBoxLocation(locId)) {
        if (!sm64_1up_checks_enabled) {
            return false;
        }
        if (SM64AP_OneUpCollected(locId)) {
            return false;
        }
        if (SM64AP_CanReportProgress()
            && sm64_sent_box_checks.count(locId) == 0
            && !SM64AP_CheckedLoc(locId)) {
            sm64_sent_box_checks.insert(locId);
            SM64AP_SendItem(locId);
        }
        return true;
    }

    if (!SM64AP_CanReportProgress() || !sm64_1up_checks_enabled || offset < 0) {
        return false;
    }

    if (SM64AP_OneUpCollected(locId)) {
        return false;
    }

    if (!sm64_sent_1up_checks[offset] && !SM64AP_CheckedLoc(locId)) {
        sm64_sent_1up_checks[offset] = true;
        SM64AP_SendItem(locId);
    }

    return true;
}

static int SM64AP_BlocksanityOffsetFromLocationId(int locId) {
    int offset = locId - SM64AP_LOCATIONID_BLOCKSANITY_START;

    if (offset < 0 || offset >= SM64AP_NUM_BLOCKSANITY_CHECKS) {
        return -1;
    }

    return offset;
}

static int SM64AP_ResolveBlocksanityLocation(s16 level, s16 area, s32 behParams, s16 x, s16 y, s16 z) {
    for (int i = 0; i < SM64AP_NUM_BLOCKSANITY_CHECKS; i++) {
        const SM64APBlocksanitySource &source = SM64AP_BLOCKSANITY_SOURCES[i];
        if (source.level == level
            && source.area == area
            && source.behParams == behParams
            && source.x == x
            && source.y == y
            && source.z == z) {
            return SM64AP_LOCATIONID_BLOCKSANITY_START + i;
        }
    }

    return 0;
}

bool SM64AP_BlocksanityChecked(s16 level, s16 area, s32 behParams, s16 x, s16 y, s16 z) {
    int locId = SM64AP_ResolveBlocksanityLocation(level, area, behParams, x, y, z);
    int offset = SM64AP_BlocksanityOffsetFromLocationId(locId);
    return offset >= 0 && (sm64_sent_blocksanity_checks[offset] || SM64AP_CheckedLoc(locId));
}

static u64 SM64AP_ExpectedPermanentCoinMask(u64 sourceId) {
    static std::map<u64, u64> expectedMasks;
    static bool initialized = false;

    if (!initialized) {
        for (const auto &entry : sm64_coin_output_catalog) {
            if (entry.physicalSource != 0 && entry.physicalSlot < 64) {
                expectedMasks[entry.physicalSource] |= 1ULL << entry.physicalSlot;
            }
        }
        for (const auto &entry : sm64_coin_completion_catalog) {
            if (entry.physicalSource == 0) {
                continue;
            }
            for (int slot = 0; slot < entry.requiredOutputCount && slot < 64; slot++) {
                expectedMasks[entry.physicalSource] |= 1ULL << slot;
            }
        }
        initialized = true;
    }

    auto expected = expectedMasks.find(sourceId);
    return expected != expectedMasks.end() ? expected->second : 0;
}

static bool SM64AP_PermanentCoinSourceExhausted(u64 sourceId) {
    u64 expectedMask = SM64AP_ExpectedPermanentCoinMask(sourceId);
    if (expectedMask == 0) {
        return false;
    }

    u64 collectedMask = 0;
    for (int slot = 0; slot < 64; slot++) {
        if ((expectedMask & (1ULL << slot)) != 0
            && sm64_permanent_coins.count(std::make_pair(sourceId, static_cast<u8>(slot))) != 0) {
            collectedMask |= 1ULL << slot;
        }
    }
    return collectedMask == expectedMask;
}

static bool SM64AP_IsRenderedCoin(const void *behavior) {
    return behavior_is(behavior, bhvYellowCoin)
        || behavior_is(behavior, bhvOneCoin)
        || behavior_is(behavior, bhvRedCoin)
        || behavior_is(behavior, bhvMovingBlueCoin)
        || behavior_is(behavior, bhvBlueCoinSliding)
        || behavior_is(behavior, bhvHiddenBlueCoin);
}

static bool SM64AP_IsOneUpBehavior(const void *behavior) {
    return behavior_is(behavior, bhv1Up)
        || behavior_is(behavior, bhv1upSliding)
        || behavior_is(behavior, bhv1upJumpOnApproach)
        || behavior_is(behavior, bhvHidden1up)
        || behavior_is(behavior, bhvHidden1upInPole)
        || behavior_is(behavior, bhv1upWalking)
        || behavior_is(behavior, bhv1upRunningAway);
}

static int SM64AP_StarProducerIndex(struct Object *obj) {
    const void *behavior = obj->behavior;
    u8 starIndex = beh_param_star(obj->oBehParams);

    if (behavior_is(behavior, bhvKingBobomb)
        || behavior_is(behavior, bhvWhompKingBoss)
        || behavior_is(behavior, bhvFirePiranhaPlant)
        || behavior_is(behavior, bhvBigBully)
        || behavior_is(behavior, bhvBigBullyWithMinions)
        || behavior_is(behavior, bhvBigChillBully)
        || behavior_is(behavior, bhvTuxiesMother)
        || behavior_is(behavior, bhvEyerokBoss)
        || behavior_is(behavior, bhvSnowmansHead)
        || behavior_is(behavior, bhvWigglerHead)
        || behavior_is(behavior, bhvRacingPenguin)
        || behavior_is(behavior, bhvUkikiCage)
        || behavior_is(behavior, bhvMantaRay)
        || behavior_is(behavior, bhvGhostHuntBigBoo)
        || behavior_is(behavior, bhvMerryGoRoundBigBoo)
        || behavior_is(behavior, bhvBalconyBigBoo)
        || behavior_is(behavior, bhvTreasureChests)
        || behavior_is(behavior, bhvTreasureChestsJrb)
        || behavior_is(behavior, bhvTreasureChestsShip)) {
        return starIndex;
    }

    if ((behavior_is(behavior, bhvUnagi) && beh_param_second_byte(obj->oBehParams) == 1)
        || (behavior_is(behavior, bhvKlepto) && beh_param_second_byte(obj->oBehParams) == 1)
        || (behavior_is(behavior, bhvMrI) && beh_param_second_byte(obj->oBehParams) != 0)) {
        return starIndex;
    }

    if (behavior_is(behavior, bhvKoopa) && beh_param_second_byte(obj->oBehParams) >= 2) {
        return starIndex;
    }

    return -1;
}

static bool SM64AP_SpecialStarProducerExhausted(struct Object *obj, bool *isProducer) {
    *isProducer = true;
    if (behavior_is(obj->behavior, bhvYoshi)) {
        return SM64AP_CheckedLoc(SM64AP_LOCATIONID_YOSHI);
    }
    if (behavior_is(obj->behavior, bhvMips)) {
        return SM64AP_CheckedLoc(SM64AP_LOCATIONID_MIPS1)
            && SM64AP_CheckedLoc(SM64AP_LOCATIONID_MIPS2);
    }
    if (behavior_is(obj->behavior, bhvToadMessage)) {
        switch (obj->oBehParams >> 24) {
            case DIALOG_082:
            case DIALOG_154:
                return SM64AP_CheckedLoc(SM64AP_LOCATIONID_BASEMENTTOAD);
            case DIALOG_076:
            case DIALOG_155:
                return SM64AP_CheckedLoc(SM64AP_LOCATIONID_SECONDFLOORTOAD);
            case DIALOG_083:
            case DIALOG_156:
                return SM64AP_CheckedLoc(SM64AP_LOCATIONID_THIRDFLOORTOAD);
        }
    }
    *isProducer = false;
    return false;
}

static bool SM64AP_BlockContentsExhausted(struct Object *obj) {
    int content = obj->oBehParams2ndByte;
    if ((obj->oBehParams >> 16) == 0x1404) {
        return SM64AP_OneUpCollected(SM64AP_BoxLocationId(obj->oBehParams & 0xFFFF));
    }
    if (content >= 4 && content <= 6) {
        return SM64AP_PermanentCoinSourceExhausted(obj->apCoinSourceId);
    }
    if (content == 7 || content == 9) {
        int locId = SM64AP_ResolveOneUpLocation(
            gCurrLevelNum, gCurrAreaIndex, SM64AP_1UP_SOURCE_BOX, content,
            static_cast<s16>(obj->oHomeX), static_cast<s16>(obj->oHomeY),
            static_cast<s16>(obj->oHomeZ));
        return SM64AP_OneUpCollected(locId);
    }
    if (content == 8 || (content >= 10 && content <= 14)) {
        int starIndex = content == 8 ? 0 : content - 9;
        return SM64AP_CollectedCourseStar(gCurrCourseNum - COURSE_MIN, starIndex);
    }
    return true;
}

static bool SM64AP_IsValidObjectPointer(const struct Object *obj) {
    uintptr_t address = reinterpret_cast<uintptr_t>(obj);
    uintptr_t poolStart = reinterpret_cast<uintptr_t>(&gObjectPool[0]);
    uintptr_t poolEnd = reinterpret_cast<uintptr_t>(&gObjectPool[OBJECT_POOL_CAPACITY]);
    return address >= poolStart && address < poolEnd
        && (address - poolStart) % sizeof(struct Object) == 0;
}

static struct Object *SM64AP_ValidObjectParent(struct Object *obj) {
    struct Object *parent = obj->parentObj;
    if (parent == obj || !SM64AP_IsValidObjectPointer(parent)
        || !(parent->activeFlags & ACTIVE_FLAG_ACTIVE)) {
        return nullptr;
    }
    return parent;
}

static u8 SM64AP_ComputeObjectVisualState(struct Object *obj) {
    bool hasExhaustibleOutput = false;
    bool exhausted = true;
    int enemySource = SM64AP_EnemyCoinSource(obj->behavior);
    struct Object *producer = obj;

    for (int depth = 0; depth < 4 && producer != nullptr; depth++) {
        int ancestorEnemySource = SM64AP_EnemyCoinSource(producer->behavior);
        if (ancestorEnemySource == SM64AP_COIN_SOURCE_BOO
            || ancestorEnemySource == SM64AP_COIN_SOURCE_BIG_BOO) {
            enemySource = ancestorEnemySource;
        }
        if (SM64AP_StarProducerIndex(producer) >= 0) {
            break;
        }
        bool specialProducer = false;
        SM64AP_SpecialStarProducerExhausted(producer, &specialProducer);
        if (specialProducer) {
            break;
        }
        producer = SM64AP_ValidObjectParent(producer);
    }

    if (behavior_is(obj->behavior, bhvExclamationBox)) {
        int locId = SM64AP_ResolveBlocksanityLocation(
            gCurrLevelNum, gCurrAreaIndex, obj->oBehParams,
            static_cast<s16>(obj->oHomeX), static_cast<s16>(obj->oHomeY),
            static_cast<s16>(obj->oHomeZ));
        if (locId != 0) {
            hasExhaustibleOutput = true;
            exhausted = SM64AP_BlocksanityChecked(
                gCurrLevelNum, gCurrAreaIndex, obj->oBehParams,
                static_cast<s16>(obj->oHomeX), static_cast<s16>(obj->oHomeY),
                static_cast<s16>(obj->oHomeZ))
                && SM64AP_BlockContentsExhausted(obj);
        }
    } else if (SM64AP_IsOneUpBehavior(obj->behavior)) {
        hasExhaustibleOutput = obj->o1UpApLocationId != 0;
        exhausted = hasExhaustibleOutput && SM64AP_OneUpCollected(obj->o1UpApLocationId);
    } else if (!SM64AP_IsRenderedCoin(obj->behavior) && obj->apCoinSourceId != 0) {
        u64 expectedMask = SM64AP_ExpectedPermanentCoinMask(obj->apCoinSourceId);
        if (expectedMask != 0) {
            hasExhaustibleOutput = true;
            exhausted = SM64AP_PermanentCoinSourceExhausted(obj->apCoinSourceId);
        }
    }

    int starIndex = producer != nullptr ? SM64AP_StarProducerIndex(producer) : -1;
    if (starIndex >= 0) {
        hasExhaustibleOutput = true;
        exhausted = exhausted
            && SM64AP_CollectedCourseStar(gCurrCourseNum - COURSE_MIN, starIndex);
    } else {
        bool specialProducer = false;
        bool specialExhausted = producer != nullptr
            && SM64AP_SpecialStarProducerExhausted(producer, &specialProducer);
        if (specialProducer) {
            hasExhaustibleOutput = true;
            exhausted = exhausted && specialExhausted;
        }
    }

    if (hasExhaustibleOutput && exhausted) {
        if (enemySource == SM64AP_COIN_SOURCE_BOO || enemySource == SM64AP_COIN_SOURCE_BIG_BOO) {
            return SM64AP_VISUAL_EXHAUSTED_DARK;
        }
        return SM64AP_VISUAL_EXHAUSTED;
    }
    return SM64AP_VISUAL_NORMAL;
}

u8 SM64AP_ObjectVisualState(struct Object *obj) {
    if (obj == nullptr) {
        return SM64AP_VISUAL_NORMAL;
    }
    if (obj->apVisualStateFrame != gGlobalTimer) {
        obj->apVisualStateFrame = gGlobalTimer;
        obj->apVisualState = SM64AP_ComputeObjectVisualState(obj);
        struct Object *parent = SM64AP_ValidObjectParent(obj);
        if (obj->apVisualState == SM64AP_VISUAL_NORMAL && parent != nullptr) {
            obj->apVisualState = SM64AP_ObjectVisualState(parent);
        }
    }
    return obj->apVisualState;
}

void SM64AP_SendBlocksanityCheck(s16 level, s16 area, s32 behParams, s16 x, s16 y, s16 z) {
    if (!SM64AP_CanReportProgress()) {
        return;
    }

    int locId = SM64AP_ResolveBlocksanityLocation(level, area, behParams, x, y, z);
    int offset = SM64AP_BlocksanityOffsetFromLocationId(locId);
    if (offset < 0 || sm64_sent_blocksanity_checks[offset] || SM64AP_CheckedLoc(locId)) {
        return;
    }

    sm64_sent_blocksanity_checks[offset] = true;
    SM64AP_SendItem(locId);
}

void SM64AP_CheckCoinCount(int courseNum, int coinCount) {
    if (!SM64AP_CanReportProgress()) {
        return;
    }

    int courseIndex = SM64AP_CoinCheckCourseIndex(courseNum);

    if (courseIndex < 0) {
        return;
    }

    if (coinCount > SM64AP_COIN_CHECK_MAX_COUNTS[courseIndex]) {
        coinCount = SM64AP_COIN_CHECK_MAX_COUNTS[courseIndex];
    }

    for (int count = 1; count <= coinCount; count++) {
        int locId = SM64AP_CoinCheckLocationId(courseIndex, count);
        int offset = SM64AP_CoinCheckOffsetFromLocationId(locId);

        if (offset >= 0 && !sm64_sent_coin_checks[offset] && !SM64AP_CheckedLoc(locId)) {
            sm64_sent_coin_checks[offset] = true;
            SM64AP_SendItem(locId);
        }
    }
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

void SM64AP_SelectSkybox(s16 level, s16 area, s16 vanillaSkybox) {
    int selectedSkybox = vanillaSkybox;
    if (sm64_skybox_shuffle_mode == SM64AP_SKYBOX_SHUFFLE_MAP) {
        auto entry = map_skybox.find(SM64AP_ENTRANCE_ID(level, area));
        if (entry != map_skybox.end() && entry->second >= 0 && entry->second < 10) {
            selectedSkybox = entry->second;
        }
    } else if (sm64_skybox_shuffle_mode == SM64AP_SKYBOX_SHUFFLE_RANDOM_ON_LOAD) {
        selectedSkybox = random_u16() % 10;
    }

    sm64_loaded_skybox = selectedSkybox;
}

s16 SM64AP_ResolveSkyboxBackground(s16 vanillaSkybox) {
    return sm64_loaded_skybox >= 0 ? static_cast<s16>(sm64_loaded_skybox) : vanillaSkybox;
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

int SM64AP_MipsSpawnTier(void) {
    if (!SM64AP_HaveProgressiveMips(1)) {
        return -1;
    }
    if (!SM64AP_CheckedLoc(SM64AP_LOCATIONID_MIPS1)) {
        return 0;
    }
    if (SM64AP_HaveProgressiveMips(2) && !SM64AP_CheckedLoc(SM64AP_LOCATIONID_MIPS2)) {
        return 1;
    }
    if (sm64_mips_skip_enabled) {
        return SM64AP_HaveProgressiveMips(2) ? 1 : 0;
    }
    return -1;
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

bool SM64AP_HaveWmotrCannon() {
    if (!sm64_buddy_checks_enabled) {
        return save_file_get_cannon_flags(gCurrSaveFileNum - 1, COURSE_WMOTR - 1) != 0;
    }

    return sm64_have_wmotr_cannon;
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
    if (courseIdx >= 0 && courseIdx < 15) {
        if (!sm64_buddy_checks_enabled) {
            return save_file_get_cannon_flags(gCurrSaveFileNum - 1, courseIdx) != 0;
        }

        return sm64_have_cannon[courseIdx];
    }
    return false;
}

bool SM64AP_HavePainting(int courseIdx) {
    switch(courseIdx) {
        case 1:  // BOB painting is always unlocked
        case 5:  // BBH doesn't have a painting
            return true;
        case COURSE_RR:
            return sm64_have_rr_level_unlock;
        case COURSE_WMOTR:
            return sm64_have_wmotr_level_unlock;
        default:
            // courses are 1-indexed, the items are 0-indexed
            return sm64_have_painting[courseIdx-1];
    }
}

bool SM64AP_HavePaintingForArea(int courseIdx, int areaIdx) {
    if (courseIdx == COURSE_THI && areaIdx == 2) {
        return sm64_have_thi_tiny_painting;
    }

    return SM64AP_HavePainting(courseIdx);
}

bool SM64AP_EasyButterflies() {
    return sm64_easy_butterflies;
}

bool SM64AP_NoDespawn() {
    return sm64_no_despawn;
}

bool SM64AP_PermanentCoinCollection() {
    return true;
}

static u64 SM64AP_PermanentCoinHashValue(u64 hash, u64 value) {
    for (int byte = 0; byte < 8; byte++) {
        hash ^= (value >> (byte * 8)) & 0xFF;
        hash *= 1099511628211ULL;
    }
    return hash;
}

void SM64AP_AssignPermanentCoinSource(
    struct Object *object, s16 level, s16 area, s16 model,
    s16 x, s16 y, s16 z, u32 behaviorParams, const void *behavior) {
    u64 hash = 1469598103934665603ULL;
    int sourceKind;
    if (object == nullptr) {
        return;
    }

    sourceKind = SM64AP_PlacedCoinSource(behaviorParams, behavior);
    if (sourceKind < 0) {
        sourceKind = SM64AP_CoinObjectSource(behaviorParams, behavior);
    }
    if (sourceKind < 0) {
        sourceKind = SM64AP_EnemyCoinSource(behavior);
    }

    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u16>(level));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u16>(area));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u32>(sourceKind + 1));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u16>(model));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u16>(x));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u16>(y));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u16>(z));
    hash = SM64AP_PermanentCoinHashValue(hash, behaviorParams);
    if (hash == 0) {
        hash = 1;
    }

    object->apCoinSourceId = hash;
    object->apCoinCourse = gCurrCourseNum;
    object->apCoinSlot = 0;
    object->apCoinValue = 0;
    object->apCoinSlotCount = 0;
    object->apCoinSourceKind = 1;
    object->apCoinSourceX = x;
    object->apCoinSourceY = y;
    object->apCoinSourceZ = z;
}

static bool SM64AP_PermanentCoinSlotCollected(u64 source, u8 slot) {
    return sm64_permanent_coins.count(std::make_pair(source, slot)) != 0;
}

static bool SM64AP_PermanentCoinSlotActive(u64 source, u8 slot) {
    for (int index = 0; index < OBJECT_POOL_CAPACITY; index++) {
        struct Object *object = &gObjectPool[index];
        if ((object->activeFlags & ACTIVE_FLAG_ACTIVE)
            && object->apCoinValue > 0
            && object->apCoinSourceId == source
            && object->apCoinSlot == slot) {
            return true;
        }
    }
    return false;
}

static void SM64AP_NormalizeInheritedCoinSource(struct Object *source) {
    if (source == nullptr || source->apCoinSourceKind != 2) {
        return;
    }
    u64 hash = source->apCoinSourceId;
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<s32>(source->oHomeX));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<s32>(source->oHomeY));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<s32>(source->oHomeZ));
    hash = SM64AP_PermanentCoinHashValue(hash, source->oBehParams);
    source->apCoinSourceId = hash != 0 ? hash : 1;
    source->apCoinSourceKind = 1;
}

void SM64AP_FinalizeRelativePermanentCoinSource(
    struct Object *source, s16 relativeX, s16 relativeY, s16 relativeZ) {
    if (source == nullptr || source->apCoinSourceKind != 2) {
        return;
    }
    source->apCoinSourceX += relativeX;
    source->apCoinSourceY += relativeY;
    source->apCoinSourceZ += relativeZ;
    u64 hash = source->apCoinSourceId;
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<s32>(source->apCoinSourceX));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<s32>(source->apCoinSourceY));
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<s32>(source->apCoinSourceZ));
    hash = SM64AP_PermanentCoinHashValue(hash, source->oBehParams);
    source->apCoinSourceId = hash != 0 ? hash : 1;
    source->apCoinSourceKind = 1;
}

void SM64AP_PreserveInheritedPermanentCoinSource(struct Object *source) {
    if (source != nullptr && source->apCoinSourceKind == 2) {
        source->apCoinSourceKind = 1;
    }
}

void SM64AP_DistinguishInheritedPermanentCoinSource(struct Object *source, int ordinal) {
    if (source == nullptr || source->apCoinSourceKind != 2) {
        return;
    }

    u64 hash = source->apCoinSourceId;
    hash = SM64AP_PermanentCoinHashValue(hash, 0x4D475242ULL);
    hash = SM64AP_PermanentCoinHashValue(hash, static_cast<u32>(ordinal));
    source->apCoinSourceId = hash != 0 ? hash : 1;
    source->apCoinSourceKind = 1;
}

void SM64AP_AssignPermanentCoinSlot(struct Object *coin, struct Object *source, int slot, int value) {
    if (coin == nullptr || source == nullptr || source->apCoinSourceId == 0) {
        return;
    }
    coin->apCoinSourceId = source->apCoinSourceId;
    coin->apCoinCourse = source->apCoinCourse;
    coin->apCoinSlot = slot;
    coin->apCoinValue = value;
    coin->apCoinSourceKind = 3;
    if (source->apCoinSlotCount <= slot) {
        source->apCoinSlotCount = slot + 1;
    }
}

bool SM64AP_AssignPermanentCoinOutput(
    struct Object *source, struct Object *coin, int value, int slotCount) {
    if (source == nullptr || coin == nullptr || source->apCoinSourceId == 0) {
        return true;
    }

    SM64AP_NormalizeInheritedCoinSource(source);

    if (slotCount < 1) {
        slotCount = 1;
    }
    if (slotCount > 64) {
        slotCount = 64;
    }
    if (source->apCoinSlotCount < slotCount) {
        source->apCoinSlotCount = slotCount;
    }

    // Emit every uncollected output before using collected slots for spent,
    // restorative coin objects.
    for (int slot = 0; slot < slotCount; slot++) {
        if (!SM64AP_PermanentCoinSlotCollected(source->apCoinSourceId, slot)
            && !SM64AP_PermanentCoinSlotActive(source->apCoinSourceId, slot)) {
            SM64AP_AssignPermanentCoinSlot(coin, source, slot, value);
            return true;
        }
    }
    for (int slot = 0; slot < slotCount; slot++) {
        if (!SM64AP_PermanentCoinSlotActive(source->apCoinSourceId, slot)) {
            SM64AP_AssignPermanentCoinSlot(coin, source, slot, value);
            return true;
        }
    }
    return false;
}

bool SM64AP_AssignPermanentAggregateCoinOutput(
    struct Object *source, struct Object *coin, int slotCount) {
    if (source == nullptr || coin == nullptr || source->apCoinSourceId == 0) {
        return true;
    }

    SM64AP_NormalizeInheritedCoinSource(source);
    if (slotCount < 1 || slotCount > 64) {
        return false;
    }

    for (int slot = 0; slot < slotCount; slot++) {
        if (SM64AP_PermanentCoinSlotCollected(source->apCoinSourceId, slot)
            || SM64AP_PermanentCoinSlotActive(source->apCoinSourceId, slot)) {
            return false;
        }
    }

    SM64AP_AssignPermanentCoinSlot(coin, source, 0, slotCount);
    coin->apCoinSlotCount = slotCount;
    return true;
}

bool SM64AP_MarkSpentPermanentCoin(struct Object *coin, int value) {
    if (coin == nullptr || coin->apCoinSourceId == 0) {
        return false;
    }
    coin->apCoinValue = value;
    coin->apCoinSpent = SM64AP_PermanentCoinSlotCollected(coin->apCoinSourceId, coin->apCoinSlot);
    if (coin->apCoinSpent) {
        coin->header.gfx.sharedChild = gLoadedGraphNodes[
            value >= 5 ? MODEL_SPENT_BLUE_COIN : MODEL_SPENT_COIN];
    }
    return coin->apCoinSpent;
}

static std::string SM64AP_PermanentCoinKey(u64 source, u8 slot) {
    return std::to_string(source) + ":" + std::to_string(static_cast<int>(slot));
}

static std::string SM64AP_SerializePermanentCoinEntries(
    const std::map<std::pair<u64, u8>, SM64APPermanentCoinRecord> &entries) {
    std::ostringstream output;
    output << '{';
    bool first = true;
    for (const auto &entry : entries) {
        if (!first) {
            output << ',';
        }
        first = false;
        output << '"' << SM64AP_PermanentCoinKey(entry.first.first, entry.first.second) << "\":["
               << static_cast<int>(entry.second.course) << ','
               << static_cast<int>(entry.second.value) << ']';
    }
    output << '}';
    return output.str();
}

static bool SM64AP_ParsePermanentCoinKey(
    const std::string &text, std::string::size_type &pos, u64 &source, int &slot) {
    SM64AP_SkipJsonWhitespace(text, pos);
    if (pos >= text.size() || text[pos++] != '"') {
        return false;
    }
    std::string::size_type sourceStart = pos;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        pos++;
    }
    if (sourceStart == pos || pos >= text.size() || text[pos++] != ':') {
        return false;
    }
    std::string::size_type slotStart = pos;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        pos++;
    }
    if (slotStart == pos || pos >= text.size() || text[pos++] != '"') {
        return false;
    }

    try {
        source = std::stoull(text.substr(sourceStart, slotStart - sourceStart - 1));
        slot = std::stoi(text.substr(slotStart, pos - slotStart - 1));
    } catch (...) {
        return false;
    }
    return true;
}

static void SM64AP_SendCoinOutputCheck(const std::pair<u64, u8> &physicalKey) {
    bool retry = false;
    for (const auto &entry : sm64_coin_output_catalog) {
        if (entry.physicalSource != physicalKey.first || entry.physicalSlot != physicalKey.second) {
            continue;
        }
        if (sm64_checked_coin_output_locations.count(entry.locationId) != 0) {
            continue;
        }
        if (!SM64AP_CanReportProgress()) {
            retry = true;
            continue;
        }
        sm64_checked_coin_output_locations.insert(entry.locationId);
        SM64AP_SendItem(entry.locationId);
    }
    if (retry) {
        sm64_pending_coin_output_checks.insert(physicalKey);
    } else {
        sm64_pending_coin_output_checks.erase(physicalKey);
    }
}

static void SM64AP_SendCoinCompletionCheck(u64 physicalSource) {
    bool retry = false;
    for (const auto &entry : sm64_coin_completion_catalog) {
        if (entry.physicalSource != physicalSource) {
            continue;
        }
        bool complete = true;
        for (int slot = 0; slot < entry.requiredOutputCount; slot++) {
            if (!SM64AP_PermanentCoinSlotCollected(physicalSource, static_cast<u8>(slot))) {
                complete = false;
                break;
            }
        }
        if (!complete || sm64_checked_coin_output_locations.count(entry.locationId) != 0) {
            continue;
        }
        if (!SM64AP_CanReportProgress()) {
            retry = true;
            continue;
        }
        sm64_checked_coin_output_locations.insert(entry.locationId);
        SM64AP_SendItem(entry.locationId);
    }
    if (retry) {
        sm64_pending_coin_completion_checks.insert(physicalSource);
    } else {
        sm64_pending_coin_completion_checks.erase(physicalSource);
    }
}

static void SM64AP_LoadPermanentCoins(const std::string &rawLedger) {
    std::map<std::pair<u64, u8>, SM64APPermanentCoinRecord> parsed;
    std::string::size_type pos = 0;
    if (!SM64AP_ConsumeJsonChar(rawLedger, pos, '{')) {
        return;
    }

    while (pos < rawLedger.size()) {
        SM64AP_SkipJsonWhitespace(rawLedger, pos);
        if (pos < rawLedger.size() && rawLedger[pos] == '}') {
            break;
        }

        u64 source = 0;
        int slot = 0;
        int course = 0;
        int value = 0;
        if (!SM64AP_ParsePermanentCoinKey(rawLedger, pos, source, slot)
            || !SM64AP_ConsumeJsonChar(rawLedger, pos, ':')
            || !SM64AP_ConsumeJsonChar(rawLedger, pos, '[')
            || !SM64AP_ParseJsonInt(rawLedger, pos, course)
            || !SM64AP_ConsumeJsonChar(rawLedger, pos, ',')
            || !SM64AP_ParseJsonInt(rawLedger, pos, value)
            || !SM64AP_ConsumeJsonChar(rawLedger, pos, ']')) {
            return;
        }
        if (source != 0 && slot >= 0 && slot < 64
            && course > COURSE_NONE && course <= COURSE_MAX
            && value > 0 && value <= 5
            && (SM64AP_ExpectedPermanentCoinMask(source) & (1ULL << slot)) != 0) {
            parsed[std::make_pair(source, static_cast<u8>(slot))] = {
                static_cast<u8>(course), static_cast<u8>(value)
            };
        }

        SM64AP_SkipJsonWhitespace(rawLedger, pos);
        if (pos < rawLedger.size() && rawLedger[pos] == ',') {
            pos++;
        } else if (pos < rawLedger.size() && rawLedger[pos] != '}') {
            return;
        }
    }

    std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
    sm64_pending_permanent_coins = std::move(parsed);
    sm64_pending_permanent_coin_snapshot = true;
}

static void SM64AP_InitializeServerStorage() {
    if (sm64_permanent_coin_storage_initialized
        || AP_GetConnectionStatus() != AP_ConnectionStatus::Authenticated) {
        return;
    }

    std::string prefix = AP_GetPrivateServerDataPrefix();
    std::string finishedBowserKey = prefix + "FinishedBowser";
    std::string moatDrainedKey = prefix + "MoatDrained";
    std::string saveFlagsKey = prefix + "SaveFlags";
    std::string cannonFlagsKey = prefix + "CannonFlags";
    std::map<std::string, AP_DataType> integerStorageKeys = {
        { finishedBowserKey, AP_DataType::Int },
        { moatDrainedKey, AP_DataType::Int },
        { saveFlagsKey, AP_DataType::Int },
        { cannonFlagsKey, AP_DataType::Int },
    };

    for (int course = 0; course < COURSE_STAGES_COUNT; course++) {
        std::string key = prefix + "CoinHighScore;" + std::to_string(course);
        integerStorageKeys[key] = AP_DataType::Int;
    }
    AP_SetNotify(integerStorageKeys, false);
    for (const auto &entry : integerStorageKeys) {
        SM64AP_SetServerInt(entry.first, "default", 0);
    }

    sm64_permanent_coin_ledger_key = prefix + "PermanentCoins";
    AP_SetNotify(sm64_permanent_coin_ledger_key, AP_DataType::Raw, true);
    sm64_permanent_coin_storage_initialized = true;
}

bool SM64AP_ReadyToStart() {
    SM64AP_InitializeServerStorage();
    bool ready = AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated
        && sm64_permanent_coin_storage_initialized
        && sm64_finished_bowser_storage_received
        && sm64_moat_storage_received
        && sm64_save_flags_storage_received
        && sm64_cannon_flags_storage_received
        && sm64_coin_scores_storage_received == COURSE_STAGES_COUNT
        && sm64_permanent_coin_storage_received;
    if (ready) {
        sm64_title_connection_wait_frames = 0;
    }
    return ready;
}

void SM64AP_PrintTitleConnectionStatus() {
    if (AP_GetConnectionStatus() != AP_ConnectionStatus::ConnectionRefused
        && ++sm64_title_connection_wait_frames < 15) {
        return;
    }

    switch (AP_GetConnectionStatus()) {
        case AP_ConnectionStatus::ConnectionRefused:
            print_text_centered(SCREEN_WIDTH / 2, 38, "CONNECTION REFUSED");
            print_text_centered(SCREEN_WIDTH / 2, 20, "CHECK CONNECTION SETTINGS");
            break;
        case AP_ConnectionStatus::Authenticated:
            print_text_centered(SCREEN_WIDTH / 2, 20, "LOADING SERVER DATA");
            break;
        default:
            print_text_centered(SCREEN_WIDTH / 2, 20, "CONNECTING TO SERVER");
            break;
    }
}

bool SM64AP_ConsumePermanentCoinReconcileRequest() {
    std::map<std::pair<u64, u8>, SM64APPermanentCoinRecord> pending;
    {
        std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
        if (sm64_pending_permanent_coin_snapshot) {
            pending = std::move(sm64_pending_permanent_coins);
            sm64_pending_permanent_coins.clear();
            sm64_pending_permanent_coin_snapshot = false;
        }
    }

    for (const auto &entry : pending) {
        if (sm64_uncollected_coin_tombstones.count(entry.first) == 0
            && sm64_permanent_coins.count(entry.first) == 0) {
            sm64_permanent_coins[entry.first] = entry.second;
            SM64AP_SendCoinOutputCheck(entry.first);
            SM64AP_SendCoinCompletionCheck(entry.first.first);
            sm64_permanent_coin_reconcile_requested = true;
        }
    }

    bool requested = sm64_permanent_coin_reconcile_requested;
    sm64_permanent_coin_reconcile_requested = false;
    return requested;
}

static void SM64AP_RecordPermanentCoin(
    const std::pair<u64, u8> &key, u8 course, u8 value) {
    SM64APPermanentCoinRecord record = { course, value };
    sm64_permanent_coins[key] = record;
    sm64_permanent_coin_updates[key] = record;
    sm64_uncollected_coin_tombstones.erase(key);
    SM64AP_SendCoinOutputCheck(key);
    SM64AP_SendCoinCompletionCheck(key.first);
}

bool SM64AP_CollectPermanentCoin(struct Object *coin, int value) {
    if (coin == nullptr || coin->apCoinSourceId == 0) {
        return true;
    }

    if (coin->apCoinSlotCount > 1 && value == coin->apCoinSlotCount) {
        for (int slot = 0; slot < coin->apCoinSlotCount; slot++) {
            if (SM64AP_PermanentCoinSlotCollected(coin->apCoinSourceId, slot)) {
                return false;
            }
        }
        for (int slot = 0; slot < coin->apCoinSlotCount; slot++) {
            SM64AP_RecordPermanentCoin(
                std::make_pair(coin->apCoinSourceId, static_cast<u8>(slot)),
                coin->apCoinCourse != 0 ? coin->apCoinCourse : static_cast<u8>(gCurrCourseNum),
                1);
        }
        return true;
    }

    std::pair<u64, u8> key = std::make_pair(coin->apCoinSourceId, coin->apCoinSlot);
    if (sm64_permanent_coins.count(key) != 0) {
        return false;
    }

    SM64AP_RecordPermanentCoin(
        key,
        coin->apCoinCourse != 0 ? coin->apCoinCourse : static_cast<u8>(gCurrCourseNum),
        static_cast<u8>(value));
    coin->apCoinValue = value;
    return true;
}

int SM64AP_CollectPermanentCoinOutputs(
    struct Object *source, int value, int slotCount, int requestedSlots) {
    if (source == nullptr || source->apCoinSourceId == 0) {
        return value * requestedSlots;
    }

    SM64AP_NormalizeInheritedCoinSource(source);
    int collectedValue = 0;
    for (int slot = 0; slot < slotCount && requestedSlots > 0; slot++) {
        std::pair<u64, u8> key = std::make_pair(source->apCoinSourceId, static_cast<u8>(slot));
        if (sm64_permanent_coins.count(key) != 0) {
            continue;
        }
        SM64AP_RecordPermanentCoin(
            key,
            source->apCoinCourse != 0 ? source->apCoinCourse : static_cast<u8>(gCurrCourseNum),
            static_cast<u8>(value));
        collectedValue += value;
        requestedSlots--;
    }
    return collectedValue;
}

u64 SM64AP_PermanentCoinMask(struct Object *source, int slotCount, int value) {
    u64 mask = 0;
    if (source == nullptr || source->apCoinSourceId == 0) {
        return 0;
    }
    SM64AP_NormalizeInheritedCoinSource(source);
    source->apCoinSlotCount = std::max(source->apCoinSlotCount, static_cast<u8>(slotCount));
    for (int slot = 0; slot < slotCount && slot < 64; slot++) {
        auto record = sm64_permanent_coins.find(std::make_pair(source->apCoinSourceId, static_cast<u8>(slot)));
        if (record != sm64_permanent_coins.end()
            && (value <= 0 || record->second.value == value)) {
            mask |= 1ULL << slot;
        }
    }
    return mask;
}

int SM64AP_CollectedPermanentRedCoins(int course) {
    int collected = 0;


    for (const auto &entry : sm64_permanent_coins) {
        if (entry.second.course == course && entry.second.value == 2) {
            collected++;
        }
    }
    return collected;
}

void SM64AP_RestorePermanentCoinCount() {
    if (gMarioState == nullptr
        || gCurrCourseNum <= COURSE_NONE || gCurrCourseNum > COURSE_MAX) {
        return;
    }

    int total = 0;
    for (const auto &entry : sm64_permanent_coins) {
        if (entry.second.course == gCurrCourseNum) {
            total += entry.second.value;
        }
    }
    if (gMarioState->numCoins < total) {
        gMarioState->numCoins = total;
        gHudDisplay.coins = total;
        SM64AP_CheckCoinCount(gCurrCourseNum, total);
    }
}

bool SM64AP_ShouldSpawnOutstandingCoinStar() {
    if (!COURSE_IS_MAIN_COURSE(gCurrCourseNum)
        || gMarioState == nullptr
        || SM64AP_CollectedCourseStar(gCurrCourseNum - COURSE_MIN, 6)) {
        return false;
    }
    return gMarioState->numCoins >= SM64AP_GetCoinStarRequirement(gCurrCourseNum);
}

void SM64AP_FlushPermanentCoinLedger() {
    if (!sm64_pending_coin_output_checks.empty()) {
        std::vector<std::pair<u64, u8>> pending(
            sm64_pending_coin_output_checks.begin(), sm64_pending_coin_output_checks.end());
        for (const auto &physicalKey : pending) {
            SM64AP_SendCoinOutputCheck(physicalKey);
        }
    }
    if (!sm64_pending_coin_completion_checks.empty()) {
        std::vector<u64> pending(
            sm64_pending_coin_completion_checks.begin(),
            sm64_pending_coin_completion_checks.end());
        for (u64 physicalSource : pending) {
            SM64AP_SendCoinCompletionCheck(physicalSource);
        }
    }

    if (sm64_permanent_coin_updates.empty() || sm64_permanent_coin_ledger_key.empty()) {
        return;
    }

    std::string serialized = SM64AP_SerializePermanentCoinEntries(sm64_permanent_coin_updates);
    std::ostringstream request;
    request << "[{\"cmd\":\"Set\",\"key\":\"" << sm64_permanent_coin_ledger_key
            << "\",\"operations\":[{\"operation\":\"update\",\"value\":" << serialized
            << "}],\"default\":{},\"want_reply\":false}]";
    SM64AP_SendSerializedRequest(request.str());
    sm64_permanent_coin_updates.clear();
}

static std::string SM64AP_SerializeUncollectTrapEvent(
    int ordinal, const std::pair<u64, u8> &key, const SM64APPermanentCoinRecord &record) {
    std::ostringstream output;
    output << "{\"ordinal\":" << ordinal
           << ",\"source_high\":" << static_cast<s32>(key.first >> 32)
           << ",\"source_low\":" << static_cast<s32>(key.first)
           << ",\"slot\":" << static_cast<int>(key.second)
           << ",\"course\":" << static_cast<int>(record.course)
           << ",\"value\":" << static_cast<int>(record.value) << '}';
    return output.str();
}

static void SM64AP_ApplyUncollectTrapEvent(const SM64APUncollectTrapEvent &event) {
    if (event.source == 0) {
        return;
    }

    std::pair<u64, u8> key = std::make_pair(event.source, event.slot);
    sm64_permanent_coins.erase(key);
    sm64_permanent_coin_updates.erase(key);
    sm64_uncollected_coin_tombstones.insert(key);
}

void SM64AP_UpdatePermanentCoinTrap() {
    std::queue<int> pendingTraps;
    std::queue<SM64APUncollectTrapEvent> pendingEvents;
    {
        std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
        std::swap(pendingTraps, sm64_pending_uncollect_coin_traps);
        std::swap(pendingEvents, sm64_pending_uncollect_trap_events);
    }

    while (!pendingEvents.empty()) {
        SM64AP_ApplyUncollectTrapEvent(pendingEvents.front());
        pendingEvents.pop();
    }

    if (!pendingTraps.empty()) {
        int ordinal = pendingTraps.front();
        pendingTraps.pop();
        std::string prefix = AP_GetPrivateServerDataPrefix();
        int candidate = static_cast<int>(
            ((AP_GetUUID() ^ (static_cast<u64>(random_u16()) << 16) ^ random_u16())
             % (std::numeric_limits<int>::max() - 1)) + 1);
        SM64APUncollectTrapState state = {
            ordinal,
            candidate,
            std::numeric_limits<int>::max(),
            0,
            false,
            prefix + "PermanentCoinTrapClaim;" + std::to_string(ordinal),
            prefix + "PermanentCoinTrapEvent;" + std::to_string(ordinal),
        };
        {
            std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
            sm64_uncollect_trap_states[ordinal] = state;
        }

        AP_SetServerDataRequest claimRequest;
        AP_DataStorageOperation claimMinimum = { "min", &candidate };
        int defaultCandidate = std::numeric_limits<int>::max();
        claimRequest.key = state.claimKey;
        claimRequest.operations = { claimMinimum };
        claimRequest.default_value = &defaultCandidate;
        claimRequest.type = AP_DataType::Int;
        claimRequest.want_reply = true;
        AP_SetServerData(&claimRequest);
        AP_SetNotify({
            { state.claimKey, AP_DataType::Int },
            { state.eventKey, AP_DataType::Raw },
        }, true);
    }

    if (!pendingTraps.empty()) {
        std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
        while (!pendingTraps.empty()) {
            sm64_pending_uncollect_coin_traps.push(pendingTraps.front());
            pendingTraps.pop();
        }
    }

    std::vector<int> winningOrdinals;
    {
        std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
        for (auto &entry : sm64_uncollect_trap_states) {
            SM64APUncollectTrapState &state = entry.second;
            if (state.eventReceived || state.winner == std::numeric_limits<int>::max()) {
                continue;
            }
            state.frames++;
            if (state.frames >= SM64AP_UNCOLLECT_TRAP_ELECTION_FRAMES
                && state.winner == state.candidate) {
                state.eventReceived = true;
                winningOrdinals.push_back(state.ordinal);
            }
        }
    }

    for (int ordinal : winningOrdinals) {
        SM64APUncollectTrapState state;
        {
            std::lock_guard<std::mutex> lock(sm64_permanent_coin_mutex);
            state = sm64_uncollect_trap_states.at(ordinal);
        }

        std::pair<u64, u8> selectedKey = { 0, 0 };
        SM64APPermanentCoinRecord selectedRecord = { 0, 0 };
        if (!sm64_permanent_coins.empty()) {
            u64 selectionHash = AP_GetUUID() ^ (static_cast<u64>(ordinal) * 0x9E3779B97F4A7C15ULL);
            auto selected = sm64_permanent_coins.begin();
            std::advance(selected, selectionHash % sm64_permanent_coins.size());
            selectedKey = selected->first;
            selectedRecord = selected->second;
        }

        std::string eventJson =
            SM64AP_SerializeUncollectTrapEvent(ordinal, selectedKey, selectedRecord);
        SM64AP_ApplyUncollectTrapEvent({
            ordinal, selectedKey.first, selectedKey.second,
            selectedRecord.course, selectedRecord.value
        });

        std::string emptyObject = "{}";
        AP_SetServerDataRequest eventRequest;
        AP_DataStorageOperation replaceEvent = { "replace", &eventJson };
        eventRequest.key = state.eventKey;
        eventRequest.operations = { replaceEvent };
        eventRequest.default_value = &emptyObject;
        eventRequest.type = AP_DataType::Raw;
        eventRequest.want_reply = true;
        AP_BulkSetServerData(&eventRequest);

        if (selectedKey.first != 0) {
            std::string serializedKey = "\"" + SM64AP_PermanentCoinKey(
                selectedKey.first, selectedKey.second) + "\"";
            AP_SetServerDataRequest ledgerRequest;
            AP_DataStorageOperation removeCoin = { "pop", &serializedKey };
            ledgerRequest.key = sm64_permanent_coin_ledger_key;
            ledgerRequest.operations = { removeCoin };
            ledgerRequest.default_value = &emptyObject;
            ledgerRequest.type = AP_DataType::Raw;
            ledgerRequest.want_reply = true;
            AP_BulkSetServerData(&ledgerRequest);
        }
        AP_CommitServerData();
    }
}

void SM64AP_RequestLiveObjectReconcile() {
    sm64_live_object_reconcile_frames.store(
        SM64AP_LIVE_OBJECT_RECONCILE_DELAY, std::memory_order_release);
}

bool SM64AP_ConsumeLiveObjectReconcileRequest() {
    int frames = sm64_live_object_reconcile_frames.load(std::memory_order_acquire);
    while (frames > 0) {
        if (sm64_live_object_reconcile_frames.compare_exchange_weak(
                frames, frames - 1, std::memory_order_acq_rel)) {
            return frames == 1;
        }
    }
    return false;
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

enum SM64APCheatItemKind {
    SM64AP_CHEAT_ITEM_BOOL,
    SM64AP_CHEAT_ITEM_CASTLE_KEY,
    SM64AP_CHEAT_ITEM_MIPS,
    SM64AP_CHEAT_ITEM_CANNON,
    SM64AP_CHEAT_ITEM_PAINTING,
    SM64AP_CHEAT_ITEM_FEATURE,
    SM64AP_CHEAT_ITEM_LEVEL_CAP,
    SM64AP_CHEAT_ITEM_OBJECT,
    SM64AP_CHEAT_ITEM_ABILITY,
    SM64AP_CHEAT_ITEM_LEVEL_MOVE,
    SM64AP_CHEAT_ITEM_COIN_UNLOCK,
    SM64AP_CHEAT_ITEM_ONE_UP_UNLOCK,
    SM64AP_CHEAT_ITEM_BOWSER_ARENA_BOMB,
    SM64AP_CHEAT_ITEM_CAP_LENGTH,
    SM64AP_CHEAT_ITEM_SIGN_UNLOCK,
    SM64AP_CHEAT_ITEM_ENEMY_UNLOCK,
};

enum SM64APCheatBoolItem {
    SM64AP_CHEAT_BOOL_WING_CAP_LIGHT,
    SM64AP_CHEAT_BOOL_BBH,
    SM64AP_CHEAT_BOOL_TOADS,
    SM64AP_CHEAT_BOOL_CASTLE_CANNON,
    SM64AP_CHEAT_BOOL_WMOTR_CANNON,
    SM64AP_CHEAT_BOOL_YOSHI,
    SM64AP_CHEAT_BOOL_BITFS,
    SM64AP_CHEAT_BOOL_HAT,
    SM64AP_CHEAT_BOOL_VCUTM_ENTRANCE,
    SM64AP_CHEAT_BOOL_BOWSER_STAGE_1UPS,
    SM64AP_CHEAT_BOOL_BITDW_1UPS,
    SM64AP_CHEAT_BOOL_BITFS_1UPS,
    SM64AP_CHEAT_BOOL_THI_TINY_PAINTING,
    SM64AP_CHEAT_BOOL_WING_CAP,
    SM64AP_CHEAT_BOOL_METAL_CAP,
    SM64AP_CHEAT_BOOL_VANISH_CAP,
    SM64AP_CHEAT_BOOL_RR_LEVEL_UNLOCK,
    SM64AP_CHEAT_BOOL_WMOTR_LEVEL_UNLOCK,
};

struct SM64APCheatItem {
    std::string name;
    int kind;
    int index;
};

static std::vector<SM64APCheatItem> sm64ap_cheat_items;

static constexpr const char *SM64AP_CHEAT_COURSE_NAMES[15] = {
    "BOB", "WF", "JRB", "CCM", "BBH",
    "HMC", "LLL", "SSL", "DDD", "SL",
    "WDW", "TTM", "THI", "TTC", "RR",
};

static constexpr const char *SM64AP_CHEAT_CASTLE_KEY_NAMES[SM64AP_NUM_CASTLE_KEYS] = {
    "FIRST FLOOR KEY",
    "BASEMENT KEY",
    "KEY 30",
    "SECOND FLOOR KEY",
    "KEY 50",
    "KEY 70",
};

static constexpr const char *SM64AP_CHEAT_FEATURE_NAMES[SM64AP_NUM_FEATURES] = {
    "BOB KING BOBOMB",
    "BOB KOOPA QUICK",
    "BOB BUDDY",
    "WF WHOMP KING",
    "WF FORTRESS",
    "WF BUDDY",
    "WF HOOT",
    "CCM SNOWMAN BODY",
    "CCM BIG PENGUIN",
    "JRB SUNKEN SHIP",
    "JRB RAISED SHIP",
    "JRB BUDDY",
    "JRB JET STREAM",
    "JRB UNAGI",
    "LLL KOOPA SHELL",
    "SSL KLEPTO WITH STAR",
    "THI KOOPA QUICK",
    "TTM UKIKI",
    "DDD MANTA RAY",
    "DDD BOWSERS SUB",
    "DDD POLES",
    "BBH STAIRCASE",
    "BBH MERRY GO ROUND",
};

static constexpr const char *SM64AP_CHEAT_LEVEL_CAP_NAMES[SM64AP_NUM_LEVEL_CAPS] = {
    "BOB WING CAP",
    "CASTLE WING CAP",
    "LLL WING CAP",
    "SSL WING CAP",
    "TOTWC WING CAP",
    "WMOTR WING CAP",
    "WF METAL CAP",
    "JRB METAL CAP",
    "HMC METAL CAP",
    "DDD METAL CAP",
    "WDW METAL CAP",
    "COTMC METAL CAP",
    "BITDW METAL CAP",
    "BBH VANISH CAP",
    "DDD VANISH CAP",
    "SL VANISH CAP",
    "VCUTM VANISH CAP",
    "WDW VANISH CAP",
};

static constexpr const char *SM64AP_CHEAT_OBJECT_ITEM_NAMES[SM64AP_NUM_OBJECT_ITEMS] = {
    "HMC SWIMMING BEAST",
    "RR CARPETS",
    "CHECKERBOARDS",
    "THI WARP PIPES",
    "CCM BABY PENGUINS",
    "SL PENGUIN",
    "SSL PYRAMID ELEVATOR",
    "ROLLING LOGS",
    "PURPLE SWITCHES",
    "RESERVED BITFS",
    "WDW WATER DIAMONDS",
    "BOB CHECKERBOARDS",
    "WF CHECKERBOARDS",
    "LLL CHECKERBOARDS",
    "HMC CHECKERBOARDS",
    "VCUTM CHECKERBOARDS",
    "LLL ROLLING LOGS",
    "TTM ROLLING LOGS",
    "BOB PURPLE SWITCH",
    "HMC PURPLE SWITCH",
    "WDW PURPLE SWITCH",
    "RR PURPLE SWITCH",
    "BITDW PURPLE SWITCH",
    "BITS PURPLE SWITCH",
    "TTC SPINNERS",
    "JRB PURPLE SWITCH",
    "DDD PURPLE SWITCH",
    "TTM PURPLE SWITCH",
    "THI PURPLE SWITCH",
    "CCM BUDDY",
    "SSL BUDDY",
    "SL BUDDY",
    "WDW BUDDY",
    "TTM BUDDY",
    "THI BUDDY",
    "RR BUDDY",
    "WMOTR BUDDY",
    "BOBOMB BUDDIES",
    "JRB TREASURE CHESTS",
    "DDD TREASURE CHESTS",
    "TREASURE CHESTS",
    "BITDW WARP PIPES",
    "BITFS WARP PIPES",
    "BITS WARP PIPES",
    "WARP PIPES",
};

static constexpr const char *SM64AP_CHEAT_ABILITY_NAMES[SM64AP_NUM_ABILITIES] = {
    "DOUBLE JUMP",
    "TRIPLE JUMP",
    "LONG JUMP",
    "BACKFLIP",
    "SIDE FLIP",
    "WALL KICK",
    "DIVE",
    "GROUND POUND",
    "KICK",
    "CLIMB",
    "LEDGE GRAB",
};

static constexpr const char *SM64AP_CHEAT_LEVEL_MOVE_AREA_NAMES[SM64AP_NUM_LEVEL_MOVE_AREAS] = {
    "BOB",
    "WF",
    "JRB",
    "CCM",
    "BBH",
    "HMC",
    "LLL",
    "SSL",
    "DDD",
    "SL",
    "WDW",
    "TTM",
    "THI",
    "TTC",
    "RR",
    "CASTLE",
    "BITDW",
    "BITFS",
    "BITS",
    "VCUTM",
    "COTMC",
    "TOTWC",
    "WMOTR",
    "MISC",
};

static constexpr const char *SM64AP_CHEAT_LEVEL_MOVE_NAMES[SM64AP_NUM_LEVEL_MOVES] = {
    "TRIPLE JUMP",
    "LONG JUMP",
    "BACKFLIP",
    "SIDE FLIP",
    "WALL KICK",
    "DIVE",
    "GROUND POUND",
    "KICK",
    "CLIMB",
    "LEDGE GRAB",
};

static void SM64AP_CheatAdd(int kind, int index, const std::string &name) {
    sm64ap_cheat_items.push_back({ name, kind, index });
}

static void SM64AP_InitCheatItems() {
    if (!sm64ap_cheat_items.empty()) {
        return;
    }

    for (int i = 0; i < SM64AP_NUM_CASTLE_KEYS; i++) {
        SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_CASTLE_KEY, i, SM64AP_CHEAT_CASTLE_KEY_NAMES[i]);
    }
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_MIPS, 1, "MIPS 1");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_MIPS, 2, "MIPS 2");

    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_WING_CAP_LIGHT, "WING CAP LIGHT");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_BBH, "BBH");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_TOADS, "TOADS");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_CASTLE_CANNON, "CASTLE CANNON");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_WMOTR_CANNON, "WMOTR CANNON");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_YOSHI, "YOSHI");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_BITFS, "BITFS");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_HAT, "HAT");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_VCUTM_ENTRANCE, "VCUTM ENTRANCE");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_BOWSER_STAGE_1UPS, "BOWSER 1UPS");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_BITDW_1UPS, "BITDW 1UPS");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_BITFS_1UPS, "BITFS 1UPS");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_WING_CAP, "GLOBAL WING CAP");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_METAL_CAP, "GLOBAL METAL CAP");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_VANISH_CAP, "GLOBAL VANISH CAP");
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_RR_LEVEL_UNLOCK, "RR LEVEL UNLOCK");
    SM64AP_CheatAdd(
        SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_WMOTR_LEVEL_UNLOCK, "WMOTR LEVEL UNLOCK");

    for (int i = 0; i < 15; i++) {
        SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_CANNON, i, std::string(SM64AP_CHEAT_COURSE_NAMES[i]) + " CANNON");
    }
    for (int i = 0; i < SM64AP_NUM_PAINTING_LOCKS; i++) {
        if (i == COURSE_THI - 1) {
            SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_PAINTING, i, "THI HUGE PAINTING");
            SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_BOOL, SM64AP_CHEAT_BOOL_THI_TINY_PAINTING, "THI TINY PAINTING");
        } else {
            SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_PAINTING, i, std::string(SM64AP_CHEAT_COURSE_NAMES[i]) + " PAINTING");
        }
    }
    for (int i = 0; i < SM64AP_NUM_FEATURES; i++) {
        SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_FEATURE, i, SM64AP_CHEAT_FEATURE_NAMES[i]);
    }
    for (int i = 0; i < SM64AP_NUM_LEVEL_CAPS; i++) {
        SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_LEVEL_CAP, i, SM64AP_CHEAT_LEVEL_CAP_NAMES[i]);
    }
    for (int i = 0; i < SM64AP_NUM_OBJECT_ITEMS; i++) {
        if (i != SM64AP_OBJECT_ITEM_RESERVED_BITFS) {
            bool globalItem = i == SM64AP_OBJECT_ITEM_CHECKERBOARD_PLATFORMS
                || i == SM64AP_OBJECT_ITEM_ROLLING_LOGS
                || i == SM64AP_OBJECT_ITEM_PURPLE_SWITCHES
                || i == SM64AP_OBJECT_ITEM_GLOBAL_BOBOMB_BUDDIES
                || i == SM64AP_OBJECT_ITEM_GLOBAL_TREASURE_CHESTS;
            globalItem = globalItem || i == SM64AP_OBJECT_ITEM_GLOBAL_WARP_PIPES;
            SM64AP_CheatAdd(
                SM64AP_CHEAT_ITEM_OBJECT, i,
                globalItem
                    ? std::string("GLOBAL ") + SM64AP_CHEAT_OBJECT_ITEM_NAMES[i]
                    : SM64AP_CHEAT_OBJECT_ITEM_NAMES[i]);
        }
    }
    for (int i = 0; i < SM64AP_NUM_ABILITIES; i++) {
        SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_ABILITY, i, std::string("GLOBAL ") + SM64AP_CHEAT_ABILITY_NAMES[i]);
    }
    for (int area = 0; area < SM64AP_NUM_LEVEL_MOVE_AREAS; area++) {
        for (int move = 0; move < SM64AP_NUM_LEVEL_MOVES; move++) {
            SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_LEVEL_MOVE,
                            area * SM64AP_NUM_LEVEL_MOVES + move,
                            std::string(SM64AP_CHEAT_LEVEL_MOVE_AREA_NAMES[area]) + " " + SM64AP_CHEAT_LEVEL_MOVE_NAMES[move]);
        }
    }
    for (int i = 0; i < SM64AP_NUM_COIN_GLOBAL_ITEMS; i++) {
        if (sm64_coin_global_items[i].source < 0) {
            continue;
        }
        SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_COIN_UNLOCK,
                        SM64AP_COIN_GLOBAL_ITEM_OFFSET + i,
                        std::string("GLOBAL ") + sm64_coin_global_items[i].name);
    }
    for (int i = 0; i < SM64AP_NUM_COIN_LEVEL_ITEMS; i++) {
        if (sm64_coin_level_items[i].source < 0) {
            continue;
        }
        SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_COIN_UNLOCK,
                        SM64AP_COIN_LEVEL_ITEM_OFFSET + i,
                        sm64_coin_level_items[i].name);
    }
    for (int i = 0; i < SM64AP_NUM_ENEMY_UNLOCK_ITEMS; i++) {
        const SM64APEnemyUnlockItem &item = sm64_enemy_unlock_items[i];
        SM64AP_CheatAdd(
            SM64AP_CHEAT_ITEM_ENEMY_UNLOCK, i,
            item.level == -1 ? std::string("GLOBAL ") + item.name : item.name);
    }
    static constexpr const char *globalOneUpNames[SM64AP_NUM_ONE_UP_CATEGORIES] = {
        "GLOBAL FREE 1UPS", "GLOBAL TRIGGER 1UPS", "GLOBAL 1UP BLOCKS", "GLOBAL BUTTERFLIES"
    };
    static constexpr int globalOneUpIds[SM64AP_NUM_ONE_UP_CATEGORIES] = {
        SM64AP_ID_GLOBAL_FREESTANDING_1UPS,
        SM64AP_ID_GLOBAL_TRIGGER_1UPS,
        SM64AP_ID_GLOBAL_1UP_BLOCKS,
        SM64AP_ID_GLOBAL_BUTTERFLIES,
    };
    for (int i = 0; i < SM64AP_NUM_ONE_UP_CATEGORIES; i++) {
        SM64AP_CheatAdd(
            SM64AP_CHEAT_ITEM_ONE_UP_UNLOCK, globalOneUpIds[i],
            globalOneUpNames[i]);
    }
    for (int i = 0; i < SM64AP_NUM_ONE_UP_LEVEL_ITEMS; i++) {
        SM64AP_CheatAdd(
            SM64AP_CHEAT_ITEM_ONE_UP_UNLOCK, SM64AP_ONE_UP_LEVEL_ITEM_OFFSET + i,
            SM64AP_ONE_UP_LEVEL_ITEMS[i].name);
    }
    SM64AP_CheatAdd(SM64AP_CHEAT_ITEM_SIGN_UNLOCK, SM64AP_ID_GLOBAL_SIGNS, "GLOBAL SIGNS");
    for (int i = 0; i < SM64AP_NUM_SIGN_LEVEL_ITEMS; i++) {
        SM64AP_CheatAdd(
            SM64AP_CHEAT_ITEM_SIGN_UNLOCK, SM64AP_SIGN_LEVEL_ITEM_OFFSET + i,
            SM64AP_SIGN_LEVEL_ITEMS[i].name);
    }

    for (int bomb = 1; bomb <= 4; bomb++) {
        SM64AP_CheatAdd(
            SM64AP_CHEAT_ITEM_BOWSER_ARENA_BOMB, 30 + bomb,
            std::string("GLOBAL BOWSER ARENA BOMB ") + std::to_string(bomb));
    }

    static constexpr const char *capLengthNames[] = {
        "WING CAP LENGTH", "METAL CAP LENGTH", "VANISH CAP LENGTH"
    };
    for (int cap = 0; cap < 3; cap++) {
        for (int item = 1; item <= sm64_cap_length_item_counts[cap]; item++) {
            SM64AP_CheatAdd(
                SM64AP_CHEAT_ITEM_CAP_LENGTH, cap * 10000 + item,
                std::string(capLengthNames[cap]) + " " + std::to_string(item));
        }
    }
    static constexpr const char *bowserStageNames[] = { "BITDW", "BITFS", "BITS" };
    static constexpr int bowserStageBombCounts[] = { 4, 4, 5 };
    for (int arena = 0; arena < 3; arena++) {
        for (int bomb = 1; bomb <= bowserStageBombCounts[arena]; bomb++) {
            SM64AP_CheatAdd(
                SM64AP_CHEAT_ITEM_BOWSER_ARENA_BOMB, arena * 10 + bomb,
                std::string(bowserStageNames[arena]) + " BOWSER ARENA BOMB " + std::to_string(bomb));
        }
    }

    static constexpr const char *levelPrefixes[] = {
        "GLOBAL", "CASTLE", "BOB", "WF", "JRB", "CCM", "BBH", "HMC", "LLL", "SSL",
        "DDD", "SL", "WDW", "TTM", "THI", "TTC", "RR", "PSS", "SA", "BITDW", "BITFS",
        "BITS", "VCUTM", "COTMC", "TOTWC", "WMOTR",
    };
    auto levelOrder = [](const std::string &name) {
        for (int i = 0; i < static_cast<int>(sizeof(levelPrefixes) / sizeof(levelPrefixes[0])); i++) {
            size_t length = std::strlen(levelPrefixes[i]);
            if (name.compare(0, length, levelPrefixes[i]) == 0
                && (name.size() == length || name[length] == ' ')) {
                return i;
            }
        }
        return 1;
    };
    std::stable_sort(
        sm64ap_cheat_items.begin(), sm64ap_cheat_items.end(),
        [levelOrder](const SM64APCheatItem &left, const SM64APCheatItem &right) {
            int leftOrder = levelOrder(left.name);
            int rightOrder = levelOrder(right.name);
            return leftOrder == rightOrder ? left.name < right.name : leftOrder < rightOrder;
        });
}

static bool SM64AP_CheatBoolEnabled(int index) {
    switch (index) {
        case SM64AP_CHEAT_BOOL_WING_CAP_LIGHT:
            return sm64_have_wing_cap_light;
        case SM64AP_CHEAT_BOOL_BBH:
            return sm64_have_bbh;
        case SM64AP_CHEAT_BOOL_TOADS:
            return sm64_have_toads;
        case SM64AP_CHEAT_BOOL_CASTLE_CANNON:
            return sm64_have_castle_cannon;
        case SM64AP_CHEAT_BOOL_WMOTR_CANNON:
            return sm64_have_wmotr_cannon;
        case SM64AP_CHEAT_BOOL_YOSHI:
            return sm64_have_yoshi;
        case SM64AP_CHEAT_BOOL_BITFS:
            return sm64_have_bitfs;
        case SM64AP_CHEAT_BOOL_HAT:
            return sm64_have_hat;
        case SM64AP_CHEAT_BOOL_VCUTM_ENTRANCE:
            return sm64_have_vcutm_entrance;
        case SM64AP_CHEAT_BOOL_BOWSER_STAGE_1UPS:
            return sm64_have_bowser_stage_1ups;
        case SM64AP_CHEAT_BOOL_BITDW_1UPS:
            return sm64_have_bitdw_1ups;
        case SM64AP_CHEAT_BOOL_BITFS_1UPS:
            return sm64_have_bitfs_1ups;
        case SM64AP_CHEAT_BOOL_THI_TINY_PAINTING:
            return sm64_have_thi_tiny_painting;
        case SM64AP_CHEAT_BOOL_WING_CAP:
            return sm64_have_wingcap;
        case SM64AP_CHEAT_BOOL_METAL_CAP:
            return sm64_have_metalcap;
        case SM64AP_CHEAT_BOOL_VANISH_CAP:
            return sm64_have_vanishcap;
        case SM64AP_CHEAT_BOOL_RR_LEVEL_UNLOCK:
            return sm64_have_rr_level_unlock;
        case SM64AP_CHEAT_BOOL_WMOTR_LEVEL_UNLOCK:
            return sm64_have_wmotr_level_unlock;
    }

    return false;
}

static void SM64AP_CheatSetBool(int index, bool enabled) {
    switch (index) {
        case SM64AP_CHEAT_BOOL_WING_CAP_LIGHT:
            sm64_have_wing_cap_light = enabled;
            break;
        case SM64AP_CHEAT_BOOL_BBH:
            sm64_have_bbh = enabled;
            break;
        case SM64AP_CHEAT_BOOL_TOADS:
            sm64_have_toads = enabled;
            break;
        case SM64AP_CHEAT_BOOL_CASTLE_CANNON:
            sm64_have_castle_cannon = enabled;
            break;
        case SM64AP_CHEAT_BOOL_WMOTR_CANNON:
            sm64_have_wmotr_cannon = enabled;
            break;
        case SM64AP_CHEAT_BOOL_YOSHI:
            sm64_have_yoshi = enabled;
            break;
        case SM64AP_CHEAT_BOOL_BITFS:
            sm64_have_bitfs = enabled;
            break;
        case SM64AP_CHEAT_BOOL_HAT:
            sm64_have_hat = enabled;
            sm64_hat_restore_with_animation_pending = false;
            sm64_hat_restore_without_animation_pending = enabled;
            break;
        case SM64AP_CHEAT_BOOL_VCUTM_ENTRANCE:
            sm64_have_vcutm_entrance = enabled;
            break;
        case SM64AP_CHEAT_BOOL_BOWSER_STAGE_1UPS:
            sm64_have_bowser_stage_1ups = enabled;
            break;
        case SM64AP_CHEAT_BOOL_BITDW_1UPS:
            sm64_have_bitdw_1ups = enabled;
            break;
        case SM64AP_CHEAT_BOOL_BITFS_1UPS:
            sm64_have_bitfs_1ups = enabled;
            break;
        case SM64AP_CHEAT_BOOL_THI_TINY_PAINTING:
            sm64_have_thi_tiny_painting = enabled;
            break;
        case SM64AP_CHEAT_BOOL_WING_CAP:
            sm64_have_wingcap = enabled;
            break;
        case SM64AP_CHEAT_BOOL_METAL_CAP:
            sm64_have_metalcap = enabled;
            break;
        case SM64AP_CHEAT_BOOL_VANISH_CAP:
            sm64_have_vanishcap = enabled;
            break;
        case SM64AP_CHEAT_BOOL_RR_LEVEL_UNLOCK:
            sm64_have_rr_level_unlock = enabled;
            break;
        case SM64AP_CHEAT_BOOL_WMOTR_LEVEL_UNLOCK:
            sm64_have_wmotr_level_unlock = enabled;
            break;
    }
}

static void SM64AP_CheatSetCastleKey(int key, bool enabled) {
    if (enabled) {
        switch (key) {
            case SM64AP_CASTLE_KEY_FIRST_FLOOR:
                sm64_have_first_floor_key = true;
                SM64AP_SetMin(sm64_have_progressive_keys, 1);
                break;
            case SM64AP_CASTLE_KEY_BASEMENT:
                SM64AP_SetMin(sm64_have_progressive_basement_keys, 1);
                SM64AP_SetMin(sm64_have_progressive_keys, 2);
                break;
            case SM64AP_CASTLE_KEY_BASEMENT_STAR:
                SM64AP_SetMin(sm64_have_progressive_basement_keys, 2);
                SM64AP_SetMin(sm64_have_progressive_keys, 3);
                break;
            case SM64AP_CASTLE_KEY_UPSTAIRS:
                SM64AP_SetMin(sm64_have_progressive_upstairs_keys, 1);
                SM64AP_SetMin(sm64_have_progressive_keys, 4);
                break;
            case SM64AP_CASTLE_KEY_50_STAR:
                SM64AP_SetMin(sm64_have_progressive_upstairs_keys, 2);
                SM64AP_SetMin(sm64_have_progressive_keys, 5);
                break;
            case SM64AP_CASTLE_KEY_70_STAR:
                SM64AP_SetMin(sm64_have_progressive_upstairs_keys, 3);
                SM64AP_SetMin(sm64_have_progressive_keys, 6);
                break;
        }
        return;
    }

    switch (key) {
        case SM64AP_CASTLE_KEY_FIRST_FLOOR:
            sm64_have_first_floor_key = false;
            if (sm64_have_progressive_keys >= 1) sm64_have_progressive_keys = 0;
            break;
        case SM64AP_CASTLE_KEY_BASEMENT:
            if (sm64_have_progressive_basement_keys >= 1) sm64_have_progressive_basement_keys = 0;
            if (sm64_have_progressive_keys >= 2) sm64_have_progressive_keys = 1;
            break;
        case SM64AP_CASTLE_KEY_BASEMENT_STAR:
            if (sm64_have_progressive_basement_keys >= 2) sm64_have_progressive_basement_keys = 1;
            if (sm64_have_progressive_keys >= 3) sm64_have_progressive_keys = 2;
            break;
        case SM64AP_CASTLE_KEY_UPSTAIRS:
            if (sm64_have_progressive_upstairs_keys >= 1) sm64_have_progressive_upstairs_keys = 0;
            if (sm64_have_progressive_keys >= 4) sm64_have_progressive_keys = 3;
            break;
        case SM64AP_CASTLE_KEY_50_STAR:
            if (sm64_have_progressive_upstairs_keys >= 2) sm64_have_progressive_upstairs_keys = 1;
            if (sm64_have_progressive_keys >= 5) sm64_have_progressive_keys = 4;
            break;
        case SM64AP_CASTLE_KEY_70_STAR:
            if (sm64_have_progressive_upstairs_keys >= 3) sm64_have_progressive_upstairs_keys = 2;
            if (sm64_have_progressive_keys >= 6) sm64_have_progressive_keys = 5;
            break;
    }
}

int SM64AP_CheatItemCount() {
    SM64AP_InitCheatItems();
    return static_cast<int>(sm64ap_cheat_items.size());
}

const char *SM64AP_CheatItemName(int index) {
    SM64AP_InitCheatItems();
    if (index < 0 || index >= static_cast<int>(sm64ap_cheat_items.size())) {
        return "";
    }
    return sm64ap_cheat_items[index].name.c_str();
}

bool SM64AP_CheatItemEnabled(int index) {
    SM64AP_InitCheatItems();
    if (index < 0 || index >= static_cast<int>(sm64ap_cheat_items.size())) {
        return false;
    }

    const SM64APCheatItem &item = sm64ap_cheat_items[index];
    switch (item.kind) {
        case SM64AP_CHEAT_ITEM_BOOL:
            return SM64AP_CheatBoolEnabled(item.index);
        case SM64AP_CHEAT_ITEM_CASTLE_KEY:
            return SM64AP_HaveCastleKey(item.index);
        case SM64AP_CHEAT_ITEM_MIPS:
            return SM64AP_HaveProgressiveMips(item.index);
        case SM64AP_CHEAT_ITEM_CANNON:
            return item.index >= 0 && item.index < 15 && sm64_have_cannon[item.index];
        case SM64AP_CHEAT_ITEM_PAINTING:
            return item.index >= 0 && item.index < SM64AP_NUM_PAINTING_LOCKS && sm64_have_painting[item.index];
        case SM64AP_CHEAT_ITEM_FEATURE:
            return item.index >= 0 && item.index < SM64AP_NUM_FEATURES && sm64_have_features[item.index];
        case SM64AP_CHEAT_ITEM_LEVEL_CAP:
            return item.index >= 0 && item.index < SM64AP_NUM_LEVEL_CAPS && sm64_have_level_caps[item.index];
        case SM64AP_CHEAT_ITEM_OBJECT:
            return item.index >= 0 && item.index < SM64AP_NUM_OBJECT_ITEMS && sm64_have_object_items[item.index];
        case SM64AP_CHEAT_ITEM_ABILITY:
            return item.index >= 0 && item.index < SM64AP_NUM_ABILITIES && sm64_have_abilities[item.index];
        case SM64AP_CHEAT_ITEM_LEVEL_MOVE:
            return item.index >= 0
                && item.index < SM64AP_NUM_LEVEL_MOVE_AREAS * SM64AP_NUM_LEVEL_MOVES
                && sm64_have_level_moves[item.index];
        case SM64AP_CHEAT_ITEM_COIN_UNLOCK:
            if (item.index >= SM64AP_COIN_GLOBAL_ITEM_OFFSET
                && item.index < SM64AP_COIN_GLOBAL_ITEM_OFFSET + SM64AP_NUM_COIN_GLOBAL_ITEMS) {
                return sm64_have_coin_global_items[item.index - SM64AP_COIN_GLOBAL_ITEM_OFFSET];
            }
            if (item.index >= SM64AP_COIN_LEVEL_ITEM_OFFSET
                && item.index < SM64AP_COIN_LEVEL_ITEM_OFFSET + SM64AP_NUM_COIN_LEVEL_ITEMS) {
                return sm64_have_coin_level_items[item.index - SM64AP_COIN_LEVEL_ITEM_OFFSET];
            }
            return false;
        case SM64AP_CHEAT_ITEM_ONE_UP_UNLOCK:
            if (item.index >= SM64AP_ONE_UP_GLOBAL_ITEM_OFFSET && item.index < SM64AP_ONE_UP_LEVEL_ITEM_OFFSET) {
                return sm64_have_one_up_global_items[item.index - SM64AP_ONE_UP_GLOBAL_ITEM_OFFSET];
            }
            if (item.index == SM64AP_ID_GLOBAL_BUTTERFLIES) {
                return sm64_have_one_up_global_items[SM64AP_ONE_UP_CATEGORY_BUTTERFLY];
            }
            if (item.index >= SM64AP_ONE_UP_LEVEL_ITEM_OFFSET && item.index <= SM64AP_ONE_UP_LEVEL_ITEM_END) {
                return sm64_have_one_up_level_items[item.index - SM64AP_ONE_UP_LEVEL_ITEM_OFFSET];
            }
            return false;
        case SM64AP_CHEAT_ITEM_BOWSER_ARENA_BOMB: {
            int arena = item.index / 10;
            int bomb = item.index % 10;
            if (arena == 3) {
                return sm64_bowser_arena_bombs[0] >= bomb
                    && sm64_bowser_arena_bombs[1] >= bomb
                    && sm64_bowser_arena_bombs[2] >= bomb;
            }
            return arena >= 0 && arena < 3 && sm64_bowser_arena_bombs[arena] >= bomb;
        }
        case SM64AP_CHEAT_ITEM_CAP_LENGTH: {
            int cap = item.index / 10000;
            int count = item.index % 10000;
            return cap >= 0 && cap < 3 && sm64_cap_length_items[cap] >= count;
        }
        case SM64AP_CHEAT_ITEM_SIGN_UNLOCK:
            if (item.index == SM64AP_ID_GLOBAL_SIGNS) {
                return sm64_have_global_signs;
            }
            return item.index >= SM64AP_SIGN_LEVEL_ITEM_OFFSET
                && item.index <= SM64AP_SIGN_LEVEL_ITEM_END
                && sm64_have_level_signs[item.index - SM64AP_SIGN_LEVEL_ITEM_OFFSET];
        case SM64AP_CHEAT_ITEM_ENEMY_UNLOCK:
            return item.index >= 0 && item.index < SM64AP_NUM_ENEMY_UNLOCK_ITEMS
                && sm64_have_enemy_unlock_items[item.index];
    }

    return false;
}

void SM64AP_CheatSetItemEnabled(int index, bool enabled) {
    SM64AP_InitCheatItems();
    if (index < 0 || index >= static_cast<int>(sm64ap_cheat_items.size())) {
        return;
    }

    const SM64APCheatItem &item = sm64ap_cheat_items[index];
    switch (item.kind) {
        case SM64AP_CHEAT_ITEM_BOOL:
            SM64AP_CheatSetBool(item.index, enabled);
            break;
        case SM64AP_CHEAT_ITEM_CASTLE_KEY:
            SM64AP_CheatSetCastleKey(item.index, enabled);
            break;
        case SM64AP_CHEAT_ITEM_MIPS:
            if (enabled) {
                SM64AP_SetMin(sm64_have_progressive_mips, item.index);
            } else if (sm64_have_progressive_mips >= item.index) {
                sm64_have_progressive_mips = item.index - 1;
            }
            break;
        case SM64AP_CHEAT_ITEM_CANNON:
            if (item.index >= 0 && item.index < 15) sm64_have_cannon[item.index] = enabled;
            break;
        case SM64AP_CHEAT_ITEM_PAINTING:
            if (item.index >= 0 && item.index < SM64AP_NUM_PAINTING_LOCKS) sm64_have_painting[item.index] = enabled;
            break;
        case SM64AP_CHEAT_ITEM_FEATURE:
            if (item.index >= 0 && item.index < SM64AP_NUM_FEATURES) sm64_have_features[item.index] = enabled;
            break;
        case SM64AP_CHEAT_ITEM_LEVEL_CAP:
            if (item.index >= 0 && item.index < SM64AP_NUM_LEVEL_CAPS) sm64_have_level_caps[item.index] = enabled;
            break;
        case SM64AP_CHEAT_ITEM_OBJECT:
            if (item.index >= 0 && item.index < SM64AP_NUM_OBJECT_ITEMS) sm64_have_object_items[item.index] = enabled;
            break;
        case SM64AP_CHEAT_ITEM_ABILITY:
            if (item.index >= 0 && item.index < SM64AP_NUM_ABILITIES) sm64_have_abilities[item.index] = enabled;
            break;
        case SM64AP_CHEAT_ITEM_LEVEL_MOVE:
            if (item.index >= 0 && item.index < SM64AP_NUM_LEVEL_MOVE_AREAS * SM64AP_NUM_LEVEL_MOVES) {
                sm64_have_level_moves[item.index] = enabled;
            }
            break;
        case SM64AP_CHEAT_ITEM_COIN_UNLOCK:
            if (item.index >= SM64AP_COIN_GLOBAL_ITEM_OFFSET
                && item.index < SM64AP_COIN_GLOBAL_ITEM_OFFSET + SM64AP_NUM_COIN_GLOBAL_ITEMS) {
                sm64_have_coin_global_items[item.index - SM64AP_COIN_GLOBAL_ITEM_OFFSET] = enabled;
            } else if (item.index >= SM64AP_COIN_LEVEL_ITEM_OFFSET
                       && item.index < SM64AP_COIN_LEVEL_ITEM_OFFSET + SM64AP_NUM_COIN_LEVEL_ITEMS) {
                sm64_have_coin_level_items[item.index - SM64AP_COIN_LEVEL_ITEM_OFFSET] = enabled;
            }
            break;
        case SM64AP_CHEAT_ITEM_ONE_UP_UNLOCK:
            if (item.index >= SM64AP_ONE_UP_GLOBAL_ITEM_OFFSET && item.index < SM64AP_ONE_UP_LEVEL_ITEM_OFFSET) {
                sm64_have_one_up_global_items[item.index - SM64AP_ONE_UP_GLOBAL_ITEM_OFFSET] = enabled;
            } else if (item.index == SM64AP_ID_GLOBAL_BUTTERFLIES) {
                sm64_have_one_up_global_items[SM64AP_ONE_UP_CATEGORY_BUTTERFLY] = enabled;
            } else if (item.index >= SM64AP_ONE_UP_LEVEL_ITEM_OFFSET
                       && item.index <= SM64AP_ONE_UP_LEVEL_ITEM_END) {
                sm64_have_one_up_level_items[item.index - SM64AP_ONE_UP_LEVEL_ITEM_OFFSET] = enabled;
            }
            break;
        case SM64AP_CHEAT_ITEM_BOWSER_ARENA_BOMB: {
            int arena = item.index / 10;
            int bomb = item.index % 10;
            int value = enabled ? bomb : bomb - 1;
            if (arena == 3) {
                for (int i = 0; i < 3; i++) {
                    if (enabled) {
                        SM64AP_SetMin(sm64_bowser_arena_bombs[i], value);
                    } else if (sm64_bowser_arena_bombs[i] >= bomb) {
                        sm64_bowser_arena_bombs[i] = value;
                    }
                }
            } else if (arena >= 0 && arena < 3) {
                if (enabled) {
                    SM64AP_SetMin(sm64_bowser_arena_bombs[arena], value);
                } else if (sm64_bowser_arena_bombs[arena] >= bomb) {
                    sm64_bowser_arena_bombs[arena] = value;
                }
            }
            break;
        }
        case SM64AP_CHEAT_ITEM_CAP_LENGTH: {
            int cap = item.index / 10000;
            int count = item.index % 10000;
            if (cap >= 0 && cap < 3) {
                if (enabled) {
                    SM64AP_SetMin(sm64_cap_length_items[cap], count);
                } else if (sm64_cap_length_items[cap] >= count) {
                    sm64_cap_length_items[cap] = count - 1;
                }
            }
            break;
        }
        case SM64AP_CHEAT_ITEM_SIGN_UNLOCK:
            if (item.index == SM64AP_ID_GLOBAL_SIGNS) {
                sm64_have_global_signs = enabled;
            } else if (item.index >= SM64AP_SIGN_LEVEL_ITEM_OFFSET
                       && item.index <= SM64AP_SIGN_LEVEL_ITEM_END) {
                sm64_have_level_signs[item.index - SM64AP_SIGN_LEVEL_ITEM_OFFSET] = enabled;
            }
            SM64AP_RequestLiveObjectReconcile();
            break;
        case SM64AP_CHEAT_ITEM_ENEMY_UNLOCK:
            if (item.index >= 0 && item.index < SM64AP_NUM_ENEMY_UNLOCK_ITEMS) {
                sm64_have_enemy_unlock_items[item.index] = enabled;
            }
            break;
    }
    SM64AP_RequestLiveObjectReconcile();
}

int SM64AP_LevelMoveAreaForLevel(s16 level) {
    switch (level) {
        case LEVEL_BOB:
            return SM64AP_LEVEL_MOVE_AREA_BOB;
        case LEVEL_WF:
            return SM64AP_LEVEL_MOVE_AREA_WF;
        case LEVEL_JRB:
            return SM64AP_LEVEL_MOVE_AREA_JRB;
        case LEVEL_CCM:
            return SM64AP_LEVEL_MOVE_AREA_CCM;
        case LEVEL_BBH:
            return SM64AP_LEVEL_MOVE_AREA_BBH;
        case LEVEL_HMC:
            return SM64AP_LEVEL_MOVE_AREA_HMC;
        case LEVEL_LLL:
            return SM64AP_LEVEL_MOVE_AREA_LLL;
        case LEVEL_SSL:
            return SM64AP_LEVEL_MOVE_AREA_SSL;
        case LEVEL_DDD:
            return SM64AP_LEVEL_MOVE_AREA_DDD;
        case LEVEL_SL:
            return SM64AP_LEVEL_MOVE_AREA_SL;
        case LEVEL_WDW:
            return SM64AP_LEVEL_MOVE_AREA_WDW;
        case LEVEL_TTM:
            return SM64AP_LEVEL_MOVE_AREA_TTM;
        case LEVEL_THI:
            return SM64AP_LEVEL_MOVE_AREA_THI;
        case LEVEL_TTC:
            return SM64AP_LEVEL_MOVE_AREA_TTC;
        case LEVEL_RR:
            return SM64AP_LEVEL_MOVE_AREA_RR;
        case LEVEL_CASTLE:
        case LEVEL_CASTLE_GROUNDS:
        case LEVEL_CASTLE_COURTYARD:
        case LEVEL_PSS:
        case LEVEL_SA:
            return SM64AP_LEVEL_MOVE_AREA_CASTLE;
        case LEVEL_BITDW:
        case LEVEL_BOWSER_1:
            return SM64AP_LEVEL_MOVE_AREA_BITDW;
        case LEVEL_BITFS:
        case LEVEL_BOWSER_2:
            return SM64AP_LEVEL_MOVE_AREA_BITFS;
        case LEVEL_BITS:
        case LEVEL_BOWSER_3:
            return SM64AP_LEVEL_MOVE_AREA_BITS;
        case LEVEL_VCUTM:
            return SM64AP_LEVEL_MOVE_AREA_VCUTM;
        case LEVEL_COTMC:
            return SM64AP_LEVEL_MOVE_AREA_COTMC;
        case LEVEL_TOTWC:
            return SM64AP_LEVEL_MOVE_AREA_TOTWC;
        case LEVEL_WMOTR:
            return SM64AP_LEVEL_MOVE_AREA_WMOTR;
    }

    return SM64AP_LEVEL_MOVE_AREA_CASTLE;
}

static int SM64AP_LevelMoveForAbility(int ability) {
    switch (ability) {
        case SM64AP_ID_TRIPLEJUMP - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_TRIPLE_JUMP;
        case SM64AP_ID_LONGJUMP - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_LONG_JUMP;
        case SM64AP_ID_BACKFLIP - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_BACKFLIP;
        case SM64AP_ID_SIDEFLIP - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_SIDE_FLIP;
        case SM64AP_ID_WALLKICK - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_WALL_KICK;
        case SM64AP_ID_DIVE - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_DIVE;
        case SM64AP_ID_GROUNDPOUND - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_GROUND_POUND;
        case SM64AP_ID_KICK - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_KICK;
        case SM64AP_ID_CLIMB - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_CLIMB;
        case SM64AP_ID_LEDGEGRAB - SM64AP_ABILITY_OFFSET:
            return SM64AP_LEVEL_MOVE_LEDGE_GRAB;
    }

    return -1;
}

static bool SM64AP_HaveLevelMove(int area, int move) {
    if (area < 0 || area >= SM64AP_NUM_LEVEL_MOVE_AREAS
        || move < 0 || move >= SM64AP_NUM_LEVEL_MOVES) {
        return false;
    }

    return sm64_have_level_moves[area * SM64AP_NUM_LEVEL_MOVES + move];
}

bool SM64AP_HaveLevelMoveOrGlobal(int area, int move) {
    int ability = move + (SM64AP_ID_TRIPLEJUMP - SM64AP_ABILITY_OFFSET);
    if (move < 0 || move >= SM64AP_NUM_LEVEL_MOVES) {
        return false;
    }

    return sm64_have_abilities[ability]
        || SM64AP_HaveLevelMove(area, move)
        || (area != SM64AP_LEVEL_MOVE_AREA_MISC
            && area >= SM64AP_LEVEL_MOVE_AREA_CASTLE
            && SM64AP_HaveLevelMove(SM64AP_LEVEL_MOVE_AREA_MISC, move));
}

static bool SM64AP_HaveAbilityForCurrentLevel(int ability) {
    int move = SM64AP_LevelMoveForAbility(ability);
    if (move < 0) {
        return sm64_have_abilities[ability];
    }

    return SM64AP_HaveLevelMoveOrGlobal(SM64AP_LevelMoveAreaForLevel(gCurrLevelNum), move);
}

bool SM64AP_CanDoubleJumpForArea(int area) {
    return sm64_have_abilities[SM64AP_ID_DOUBLEJUMP - SM64AP_ABILITY_OFFSET]
           || SM64AP_HaveLevelMoveOrGlobal(area, SM64AP_LEVEL_MOVE_TRIPLE_JUMP);
}

bool SM64AP_CanDoubleJump() {
    return SM64AP_CanDoubleJumpForArea(SM64AP_LevelMoveAreaForLevel(gCurrLevelNum));
}

bool SM64AP_CanTripleJump() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_TRIPLEJUMP - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanLongJump() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_LONGJUMP - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanBackflip() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_BACKFLIP - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanSideFlip() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_SIDEFLIP - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanWallKick() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_WALLKICK - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanDive() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_DIVE - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanGroundPound() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_GROUNDPOUND - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanKick() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_KICK - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanClimb() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_CLIMB - SM64AP_ABILITY_OFFSET);
}

bool SM64AP_CanLedgeGrab() {
    return SM64AP_HaveAbilityForCurrentLevel(SM64AP_ID_LEDGEGRAB - SM64AP_ABILITY_OFFSET);
}

static std::vector<std::string> SM64AP_WrapItemMessage(const std::string &text) {
    static constexpr size_t glyphWidth = 12;
    static constexpr size_t horizontalMargin = 2;
    const size_t maxLineLength = static_cast<size_t>(
        (GFX_DIMENSIONS_FROM_RIGHT_EDGE(0) - GFX_DIMENSIONS_FROM_LEFT_EDGE(0)) / glyphWidth
    ) - horizontalMargin;
    std::vector<std::string> lines;
    size_t position = 0;

    while (position < text.size()) {
        size_t remaining = text.size() - position;
        if (remaining <= maxLineLength) {
            lines.push_back(text.substr(position));
            break;
        }

        size_t end = position + maxLineLength;
        size_t split = text.rfind(' ', end);
        if (split == std::string::npos || split < position) {
            split = end;
        }
        lines.push_back(text.substr(position, split - position));
        position = split;
        while (position < text.size() && text[position] == ' ') {
            position++;
        }
    }

    if (lines.empty()) {
        lines.emplace_back();
    }
    return lines;
}

static void SM64AP_PrintWrappedItemMessage(
    const std::string &itemLine, const std::string &playerLine
) {
    std::vector<std::string> lines = SM64AP_WrapItemMessage(itemLine);
    for (size_t i = 0; i < lines.size(); i++) {
        print_text(
            GFX_DIMENSIONS_FROM_LEFT_EDGE(0),
            static_cast<s32>((lines.size() - i) * 20),
            lines[i].c_str());
    }
    print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), 0, playerLine.c_str());
}

void SM64AP_PrintNext() {
    SM64AP_InitializeServerStorage();
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
    int display_duration = msg_frame_duration;
    if (msg->type == AP_MessageType::ItemSend) {
        AP_ItemSendMessage* o_msg = static_cast<AP_ItemSendMessage*>(msg);
        SM64AP_PrintWrappedItemMessage(
            o_msg->item, "Sent to " + o_msg->recvPlayer);
    } else if (msg->type == AP_MessageType::ItemRecv) {
        AP_ItemRecvMessage* o_msg = static_cast<AP_ItemRecvMessage*>(msg);
        SM64AP_PrintWrappedItemMessage(
            o_msg->item, "Received from " + o_msg->sendPlayer);
    } else if (msg->type == AP_MessageType::Countdown) {
        display_duration = 30;
        AP_CountdownMessage* o_msg = static_cast<AP_CountdownMessage*>(msg);
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0) + SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, std::to_string(o_msg->timer).c_str());
    } else {
        //print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1-0)*20, msg->text.c_str());
    }
    int displayed_frames = msg_frame_duration - cur_msg_frame_duration;
    if (displayed_frames + 1 < display_duration) {
        cur_msg_frame_duration--;
    } else {
        AP_ClearLatestMessage();
        cur_msg_frame_duration = msg_frame_duration;
    }
}
