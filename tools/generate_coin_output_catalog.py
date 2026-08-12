#!/usr/bin/env python3
"""Generate the raw physical permanent-coin catalog from SM64 level data.

This intentionally does not assign CoinLogic source IDs or AP location IDs.
The output identifies each runtime persistence root by the exact hash used by
SM64AP_AssignPermanentCoinSource and describes its physical output slots.
"""

from __future__ import annotations

import argparse
import ast
import json
import re
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MASK64 = (1 << 64) - 1


SOURCE_NAMES = (
    "yellow_coin", "red_coin", "moving_blue_coin", "blue_coin_switch",
    "horizontal_line", "horizontal_ring", "arrow", "vertical_line", "vertical_ring",
    "bobomb", "boo", "bully", "chuckya", "enemy_lakitu", "eyerok",
    "fire_piranha_plant", "fly_guy", "flying_bookend", "goomba", "koopa_troopa",
    "moneybag", "mr_blizzard", "mr_i", "monty_mole", "big_bully", "piranha_plant",
    "pokey", "scuttlebug", "skeeter", "snufit", "spindrift", "swoop", "whomp",
    "one_coin_block", "three_coin_block", "ten_coin_block", "breakable_coin_box",
    "small_breakable_box", "jumping_box", "wooden_post", "bowser_puzzle", "big_boo",
    "thwomp",
)
SOURCE_INDEX = {name: index for index, name in enumerate(SOURCE_NAMES)}


# behavior: (source category, output values). Repeated values are separate slots.
# None means this is a dynamic producer whose children need an expansion rule.
BEHAVIOR_MANIFEST: dict[str, tuple[str, tuple[int, ...] | None]] = {
    "bhvYellowCoin": ("yellow_coin", (1,)), "bhvOneCoin": ("yellow_coin", (1,)),
    "bhvRedCoin": ("red_coin", (2,)), "bhvMovingBlueCoin": ("moving_blue_coin", (5,)),
    "bhvBlueCoinSliding": ("moving_blue_coin", (5,)),
    "bhvHiddenBlueCoin": ("blue_coin_switch", (5,)),
    "bhvBobomb": ("bobomb", (1,)), "bhvBoo": ("boo", (5,)),
    "bhvGhostHuntBoo": ("boo", (5,)), "bhvBooInCastle": ("boo", (5,)),
    "bhvBooWithCage": ("boo", (5,)), "bhvGhostHuntBigBoo": ("big_boo", (5,)),
    "bhvBalconyBigBoo": ("big_boo", (5,)), "bhvSmallBully": ("bully", (1,)),
    "bhvChuckya": ("chuckya", (1, 1, 1, 1, 1)),
    "bhvEnemyLakitu": ("enemy_lakitu", (1, 1, 1, 1, 1)),
    "bhvFirePiranhaPlant": ("fire_piranha_plant", (1,)),
    "bhvFlyGuy": ("fly_guy", (1, 1)), "bhvFlyingBookend": ("flying_bookend", (5,)),
    "bhvGoomba": ("goomba", (1,)), "bhvKoopa": ("koopa_troopa", (5,)),
    "bhvMoneybag": ("moneybag", (1, 1, 1, 1, 1)),
    "bhvMoneybagHidden": ("moneybag", (1, 1, 1, 1, 1)),
    "bhvMrBlizzard": ("mr_blizzard", (1, 1, 1)), "bhvMrI": ("mr_i", (5,)),
    "bhvBigBully": ("big_bully", (5,)), "bhvPiranhaPlant": ("piranha_plant", (5,)),
    "bhvPokey": ("pokey", (5,)),
    "bhvScuttlebug": ("scuttlebug", (1, 1, 1)), "bhvSkeeter": ("skeeter", (1, 1, 1)),
    "bhvSnufit": ("snufit", (1, 1)), "bhvSpindrift": ("spindrift", (1, 1, 1)),
    "bhvSwoop": ("swoop", (1,)), "bhvSmallWhomp": ("whomp", (1,) * 10),
    "bhvThwomp": ("thwomp", (1,)), "bhvThwomp2": ("thwomp", (1,)),
    "bhvBreakableBoxSmall": ("small_breakable_box", (1, 1, 1)),
    "bhvJumpingBox": ("jumping_box", (1,) * 5), "bhvWoodenPost": ("wooden_post", (1,) * 5),
    "bhvLllBowserPuzzle": ("bowser_puzzle", (1,) * 5),
    "bhvGoombaTripletSpawner": ("goomba", None),
    "bhvMerryGoRoundBooManager": ("boo", None), "bhvBookendSpawn": ("flying_bookend", None),
    "bhvCourtyardBooTriplet": ("boo", None), "bhvScuttlebugSpawn": ("scuttlebug", None),
}

DYNAMIC_MANIFEST = {
    "bhvGoombaTripletSpawner": {"kind": "relative_children", "count": 3, "child": "bhvGoomba"},
    "bhvMerryGoRoundBooManager": {"kind": "ordinal_children", "count": 5, "child": "bhvMerryGoRoundBoo"},
    "bhvCourtyardBooTriplet": {"kind": "runtime_children", "count": 3, "child": "bhvBooInCastle"},
    "bhvBookendSpawn": {"kind": "runtime_child", "count": 1, "child": "bhvFlyingBookend"},
    "bhvScuttlebugSpawn": {"kind": "runtime_children", "count": None, "child": "bhvScuttlebug"},
}

# Producers that do not themselves match SM64AP_EnemyCoinSource. Their placed
# parent therefore hashes with sourceKind == -1 before children inherit it.
ROOT_SPAWNER_MANIFEST = {
    "bhvChainChomp": ("wooden_post", {"kind": "spawn_child_at_parent", "count": 1,
                                      "child": "bhvWoodenPost"}),
    "bhvBigBullyWithMinions": ("bully", {"kind": "runtime_children", "count": 3,
                                          "child": "bhvSmallBully"}),
}

DYNAMIC_CHILD_POSITIONS = {
    "bhvBigBullyWithMinions": ((4454, 307, -5426), (3840, 307, -6041), (3226, 307, -5426)),
    "bhvCourtyardBooTriplet": ((0, 50, 0), (210, 110, 210), (-210, 70, -210)),
}


@dataclass
class Placement:
    level: str
    level_id: int
    area: int
    model: str
    model_id: int
    x: int
    y: int
    z: int
    behavior_params: int
    behavior: str
    origin: str
    preset: str | None = None
    yaw: int = 0


def strip_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def split_args(text: str) -> list[str]:
    result, start, depth = [], 0, 0
    for index, char in enumerate(text):
        if char == "(": depth += 1
        elif char == ")": depth -= 1
        elif char == "," and depth == 0:
            result.append(text[start:index].strip())
            start = index + 1
    result.append(text[start:].strip())
    return result


def calls(text: str, names: tuple[str, ...]):
    pattern = re.compile(r"\b(" + "|".join(map(re.escape, names)) + r")\s*\(")
    for match in pattern.finditer(text):
        depth, index = 1, match.end()
        while index < len(text) and depth:
            depth += (text[index] == "(") - (text[index] == ")")
            index += 1
        if depth == 0:
            yield match.group(1), split_args(strip_comments(text[match.end():index - 1])), match.start()


def safe_eval(expression: str, names: dict[str, int]) -> int:
    expression = expression.strip().replace("ULL", "").replace("UL", "").replace("U", "")
    tree = ast.parse(expression, mode="eval")
    allowed = (ast.Expression, ast.Constant, ast.Name, ast.Load, ast.Add, ast.Sub, ast.Mult, ast.BitOr,
               ast.BitAnd, ast.LShift, ast.RShift, ast.UnaryOp, ast.USub, ast.UAdd, ast.BinOp)
    if any(not isinstance(node, allowed) for node in ast.walk(tree)):
        raise ValueError(f"unsupported expression: {expression}")
    return int(eval(compile(tree, "<expression>", "eval"), {"__builtins__": {}}, names))


def parse_numeric_defines(path: Path, names: dict[str, int]) -> None:
    for line in path.read_text().splitlines():
        match = re.match(r"\s*#define\s+(\w+)\s+([^/]+?)(?:\s*//.*)?$", line)
        if not match or "(" in match.group(1):
            continue
        try:
            names[match.group(1)] = safe_eval(match.group(2).strip(), names)
        except (KeyError, NameError, SyntaxError, ValueError):
            pass


def load_constants() -> tuple[dict[str, int], dict[str, str]]:
    names: dict[str, int] = {}
    folders: dict[str, str] = {}
    value = 1
    for line in (ROOT / "levels/level_defines.h").read_text().splitlines():
        match = re.search(r"(?:STUB_LEVEL|DEFINE_LEVEL)\([^,]*,\s*(LEVEL_\w+)", line)
        if match:
            names[match.group(1)] = value
            folder = re.search(r"DEFINE_LEVEL\([^,]*,\s*LEVEL_\w+,\s*[^,]+,\s*(\w+)", line)
            if folder:
                folders[folder.group(1)] = match.group(1)
            value += 1
    for path in (ROOT / "include/model_ids.h", ROOT / "include/coin_formation.h"):
        if path.exists(): parse_numeric_defines(path, names)
    names.update({"COIN_FORMATION_FLAG_VERTICAL": 1, "COIN_FORMATION_FLAG_RING": 2,
                  "COIN_FORMATION_FLAG_ARROW": 4, "COIN_FORMATION_FLAG_FLYING": 16})
    return names, folders


def parse_presets(path: Path, array_name: str) -> list[tuple[str, str, int]]:
    text = strip_comments(path.read_text())
    body = text.split(array_name, 1)[1]
    result = []
    for match in re.finditer(r"\{\s*(bhv\w+|NULL)\s*,\s*(MODEL_\w+)\s*,\s*([^}]+)\}", body):
        result.append((match.group(1), match.group(2), match.group(3).strip()))
    return result


def macro_names() -> list[str]:
    return re.findall(r"\b(macro_\w+)\s*,?", (ROOT / "include/macro_preset_names.h").read_text())


def hash_value(hash_: int, value: int) -> int:
    value &= MASK64
    for byte in range(8):
        hash_ ^= (value >> (byte * 8)) & 0xFF
        hash_ = (hash_ * 1099511628211) & MASK64
    return hash_


def source_hash(p: Placement, source: str, source_kind: int | None = None) -> int:
    result = 1469598103934665603
    if source_kind is None:
        source_kind = SOURCE_INDEX[source]
    for value in (p.level_id & 0xFFFF, p.area & 0xFFFF, source_kind + 1,
                  p.model_id & 0xFFFF, p.x & 0xFFFF, p.y & 0xFFFF, p.z & 0xFFFF,
                  p.behavior_params & 0xFFFFFFFF):
        result = hash_value(result, value)
    return result or 1


def distinguished_child_hash(parent_hash: int, ordinal: int) -> int:
    result = hash_value(parent_hash, 0x4D475242)
    result = hash_value(result, ordinal & 0xFFFFFFFF)
    return result or 1


def inherited_child_hash(parent_hash: int, x: int, y: int, z: int, behavior_params: int) -> int:
    result = parent_hash
    for value in (x, y, z, behavior_params & 0xFFFFFFFF):
        result = hash_value(result, value)
    return result or 1


def trig_table() -> list[float]:
    body = (ROOT / "include/trig_tables.inc.c").read_text().split("f32 gSineTable[] = {", 1)[1]
    # With AVOID_UB the conditional first closing brace is omitted and the
    # following cosine values are the overlapping tail of gSineTable.
    return [float(value.rstrip("f")) for value in re.findall(r"[-+]?(?:\d+\.\d*|\.\d+)(?:[Ee][-+]?\d+)?f?", body)]


def expand_goomba_triplet(parent_hash: int, p: Placement) -> list[dict]:
    table = trig_table()
    size = (p.behavior_params >> 16) & 0x03
    extra = ((p.behavior_params >> 16) & 0x0C) >> 2
    count = 3 + extra
    angle_step = 0x10000 // count
    children = []
    for ordinal in range(count):
        angle = ordinal * angle_step
        dx = int(500.0 * table[((angle & 0xFFFF) >> 4) + 0x400])
        dz = int(500.0 * table[(angle & 0xFFFF) >> 4])
        # All vanilla triplet roots have zero yaw. Keep this explicit so a
        # future rotated root fails instead of receiving an approximate hash.
        if p.yaw != 0:
            raise ValueError(f"rotated Goomba triplet is unsupported: {p.origin} at {p.x, p.y, p.z}")
        child_param = size | (1 << (ordinal + 2))
        behavior_params = child_param << 16
        child_hash = inherited_child_hash(parent_hash, p.x + dx, p.y, p.z + dz, behavior_params)
        values = (1,) * 5 if size == 1 else (1,)
        children.append({
            "physical_source_hash": str(child_hash), "parent_physical_source_hash": str(parent_hash),
            "dynamic_ordinal": ordinal, "level": p.level, "level_id": p.level_id, "area": p.area,
            "model": "MODEL_GOOMBA", "position": [p.x + dx, p.y, p.z + dz],
            "behavior_params": f"0x{behavior_params:08X}", "behavior": "bhvGoomba",
            "source_category": "goomba", "origin": p.origin, "preset": p.preset,
            "outputs": [{"slot": slot, "value": value} for slot, value in enumerate(values)],
            "producer_completion": {"required_slots": len(values)} if size == 1 else None,
        })
    return children


def dynamic_child_record(parent_hash: int, p: Placement, source: str, behavior: str,
                         ordinal: int, position: tuple[int, int, int], values: tuple[int, ...],
                         behavior_params: int = 0) -> dict:
    child_hash = inherited_child_hash(parent_hash, *position, behavior_params)
    return {
        "physical_source_hash": str(child_hash), "parent_physical_source_hash": str(parent_hash),
        "dynamic_ordinal": ordinal, "level": p.level, "level_id": p.level_id, "area": p.area,
        "model": None, "model_id": None, "position": list(position),
        "behavior_params": f"0x{behavior_params:08X}", "behavior": behavior,
        "source_category": source, "origin": p.origin, "preset": p.preset,
        "outputs": [{"slot": slot, "value": value} for slot, value in enumerate(values)],
    }


def expand_inherited_dynamic(parent_hash: int, p: Placement, source: str, dynamic: dict) -> list[dict] | None:
    if p.behavior in DYNAMIC_CHILD_POSITIONS:
        positions = DYNAMIC_CHILD_POSITIONS[p.behavior]
    elif p.behavior in ("bhvBookendSpawn", "bhvScuttlebugSpawn", "bhvChainChomp"):
        positions = ((p.x, p.y, p.z),)
    else:
        return None
    child = dynamic["child"]
    values = {
        "bhvFlyingBookend": (5,), "bhvScuttlebug": (1, 1, 1), "bhvWoodenPost": (1,) * 5,
        "bhvSmallBully": (1,), "bhvBooInCastle": (5,),
    }[child]
    params = 0x00010000 if p.behavior == "bhvCourtyardBooTriplet" else 0
    return [dynamic_child_record(parent_hash, p, source, child, ordinal, position, values, params)
            for ordinal, position in enumerate(positions)]


def classify(behavior: str, params: int) -> tuple[str, tuple[int, ...] | None] | None:
    if behavior == "bhvCoinFormation":
        flags = (params >> 16) & 0xFF
        if flags & 4: return "arrow", (1,) * 8
        if flags & 2: return ("vertical_ring" if flags & 1 else "horizontal_ring"), (1,) * 8
        return ("vertical_line" if flags & 1 else "horizontal_line"), (1,) * 5
    if behavior == "bhvExclamationBox":
        if (params >> 16) == 0x1404: return None
        content = (params >> 16) & 0xFF
        return {4: ("one_coin_block", (1,)), 5: ("three_coin_block", (1,) * 3),
                6: ("ten_coin_block", (1,) * 10)}.get(content)
    if behavior == "bhvBreakableBox" and ((params >> 16) & 0xFF) == 1:
        return "breakable_coin_box", (1,) * 3
    if behavior == "bhvKoopa" and ((params >> 16) & 0xFF) in (2, 3):
        return None
    if behavior == "bhvMrI" and ((params >> 16) & 0xFF) == 1:
        return None
    manifest = BEHAVIOR_MANIFEST.get(behavior)
    if behavior == "bhvGoomba" and ((params >> 16) & 0xFF) == 1:
        return "goomba", (1,) * 5
    return manifest


def parse_level_objects(constants: dict[str, int], folders: dict[str, str]) -> list[Placement]:
    result = []
    for folder, level in folders.items():
        path = ROOT / "levels" / folder / "script.c"
        if not path.exists(): continue
        text = path.read_text()
        functions = {m.group(1): m.group(2) for m in re.finditer(
            r"(?:static\s+)?const LevelScript\s+(\w+)\[\]\s*=\s*\{(.*?)\n\};", text, re.S)}
        entry = functions.get(f"level_{folder}_entry", "")
        for area_match in re.finditer(r"AREA\s*\([^,]+,.*?\)(.*?)END_AREA\s*\(\)", entry, re.S):
            area_call = entry[area_match.start():area_match.start(1)]
            area = int(re.search(r"AREA\s*\([^0-9-]*(\d+)", area_call).group(1))
            body = area_match.group(1)
            linked = re.findall(r"JUMP_LINK\s*\(\s*(\w+)\s*\)", body)
            bodies = [(body, f"{path.relative_to(ROOT)}:area{area}")] + [
                (functions[name], f"{path.relative_to(ROOT)}:{name}") for name in linked if name in functions]
            for object_text, origin in bodies:
                for _, args, _ in calls(object_text, ("OBJECT", "OBJECT_WITH_ACTS")):
                    if len(args) < 9: continue
                    try:
                        model, x, y, z = args[0], *(safe_eval(arg, constants) for arg in args[1:4])
                        params, behavior = safe_eval(args[7], constants), args[8]
                        result.append(Placement(level, constants[level], area, model, constants[model], x, y, z,
                                                params & 0xFFFFFFFF, behavior, origin, yaw=safe_eval(args[5], constants)))
                    except (KeyError, ValueError, SyntaxError, NameError):
                        continue
    return result


def parse_macros(constants: dict[str, int], folders: dict[str, str]) -> list[Placement]:
    preset_names = macro_names()
    presets = parse_presets(ROOT / "include/macro_presets.h", "MacroObjectPresets")
    preset_map = dict(zip(preset_names, presets))
    result = []
    for path in ROOT.glob("levels/*/areas/*/macro.inc.c"):
        folder, area = path.parts[-4], int(path.parts[-2])
        level = folders.get(folder)
        if not level: continue
        for call, args, _ in calls(path.read_text(), ("MACRO_OBJECT", "MACRO_OBJECT_WITH_BEH_PARAM")):
            if len(args) < 5 or args[0] not in preset_map: continue
            behavior, model, default_param = preset_map[args[0]]
            try:
                raw_param = safe_eval(args[5], constants) if call.endswith("WITH_BEH_PARAM") else 0
                default = safe_eval(default_param, constants)
                if default: raw_param = (raw_param & 0xFF00) + (default & 0xFF)
                params = ((raw_param & 0xFF) << 16) + (raw_param & 0xFF00)
                x, y, z = (safe_eval(arg, constants) for arg in args[2:5])
                result.append(Placement(level, constants[level], area, model, constants[model], x, y, z,
                                        params, behavior, str(path.relative_to(ROOT)), args[0],
                                        yaw=safe_eval(args[1], constants)))
            except (KeyError, ValueError, NameError):
                continue
    return result


def parse_specials(constants: dict[str, int], folders: dict[str, str]) -> list[Placement]:
    text = strip_comments((ROOT / "include/special_presets.h").read_text())
    presets = {}
    for match in re.finditer(r"\{\s*(0x[0-9A-Fa-f]+)\s*,\s*(SPTYPE_\w+)\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*"
                             r"(MODEL_\w+)\s*,\s*(bhv\w+|NULL)\s*\}", text):
        presets[int(match.group(1), 16)] = match.groups()[1:]
    special_ids = {name: index for index, name in enumerate(re.findall(
        r"\b(special_\w+)\s*(?:=\s*(0x[0-9A-Fa-f]+|special_\w+))?\s*,?",
        (ROOT / "include/special_preset_names.h").read_text()))}  # overwritten below
    # The source uses explicit aliases and jumps; map names from the preset table comments by enum evaluation.
    special_ids = {}
    value = 0
    for name, assignment in re.findall(r"\b(special_\w+)\s*(?:=\s*(0x[0-9A-Fa-f]+|special_\w+))?\s*,?",
                                       (ROOT / "include/special_preset_names.h").read_text()):
        if assignment:
            value = int(assignment, 16) if assignment.startswith("0x") else special_ids[assignment]
        special_ids[name] = value
        value += 1
    result = []
    for path in ROOT.glob("levels/*/areas/*/collision.inc.c"):
        folder, area = path.parts[-4], int(path.parts[-2])
        level = folders.get(folder)
        if not level: continue
        for call, args, _ in calls(path.read_text(), ("SPECIAL_OBJECT", "SPECIAL_OBJECT_WITH_YAW",
                                                       "SPECIAL_OBJECT_WITH_YAW_AND_PARAM")):
            if len(args) < 4 or args[0] not in special_ids: continue
            preset_id = special_ids[args[0]]
            if preset_id not in presets: continue
            type_, default, model, behavior = presets[preset_id]
            try:
                x, y, z = (safe_eval(arg, constants) for arg in args[1:4])
                param = safe_eval(args[-1], constants) if type_ == "SPTYPE_PARAMS_AND_YROT" else int(default, 16)
                params = (param & 0xFFFF) << (24 if type_ == "SPTYPE_DEF_PARAM_AND_YROT" else 16)
                result.append(Placement(level, constants[level], area, model, constants[model], x, y, z,
                                        params & 0xFFFFFFFF, behavior, str(path.relative_to(ROOT)), args[0]))
            except (KeyError, ValueError, NameError):
                continue
    return result


def documented_globals() -> dict[str, tuple[int, int]]:
    aliases = {
        "Yellow Coins": "yellow_coin", "Red Coins": "red_coin", "Single Blue Coins": "moving_blue_coin",
        "Horizontal Coin Lines": "horizontal_line", "Horizontal Coin Rings": "horizontal_ring",
        "Coin Arrows": "arrow", "Vertical Coin Lines": "vertical_line", "Vertical Coin Rings": "vertical_ring",
        "Bob-ombs": "bobomb", "Boos": "boo", "Bullies": "bully", "Chuckyas": "chuckya",
        "Enemy Lakitus": "enemy_lakitu", "Fire Piranha Plants": "fire_piranha_plant", "Fly Guys": "fly_guy",
        "Goombas": "goomba", "Koopa Troopas": "koopa_troopa", "Mr. Blizzards": "mr_blizzard",
        "Mr. Is": "mr_i", "Scuttlebugs": "scuttlebug", "Snufits": "snufit", "Spindrifts": "spindrift",
        "Whomps": "whomp", "Breakable Coin Boxes": "breakable_coin_box",
        "Throwable Cork Boxes": "small_breakable_box", "CRAZY Boxes": "jumping_box",
        "Wooden Posts": "wooden_post", "3-Coin Blocks": "three_coin_block", "10-Coin Blocks": "ten_coin_block",
    }
    result = {}
    for line in (ROOT / "COIN_UNLOCK_ITEM_IDS.md").read_text().splitlines():
        match = re.match(r"\| \*\*Global (.+?)\*\* \| \*\*([0-9]+)\*\* \|(?: \*\*([0-9]+)\*\* \|)?", line)
        if match and match.group(1) in aliases:
            numbers = [int(value) for value in re.findall(r"\*\*([0-9]+)\*\*", line)]
            result[aliases[match.group(1)]] = (numbers[0], numbers[-1])
    return result


def generate() -> dict:
    constants, folders = load_constants()
    placements = parse_level_objects(constants, folders) + parse_macros(constants, folders) + parse_specials(constants, folders)
    roots, unknowns = [], []
    seen_roots: dict[tuple[int, str], dict] = {}
    duplicate_roots = []
    placement_counts, coin_counts = Counter(), Counter()
    for p in placements:
        classified = classify(p.behavior, p.behavior_params)
        root_spawner = ROOT_SPAWNER_MANIFEST.get(p.behavior)
        if classified is None and root_spawner is None:
            continue
        if root_spawner is not None:
            source, dynamic = root_spawner
            outputs = None
            hash_ = source_hash(p, source, -1)
        else:
            source, outputs = classified
            if p.behavior == "bhvFirePiranhaPlant" and p.behavior_params & 0x00010000:
                outputs = (1, 1)
            dynamic = DYNAMIC_MANIFEST.get(p.behavior)
            hash_ = source_hash(p, source)
        record = {
            "physical_source_hash": str(hash_), "level": p.level, "level_id": p.level_id, "area": p.area,
            "model": p.model, "model_id": p.model_id, "position": [p.x, p.y, p.z],
            "behavior_params": f"0x{p.behavior_params:08X}", "behavior": p.behavior,
            "source_category": source, "origin": p.origin, "preset": p.preset,
        }
        inherited_children = expand_inherited_dynamic(hash_, p, source, dynamic) if outputs is None else None
        if outputs is None and p.behavior == "bhvGoombaTripletSpawner":
            record["dynamic"] = dynamic
            record["outputs"] = []
            for child in expand_goomba_triplet(hash_, p):
                child_identity = (int(child["physical_source_hash"]), source)
                if child_identity not in seen_roots:
                    seen_roots[child_identity] = child
                    roots.append(child)
                    placement_counts[source] += 1
                    coin_counts[source] += sum(output["value"] for output in child["outputs"])
        elif outputs is None and dynamic and dynamic["kind"] == "ordinal_children":
            record["dynamic"] = dynamic
            record["outputs"] = []
            for ordinal in range(dynamic["count"]):
                child_hash = distinguished_child_hash(hash_, ordinal)
                child = {
                    "physical_source_hash": str(child_hash), "parent_physical_source_hash": str(hash_),
                    "dynamic_ordinal": ordinal, "level": p.level, "level_id": p.level_id, "area": p.area,
                    "model": "MODEL_BOO", "model_id": constants["MODEL_BOO"], "position": None,
                    "behavior_params": "0x00000000", "behavior": dynamic["child"],
                    "source_category": source, "origin": p.origin, "preset": p.preset,
                    "outputs": [{"slot": 0, "value": 5}],
                }
                child_identity = (child_hash, source)
                if child_identity not in seen_roots:
                    seen_roots[child_identity] = child
                    roots.append(child)
                    placement_counts[source] += 1
                    coin_counts[source] += 5
        elif outputs is None and inherited_children is not None:
            record["dynamic"] = dynamic
            record["outputs"] = []
            for child in inherited_children:
                child_identity = (int(child["physical_source_hash"]), source)
                if child_identity not in seen_roots:
                    seen_roots[child_identity] = child
                    roots.append(child)
                    placement_counts[source] += 1
                    coin_counts[source] += sum(output["value"] for output in child["outputs"])
        elif outputs is None:
            record["dynamic"] = DYNAMIC_MANIFEST.get(p.behavior, {"kind": "unknown"})
            record["outputs"] = []
        else:
            record["outputs"] = [{"slot": slot, "value": value} for slot, value in enumerate(outputs)]
        identity = (hash_, source)
        if identity in seen_roots:
            duplicate_roots.append({"physical_source_hash": str(hash_), "source_category": source,
                                    "kept_origin": seen_roots[identity]["origin"],
                                    "duplicate_origin": record["origin"]})
            continue
        seen_roots[identity] = record
        roots.append(record)
        if outputs is None and not (p.behavior == "bhvGoombaTripletSpawner" or inherited_children is not None
                                    or (dynamic and dynamic["kind"] == "ordinal_children")):
            unknowns.append({"reason": "dynamic expansion not exact", **record})
        elif outputs is not None:
            placement_counts[source] += 1
            coin_counts[source] += sum(outputs)
    documented = documented_globals()
    validation = []
    for source, (expected_placements, expected_coins) in sorted(documented.items()):
        validation.append({"source_category": source, "expected_placements": expected_placements,
                           "enumerated_placements": placement_counts[source], "expected_coins": expected_coins,
                           "enumerated_coins": coin_counts[source],
                           "matches": (expected_placements, expected_coins) ==
                                      (placement_counts[source], coin_counts[source])})
    return {"schema_version": 1, "hash_algorithm": "SM64AP_AssignPermanentCoinSource/FNV64-fields-v1",
            "semantic_source_ids_assigned": False, "root_count": len(roots),
            "enumerated_output_count": sum(len(root["outputs"]) for root in roots),
            "enumerated_coin_value": sum(sum(out["value"] for out in root["outputs"]) for root in roots),
            "roots": roots, "duplicate_physical_roots": duplicate_roots,
            "unknown_dynamic_roots": unknowns, "documentation_validation": validation}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / "src/sm64ap_coin_output_catalog.raw.json")
    args = parser.parse_args()
    catalog = generate()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalog, indent=2) + "\n")
    mismatches = [row for row in catalog["documentation_validation"] if not row["matches"]]
    print(f"wrote {args.output}")
    print(f"roots: {catalog['root_count']}; outputs: {catalog['enumerated_output_count']}; "
          f"coin value: {catalog['enumerated_coin_value']}")
    print(f"dynamic/unknown roots: {len(catalog['unknown_dynamic_roots'])}; "
          f"documentation mismatches: {len(mismatches)}")
    for row in mismatches:
        print(f"  {row['source_category']}: placements {row['enumerated_placements']}/"
              f"{row['expected_placements']}, coins {row['enumerated_coins']}/{row['expected_coins']}")


if __name__ == "__main__":
    main()
