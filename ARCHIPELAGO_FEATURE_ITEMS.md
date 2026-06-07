# Archipelago-side work for Spicy Mycena 64 feature items

This Spicy Mycena 64 client branch removes the star-select menu as the source of level feature state. The client now ignores object act masks and decides whether star/act-based level elements exist from received Archipelago items, plus a few already-collected star checks.

The Archipelago project needs to define and place the feature items below, then send them to the client as normal received items.

## Client assumptions

- The choose-a-star menu is skipped. The client sets `gCurrActNum` to `1` only as a harmless fallback value.
- `OBJECT_WITH_ACTS` act masks are ignored by the level script loader.
- Level feature item IDs are consecutive:
  - `SM64AP_ID_OFFSET = 3626000`
  - `SM64AP_NUM_LOCS = 244`
  - `SM64AP_FEATURE_OFFSET = 3626245`
  - item ID = `3626245 + feature index`
- Multiplayer clients send `Spicy Mycena 64` as the AP game name.
- The client accepts received item IDs from `3626245` through `3626267` and marks the corresponding feature as obtained.
- These items should be progression items if any location can require the corresponding feature.
- Archipelago logic should stop relying on selected act/star to decide what can exist in a level.
- The old 1-star and 3-star castle doors are now 0-star doors, like the BoB lobby door.
- This fork should not require or generate AP `Power Star` items for progression. The client still tolerates the old star item ID for compatibility, but the gates below replace the progression uses of AP star count.

## Entrance shuffle slot data

The `AreaRando` slot data remains an integer-to-integer map, but WDW and TTC now have multiple entrance variants that should be shuffled independently.

Use `level_id * 10 + variant` for both source keys and destination values. For ordinary destinations, `variant` is still the destination area number used by the client today. For WDW and TTC, the low digit is an entrance mode instead:

### WDW variants

WDW has three source keys, based on which WDW painting height Mario enters:

| Key variant | Meaning |
| ---: | --- |
| `LEVEL_WDW * 10 + 1` | WDW low water |
| `LEVEL_WDW * 10 + 2` | WDW middle water |
| `LEVEL_WDW * 10 + 3` | WDW high water |

The client uses vanilla's WDW entry-height thresholds to select the source key: Mario Y entry `<= 1382.4` is low, `>= 1600.0` is high, and anything between is middle.

When a destination value points to one of these WDW variants, the client loads WDW area 1 and forces the initial water level to low (`31`), middle (`1024`), or high (`2816`). This means any shuffled entrance can lead directly to "WDW low water", "WDW middle water", or "WDW high water".

### TTC variants

TTC has four source keys, based on the clock face at the real TTC entrance:

| Key variant | Meaning |
| ---: | --- |
| `LEVEL_TTC * 10 + 1` | TTC stopped |
| `LEVEL_TTC * 10 + 2` | TTC slow |
| `LEVEL_TTC * 10 + 3` | TTC random |
| `LEVEL_TTC * 10 + 4` | TTC fast |

When a destination value points to one of these TTC variants, the client loads TTC area 1 and forces `gTTCSpeedSetting` to that variant's speed. This means any shuffled entrance can lead directly to "TTC stopped", "TTC slow", "TTC random", or "TTC fast".

The client no longer moves the castle clock hands to whichever entrance currently maps to TTC. The only active clock hands are the real TTC clock hands, and entering the real TTC entrance uses the current clock face to choose one of the four TTC source keys above.

For compatibility, if a new WDW/TTC variant source key is missing from `AreaRando`, the client falls back to the base variant key (`level_id * 10 + 1`) if present. New Archipelago generation should provide all three WDW keys and all four TTC keys when entrance shuffle is enabled.

## Castle key door options

This client replaces the 8-, 30-, 50-, and 70-star castle doors with key-door objects. Existing basement/upstairs key doors are also controlled by AP key tiers.

The old star-cost slot data for `FirstBowserDoorCost`, `BasementDoorCost`, `SecondFloorDoorCost`, and `StarsToFinish` no longer controls castle progression in this fork. Use the key items below instead.

Archipelago should support two key modes:

1. Grouped castle keys:
   - `3626268` / `First Floor Key`: unlocks the old 8-star door.
   - `3626269` / `Progressive Basement Key`, two copies:
     - first copy unlocks the normal basement key door.
     - second copy unlocks the old 30-star door.
   - `3626270` / `Progressive Upstairs Key`, three copies:
     - first copy unlocks the normal second-floor key door.
     - second copy unlocks the old 50-star door.
     - third copy unlocks the old 70-star door and disables endless stairs.
2. Single progressive key:
   - `3626180` / `Progressive Key`, six copies, unlock order:
     1. old 8-star door.
     2. normal basement key door.
     3. old 30-star door.
     4. normal second-floor key door.
     5. old 50-star door.
     6. old 70-star door and endless stairs.

Compatibility note: `3626178` / Basement Key is still accepted as the first basement tier, and `3626179` / Second Floor Key is still accepted as the first upstairs tier. New generation should prefer one of the two modes above instead of mixing key schemes.

The old 70-star door can be walked through without its key, matching vanilla's "walk through after dialog" behavior. However, endless stairs and infinite-stairs music remain active until the 70-star key tier has been received. If the key is present, Mario uses the key unlock animation on that door.

## Castle progression items

These item IDs immediately follow the castle key items:

| ID | Suggested item name | Copies | Client behavior |
| --- | --- | ---: | --- |
| 3626271 | Progressive MIPS | 2 | First copy spawns the first MIPS rabbit if MIPS 1 is unchecked. Second copy spawns the second MIPS rabbit after MIPS 1 is checked and while MIPS 2 is unchecked. |
| 3626272 | Wing Cap Light | 1 | Enables the castle lobby light and the look-up warp to Tower of the Wing Cap. The light still hides after the Wing Cap switch location is checked. |
| 3626273 | BBH | 1 | Enables the castle Boo, courtyard Boo triplet, and cage Boo needed to enter Big Boo's Haunt. |
| 3626274 | Castle Toads | 1 | Spawns all three star-giving castle Toads. Each Toad still switches to its already-collected dialog after its own location is checked. |
| 3626275 | Castle Cannon | 1 | Removes the cannon grill outside the castle, making the already-present castle grounds cannon usable. |

The old `MIPS1Cost` and `MIPS2Cost` slot data no longer controls MIPS availability in this fork. Use the two `Progressive MIPS` items instead.

## Cosmetic slot data

The client accepts optional `MarioColors` slot data to recolor Mario's material-lit clothes/body colors. This is cosmetic only and should not be represented as an item or location.

Use a JSON object with optional RGB arrays. Each channel should be an integer from `0` through `255`.

```json
{
  "shirt": [255, 0, 0],
  "overalls": [0, 0, 255],
  "gloves": [255, 255, 255],
  "shoes": [114, 28, 14],
  "skin": [254, 193, 121],
  "hair": [115, 6, 0]
}
```

Omitted or invalid entries fall back to vanilla colors. The client-side implementation only updates the solid material light colors used for Mario's shirt/cap/arms, overalls, gloves, shoes, skin, and hair. It does not recolor eye textures, mustache textures, the M logo texture, or metal-cap textures/effects.

## Feature item IDs

| ID | Feature index | Suggested item name |
| --- | ---: | --- |
| 3626245 | 0 | BoB: King Bob-omb |
| 3626246 | 1 | BoB: Koopa the Quick |
| 3626247 | 2 | BoB: Bob-omb Buddy |
| 3626248 | 3 | WF: Whomp King |
| 3626249 | 4 | WF: Fortress |
| 3626250 | 5 | WF: Bob-omb Buddy |
| 3626251 | 6 | WF: Hoot |
| 3626252 | 7 | CCM: Snowman's Head |
| 3626253 | 8 | CCM: Big Penguin |
| 3626254 | 9 | JRB: Sunken Ship |
| 3626255 | 10 | JRB: Raised Ship |
| 3626256 | 11 | JRB: Bob-omb Buddy |
| 3626257 | 12 | JRB: Jet Stream |
| 3626258 | 13 | JRB: Unagi |
| 3626259 | 14 | LLL: Koopa Shell |
| 3626260 | 15 | SSL: Klepto Star |
| 3626261 | 16 | THI: Koopa the Quick |
| 3626262 | 17 | TTM: Ukiki |
| 3626263 | 18 | DDD: Manta Ray |
| 3626264 | 19 | DDD: Bowser's Sub |
| 3626265 | 20 | DDD: Poles |
| 3626266 | 21 | BBH: Staircase |
| 3626267 | 22 | BBH: Merry-go-round |

## Feature behavior in the client

Archipelago logic should gate the relevant locations with the same feature names.

- `BoB: King Bob-omb`: spawns King Bob-omb.
- `BoB: Koopa the Quick`: spawns Koopa the Quick and his race endpoint.
- `BoB: Bob-omb Buddy`: swaps BoB from water-bomb cannons and early buddies to cannon lids and the cannon-opening buddy.
- `WF: Whomp King`: spawns King Whomp. WF boss camera is used only while this is present and `WF: Fortress` is not present.
- `WF: Fortress`: spawns the kickable board, 1-Up, Bullet Bill, tower, tower platforms, tower door, and fortress star. Also makes WF use the star 2+ camera behavior instead of the act 1 boss camera behavior.
- `WF: Bob-omb Buddy`: spawns the WF cannon-opening buddy.
- `WF: Hoot`: spawns Hoot in WF.
- `CCM: Snowman's Head`: spawns the snowman body/head flow unless the Snowman's Big Head star is already collected. If that star is collected, the snowman head is prebuilt regardless of this item.
- `CCM: Big Penguin`: spawns the racing penguin.
- `JRB: Sunken Ship`: spawns the sunken ship pieces/interior.
- `JRB: Raised Ship`: spawns the raised ship pieces and sliding box.
- `JRB: Bob-omb Buddy`: spawns the JRB cannon-opening buddy.
- `JRB: Jet Stream`: spawns the JRB jet stream, jet stream star, and whirlpool.
- `JRB: Unagi`: spawns the star-holding Unagi in the wall if JRB Star 2 is not collected; after JRB Star 2 is collected, spawns the later Unagi instead.
- `LLL: Koopa Shell`: spawns the shell exclamation box.
- `SSL: Klepto Star`: spawns Klepto with the star while SSL Star 1 is not collected. If the item is missing or the star is already collected, the no-star Klepto spawns instead.
- `THI: Koopa the Quick`: spawns Koopa the Quick and his race endpoint.
- `TTM: Ukiki`: spawns Ukiki and the Ukiki cage.
- `DDD: Manta Ray`: spawns the Manta Ray.
- `DDD: Bowser's Sub`: spawns Bowser's Sub and its door.
- `DDD: Poles`: spawns DDD poles, the pole whirlpool, the sub door, and the alternate sub object when `DDD: Bowser's Sub` is not present. The normal sub no longer despawns when poles are present.
- `BBH: Staircase`: spawns the hidden staircase.
- `BBH: Merry-go-round`: spawns the merry-go-round, flamethrower, and merry-go-round boo manager.

## Collected-star conditions still used by the client

These are not new feature items, but Archipelago logic should account for them when deciding reachability:

- BoB Star 1 collected enables the BoB bowling ball spawner and the relevant pit bowling ball.
- BoB Star 2 collected enables the TTM bowling-ball spawner used in BoB.
- CCM Snowman's Big Head collected causes the snowman head to be prebuilt regardless of selected act.
- JRB Star 2 collected changes which Unagi variant can spawn when `JRB: Unagi` is obtained.
- SSL Star 1 collected changes `SSL: Klepto Star` from star Klepto to no-star Klepto.
- BBH Star 1 collected swaps ghost-hunt boos/big boo out and normal boos in.
- TTM Star 1's standalone star is always present.

MIPS, Wing Cap light/warp, BBH entry, Toad spawns, the outside castle cannon, and castle star doors no longer depend on AP star count. The normal HUD also no longer displays the AP star count.

## Known follow-up

SSL has extra act 4-6 tweesters that were not assigned a feature item. Because act masks are ignored, those currently spawn whenever SSL loads. Either leave this as intended, gate them behind an existing feature, or add another feature item if Archipelago logic needs to model them.
