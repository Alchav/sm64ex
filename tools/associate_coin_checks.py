#!/usr/bin/env python3
"""Associate AP semantic coin outputs with the compiled physical producer catalog.

Unambiguous course/category groups are assigned automatically. Ambiguous groups
must be described by REVIEWED_GROUP_OVERRIDES using physical source hashes; this
keeps descriptive AP names tied to reviewed game objects instead of scan order.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
import types
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AP_ROOT = ROOT.parents[1] / "PycharmProjects/Archipelago"

LEVEL_TO_COURSE = {
    "LEVEL_BOB": "Bob-omb Battlefield", "LEVEL_WF": "Whomp's Fortress",
    "LEVEL_JRB": "Jolly Roger Bay", "LEVEL_CCM": "Cool, Cool Mountain",
    "LEVEL_BBH": "Big Boo's Haunt", "LEVEL_HMC": "Hazy Maze Cave",
    "LEVEL_LLL": "Lethal Lava Land", "LEVEL_SSL": "Shifting Sand Land",
    "LEVEL_DDD": "Dire, Dire Docks", "LEVEL_SL": "Snowman's Land",
    "LEVEL_WDW": "Wet-Dry World", "LEVEL_TTM": "Tall, Tall Mountain",
    "LEVEL_THI": "Tiny-Huge Island", "LEVEL_TTC": "Tick Tock Clock",
    "LEVEL_RR": "Rainbow Ride", "LEVEL_PSS": "The Princess's Secret Slide",
    "LEVEL_SA": "The Secret Aquarium", "LEVEL_WMOTR": "Wing Mario Over the Rainbow",
    "LEVEL_TOTWC": "Tower of the Wing Cap", "LEVEL_VCUTM": "Vanish Cap Under the Moat",
    "LEVEL_COTMC": "Cavern of the Metal Cap", "LEVEL_BITDW": "Bowser in the Dark World",
    "LEVEL_BITFS": "Bowser in the Fire Sea", "LEVEL_BITS": "Bowser in the Sky",
    "LEVEL_CASTLE_GROUNDS": "Castle", "LEVEL_CASTLE": "Castle",
    "LEVEL_CASTLE_COURTYARD": "Castle",
}

CATEGORY_BY_SOURCE_KIND = {
    "yellow": {"yellow_coin", "horizontal_line", "horizontal_ring", "arrow", "vertical_line",
               "vertical_ring", "bobomb", "bully", "fire_piranha_plant", "fly_guy", "goomba",
               "small_breakable_box", "jumping_box", "wooden_post", "bowser_puzzle", "thwomp",
               "three_coin_block", "ten_coin_block", "breakable_coin_box", "swoop", "scuttlebug",
               "skeeter", "snufit", "spindrift", "whomp", "mr_blizzard", "boo"},
    "blue": {"moving_blue_coin", "blue_coin_switch", "boo", "chuckya", "enemy_lakitu", "eyerok",
             "flying_bookend", "koopa_troopa", "moneybag", "mr_i", "monty_mole", "big_bully",
             "piranha_plant", "pokey", "big_boo"},
    "red": {"red_coin"},
    "giant": {"goomba"},
}

# Each value is an ordered tuple of physical source hashes. Output slots are
# consumed in slot order. Giant Goomba groups list one producer per AP pair.
OVERRIDE_PATH = ROOT / "tools/coin_check_association_overrides.json"

# Physical Red Coin order matching CoinChecks.py's semantic output order.
# Courses absent here use source order because their semantic groups already
# match the level data ordering or all eight outputs have identical logic.
REVIEWED_RED_HASH_ORDER: dict[str, tuple[int, ...]] = {
    "Whomp's Fortress": (
        7030571149922220527, 18313336984663760333, 180791707638267664,
        7214758931168833692, 8549472941507367279, 15113789788046324901,
        16792108545258083840, 7504927978984161415,
    ),
    "Jolly Roger Bay": (
        18139714727336836904, 949700393467353693, 3428102740230172561,
        13155619553738441905, 14279045620105517878, 3603229129072008685,
        4449341382558965475, 2889003921391560967,
    ),
    "Big Boo's Haunt": (
        8667433734647278604, 16881097927934041240, 9638851123462181041,
        3281913720013958068, 15814924308723397167, 6567005153366349841,
        13432156730883591705, 16137343123624575578,
    ),
    "Hazy Maze Cave": (
        14845559467394506238, 4218543455029372485, 8036490565236904619,
        17973164593362242457, 2214674137897018179, 17474056326124368734,
        9002886679087978981, 6463008797861104221,
    ),
    "Shifting Sand Land": (
        14293553691176029742, 9414584811979787400, 12943639299368728442,
        701286945924817480, 5565532866131989376, 1518695247012517621,
        13021393807147271157, 12697692883767671072,
    ),
    "Tall, Tall Mountain": (
        4684692001162402332, 2767061278562490405, 16202997476241837105,
        18083974048045418459, 8259156456772215330, 3034589869973125385,
        14467065046542236379, 3360904626153011300,
    ),
    "Cavern of the Metal Cap": (
        64447380047120969, 552365245934888293, 12739120665922369493,
        17533279974305916732, 1034539936773875418, 15718598397290783486,
        8406457127422404935, 4663306691056080145,
    ),
}

# Physical outputs whose semantic numbering follows a reviewed order that is
# not represented by source order. Each token is (physical source hash, slot).
REVIEWED_OUTPUT_ORDER: dict[tuple[str, str], tuple[tuple[int, int], ...]] = {
    ("Tower of the Wing Cap", "red_coin"): tuple((hash_, 0) for hash_ in (
        2647981142279759938, 18145410749851002875,
        9916294783131193146, 2660154997581476067,
        6886722025076844988, 654615134707837313,
        16029695549819568119, 13541641743241052262,
    )),
    ("Tower of the Wing Cap", "totwc_single_yellow_coins"): tuple((hash_, 0) for hash_ in (
        1849913861491102253, 2628286647720010284, 5351088518062821047,
        18331948652615581382, 9747416949232951027, 2367604846651377465,
        16985002078227786033, 13559150810322877781, 9522588669759971532,
        282422605119439394, 7714838953167973092, 400632405561887190,
        12313411549799293874, 4251248539443003240, 14541183058856757648,
    )),
    ("Tower of the Wing Cap", "coin_ring"): tuple(
        (hash_, slot)
        for hash_ in (
            10834815184780141221, 5933618129751255523,
            6344001050721121066, 9321291962489646780,
        )
        for slot in (2, 1, 3, 0, 4, 5, 7, 6)
    ),
}


def load_overrides() -> dict[tuple[str, str], dict]:
    if not OVERRIDE_PATH.exists():
        return {}
    raw = json.loads(OVERRIDE_PATH.read_text())
    return {(entry["course"], entry["source_id"]): entry for entry in raw["groups"]}


def load_ap_catalog():
    package_name = "_sm64_coin_catalog"
    package = types.ModuleType(package_name)
    package.__path__ = [str(AP_ROOT / "worlds/sm64ex_spicy")]
    sys.modules[package_name] = package
    for module_name in ("CoinCheckData", "CoinChecks"):
        path = AP_ROOT / "worlds/sm64ex_spicy" / f"{module_name}.py"
        spec = importlib.util.spec_from_file_location(f"{package_name}.{module_name}", path)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        assert spec.loader
        spec.loader.exec_module(module)
    return sys.modules[f"{package_name}.CoinChecks"]


def root_capacity(root: dict) -> int:
    return len(root["outputs"])


def source_kind(source) -> str:
    if source.source_id == "red_coin":
        return "red"
    layout_kind = source.outputs[0].kind.value
    if "giant_goomba" in source.source_id \
            and any(output.coin_value == 5 for output in source.outputs) \
            and len(source.outputs) % 2 == 0:
        return "giant"
    return layout_kind


def semantic_categories(source) -> set[str]:
    text = f"{source.source_id} {source.label}".lower().replace("-", "_")
    if source.course_name == "Tower of the Wing Cap" and source.source_id == "coin_ring":
        return {"vertical_ring"}
    checks = (
        (("ring_center",), {"yellow_coin"}),
        (("lll_volcano_first_ridge_coin_line",), {"yellow_coin"}),
        (("ddd_sub_area_coin_rings",), {"vertical_ring"}),
        (("low_blue_coins", "red_area_blue_coins", "rr_maze_blue_coin",
          "rr_maze_wall_kick_blue_coins"),
         {"blue_coin_switch"}),
        (("ttm_slide_blue_coins",), {"moving_blue_coin"}),
        (("ttm_top_switch_base_coins", "ttm_top_switch_middle_coins",
          "ttm_top_switch_highest_coin"), {"vertical_line"}),
        (("tiny_piranha_area_plant", "huge_piranha_area_plants"), {"fire_piranha_plant"}),
        (("wmotr_rainbow_coin_rings",), {"vertical_ring"}),
        (("bits_chuckya_goomba",), {"goomba"}),
        (("blue_coin_block",), {"blue_coin_switch"}),
        (("three_coin_block", "3_coin_block", "3_coin block", "3 coin block"), {"three_coin_block"}),
        (("ten_coin_block", "10_coin_block", "10_coin block", "10 coin block"), {"ten_coin_block"}),
        (("breakable_coin_box", "breakable_boxes", "breakable coin boxes"), {"breakable_coin_box"}),
        (("throwable_cork",), {"small_breakable_box"}),
        (("crazy_box",), {"jumping_box"}),
        (("wooden_post", "wooden post"), {"wooden_post"}),
        (("bowser_puzzle",), {"bowser_puzzle"}),
        (("vertical_coin_ring", "vertical ring"), {"vertical_ring"}),
        (("horizontal_ring", "coin_ring", "_ring"), {"horizontal_ring"}),
        (("coin_arrow", "floating_arrow"), {"arrow"}),
        (("vertical_coin_line", "vertical line", "vertical coin line", "vertical coin lines"),
         {"vertical_line"}),
        (("purple_switch_lower_coin_line", "purple_switch_upper_coin_line"), {"vertical_line"}),
        (("horizontal_line", "coin_line", "_line"), {"horizontal_line"}),
        (("bob_omb", "bobomb"), {"bobomb"}),
        (("big_boo",), {"big_boo"}),
        (("bookend",), {"flying_bookend"}),
        (("boo",), {"boo"}),
        (("big_bully",), {"big_bully"}),
        (("bull",), {"bully"}),
        (("chuckya",), {"chuckya"}),
        (("lakitu",), {"enemy_lakitu"}),
        (("fire_piranha",), {"fire_piranha_plant"}),
        (("piranha",), {"piranha_plant"}),
        (("fly_guy",), {"fly_guy"}),
        (("giant_goomba",), {"goomba"}),
        (("goomba",), {"goomba"}),
        (("koopa",), {"koopa_troopa"}),
        (("moneybag",), {"moneybag"}),
        (("mr_blizzard",), {"mr_blizzard"}),
        (("mr_i",), {"mr_i"}),
        (("pokey",), {"pokey"}),
        (("scuttlebug",), {"scuttlebug"}),
        (("skeeter",), {"skeeter"}),
        (("snufit",), {"snufit"}),
        (("spindrift",), {"spindrift"}),
        (("swoop",), {"swoop"}),
        (("whomp",), {"whomp"}),
        (("single", "individual", "_coins", "coin on", "coins on", "coin above", "coins above",
          "coin before", "coins before", "coin after", "coins after", "coin by", "coins by"),
         {"yellow_coin"}),
    )
    for needles, categories in checks:
        if any(needle in text for needle in needles):
            if categories == {"yellow_coin"} and source_kind(source) == "blue":
                continue
            return categories
    kind = source_kind(source)
    return {"yellow_coin"} if kind == "yellow" else CATEGORY_BY_SOURCE_KIND[kind]


def compatible(source, roots: list[dict]) -> list[dict]:
    kind = source_kind(source)
    categories = semantic_categories(source)
    candidates = [root for root in roots if root["source_category"] in categories]
    if kind == "giant":
        candidates = [root for root in candidates if root_capacity(root) == 5]
    elif categories == {"goomba"}:
        candidates = [root for root in candidates if root_capacity(root) == 1]
    return candidates


def unique_capacity_subset(candidates: list[dict], needed: int) -> list[dict] | None:
    # value -> (solution count capped at two, one representative index tuple)
    states: dict[int, tuple[int, tuple[int, ...]]] = {0: (1, ())}
    for index, root in enumerate(candidates):
        capacity = root_capacity(root)
        next_states = dict(states)
        for value, (count, selected) in states.items():
            new_value = value + capacity
            if new_value > needed:
                continue
            old_count, old_selected = next_states.get(new_value, (0, ()))
            next_states[new_value] = (min(2, old_count + count), old_selected or selected + (index,))
        states = next_states
    count, selected = states.get(needed, (0, ()))
    return [candidates[index] for index in selected] if count == 1 else None


def first_capacity_subset(candidates: list[dict], needed: int) -> list[dict] | None:
    states: dict[int, tuple[int, ...]] = {0: ()}
    for index, root in enumerate(candidates):
        capacity = root_capacity(root)
        for value, selected in list(states.items())[::-1]:
            new_value = value + capacity
            if new_value <= needed and new_value not in states:
                states[new_value] = selected + (index,)
    selected = states.get(needed)
    return None if selected is None else [candidates[index] for index in selected]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", type=Path, default=ROOT / "src/sm64ap_coin_output_catalog.raw.json")
    parser.add_argument("--report", type=Path, default=ROOT / "src/sm64ap_coin_association_report.json")
    parser.add_argument("--inc", type=Path, default=ROOT / "src/sm64ap_coin_check_catalog.inc")
    args = parser.parse_args()

    ap = load_ap_catalog()
    reviewed_overrides = load_overrides()
    raw = json.loads(args.raw.read_text())
    roots_by_course: dict[str, list[dict]] = defaultdict(list)
    for root in raw["roots"]:
        course = LEVEL_TO_COURSE.get(root["level"])
        if course:
            roots_by_course[course].append(root)

    sources_by_course: dict[str, list] = defaultdict(list)
    for source in ap.coin_source_catalog:
        sources_by_course[source.course_name].append(source)

    associations: dict[tuple[str, str], dict] = {}
    unresolved = []
    used_outputs: set[tuple[int, int]] = set()
    used_giant_roots: set[int] = set()
    for course, sources in sources_by_course.items():
        roots = roots_by_course[course]
        # Resolve categories represented by exactly one semantic group and an
        # exact physical output count. More specific ambiguous cases are overrides.
        by_kind: dict[str, list] = defaultdict(list)
        for source in sources:
            by_kind[source_kind(source)].append(source)
        pending = list(sources)
        made_progress = True
        while made_progress:
            made_progress = False
            for source in pending.copy():
                kind = source_kind(source)
                key = (course, source.source_id)
                if key in reviewed_overrides:
                    by_hash = {int(root["physical_source_hash"]): root for root in roots}
                    override = reviewed_overrides[key]
                    if kind == "giant":
                        hashes = tuple(int(root["physical_source_hash"]) for root in override["physical_roots"])
                        if any(hash_ not in by_hash for hash_ in hashes):
                            raise ValueError(f"override {key} references a missing Giant Goomba root")
                        associations[key] = {"giant_roots": [by_hash[hash_] for hash_ in hashes]}
                        used_giant_roots.update(hashes)
                    else:
                        selected = []
                        for token in override["physical_outputs"]:
                            hash_, slot = int(token["physical_source_hash"]), token["slot"]
                            root = by_hash.get(hash_)
                            if root is None or slot >= len(root["outputs"]):
                                raise ValueError(f"override {key} references missing output {hash_}/{slot}")
                            selected.append((root, slot))
                            used_outputs.add((hash_, slot))
                        reviewed_order = REVIEWED_OUTPUT_ORDER.get(key)
                        if reviewed_order is not None:
                            order = {token: index for index, token in enumerate(reviewed_order)}
                            selected_tokens = {
                                (int(root["physical_source_hash"]), slot) for root, slot in selected
                            }
                            if set(order) != selected_tokens:
                                raise ValueError(f"reviewed output order does not match override outputs for {key}")
                            selected.sort(key=lambda token: order[(int(token[0]["physical_source_hash"]), token[1])])
                        associations[key] = {"outputs": selected}
                    pending.remove(source)
                    made_progress = True
                    continue
                candidates = [root for root in compatible(source, roots)]
                needed_slots = len(source.outputs) if kind != "giant" else len(source.outputs) // 2
                if kind == "giant":
                    available = [root for root in candidates
                                 if int(root["physical_source_hash"]) not in used_giant_roots]
                    selected = unique_capacity_subset(available, needed_slots * 5)
                    unique = selected
                else:
                    available_outputs = [(root, output["slot"]) for root in candidates for output in root["outputs"]
                                         if (int(root["physical_source_hash"]), output["slot"]) not in used_outputs]
                    unique = available_outputs if len(available_outputs) == needed_slots else None
                if unique is not None:
                    if kind == "giant":
                        associations[key] = {"giant_roots": unique}
                        used_giant_roots.update(int(root["physical_source_hash"]) for root in unique)
                    else:
                        associations[key] = {"outputs": unique}
                        used_outputs.update((int(root["physical_source_hash"]), slot) for root, slot in unique)
                    pending.remove(source)
                    made_progress = True
        for source in pending:
            kind = source_kind(source)
            key = (course, source.source_id)
            candidates = [root for root in compatible(source, roots)]
            unresolved.append({
                        "course": course, "source_id": source.source_id, "label": source.label,
                        "kind": kind, "ap_output_count": len(source.outputs),
                        "candidate_output_count": sum(root_capacity(root) for root in candidates),
                        "candidates": [{k: root[k] for k in ("physical_source_hash", "area", "behavior",
                                                              "source_category", "position", "origin", "outputs")}
                                       for root in candidates],
                    })

    rows = []
    semantic_rows: set[tuple[int, int, str, int]] = set()
    mapped_ids = set()
    mapped_physical_outputs: set[tuple[int, int]] = set()
    for source in ap.coin_source_catalog:
        key = (source.course_name, source.source_id)
        association = associations.get(key)
        if association is None:
            continue
        if "giant_roots" in association:
            if len(source.outputs) != len(association["giant_roots"]) * 2:
                raise ValueError(f"Giant Goomba cardinality mismatch for {key}")
            for index, root in enumerate(association["giant_roots"]):
                yellow, blue = source.outputs[index * 2:index * 2 + 2]
                if yellow.coin_value != 1 or blue.coin_value != 5 or sum(x["value"] for x in root["outputs"]) != 5:
                    raise ValueError(f"Giant Goomba value mismatch for {key}")
                rows.extend((("output", int(root["physical_source_hash"]), 0, yellow.location_id, yellow.location_name),
                             ("completion", int(root["physical_source_hash"]), 5, blue.location_id, blue.location_name)))
                semantic_rows.update(
                    (int(root["physical_source_hash"]), slot, source.source_id, 1)
                    for slot in range(5)
                )
                for method in yellow.source_methods:
                    semantic_rows.add((int(root["physical_source_hash"]), 0, method, 1))
                for method in blue.source_methods:
                    semantic_rows.update(
                        (int(root["physical_source_hash"]), slot, method, 1)
                        for slot in range(5)
                    )
                physical_key = (int(root["physical_source_hash"]), 0)
                if physical_key in mapped_physical_outputs:
                    raise ValueError(f"physical Giant Goomba output mapped twice: {physical_key}")
                mapped_physical_outputs.add(physical_key)
                mapped_ids.update((yellow.location_id, blue.location_id))
        else:
            if len(source.outputs) != len(association["outputs"]):
                raise ValueError(f"output cardinality mismatch for {key}")
            for output, (root, slot) in zip(source.outputs, association["outputs"]):
                physical_value = root["outputs"][slot]["value"]
                if physical_value != output.coin_value:
                    raise ValueError(f"value mismatch for {output.location_name}: AP {output.coin_value}, "
                                     f"physical {physical_value}")
                rows.append(("output", int(root["physical_source_hash"]), slot,
                             output.location_id, output.location_name))
                for source_id in dict.fromkeys((source.source_id, *output.source_methods)):
                    semantic_rows.add((
                        int(root["physical_source_hash"]), slot, source_id, output.coin_value))
                physical_key = (int(root["physical_source_hash"]), slot)
                if physical_key in mapped_physical_outputs:
                    raise ValueError(f"physical output mapped twice: {physical_key}")
                mapped_physical_outputs.add(physical_key)
                mapped_ids.add(output.location_id)
    if not unresolved:
        expected_ids = {output.location_id for output in ap.coin_output_catalog}
        if mapped_ids != expected_ids or len(rows) != len(ap.coin_output_catalog):
            raise ValueError(f"catalog is not bijective: rows={len(rows)}, mapped={len(mapped_ids)}, "
                             f"expected={len(expected_ids)}")
        lines = ["// Generated by tools/associate_coin_checks.py. Do not edit by hand.", ""]
        for kind, hash_, slot_or_count, location_id, name in rows:
            macro = "COIN_OUTPUT" if kind == "output" else "COIN_COMPLETION"
            lines.append(f"{macro}(UINT64_C({hash_}), {slot_or_count}, {location_id}) // {name}")
        lines.append("")
        for hash_, slot, source_id, value in sorted(semantic_rows):
            lines.append(
                f'COIN_SEMANTIC_SOURCE(UINT64_C({hash_}), {slot}, "{source_id}", {value})')
        args.inc.write_text("\n".join(lines) + "\n")

    report = {
        "ap_output_count": len(ap.coin_output_catalog),
        "associated_group_count": len(associations),
        "unresolved_group_count": len(unresolved),
        "mapped_output_count": len(rows),
        "mapped_unique_location_id_count": len(mapped_ids),
        "mapped_unique_physical_output_count": len(mapped_physical_outputs),
        "unresolved": unresolved,
    }
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(f"associated groups: {len(associations)}; unresolved: {len(unresolved)}")
    print(f"wrote {args.report}")
    return 1 if unresolved else 0


def propose_overrides() -> int:
    ap = load_ap_catalog()
    raw = json.loads((ROOT / "src/sm64ap_coin_output_catalog.raw.json").read_text())
    roots_by_course: dict[str, list[dict]] = defaultdict(list)
    for root in raw["roots"]:
        course = LEVEL_TO_COURSE.get(root["level"])
        if course:
            roots_by_course[course].append(root)
    groups = []
    used_outputs: set[tuple[int, int]] = set()
    used_giant_roots: set[int] = set()
    # Keep semantic source order and physical source order. Exact producer
    # categories and capacities constrain every selection; the resulting
    # coordinates/origins are persisted for line-by-line review.
    for source in ap.coin_source_catalog:
        candidates = compatible(source, roots_by_course[source.course_name])
        needed = len(source.outputs) if source_kind(source) != "giant" else len(source.outputs) // 2
        entry = {
            "course": source.course_name, "source_id": source.source_id, "label": source.label,
        }
        if source_kind(source) == "giant":
            available = [root for root in candidates
                         if int(root["physical_source_hash"]) not in used_giant_roots]
            selected = available[:needed]
            if len(selected) != needed:
                raise ValueError(f"cannot propose Giant Goombas for {source.course_name}/{source.source_id}")
            used_giant_roots.update(int(root["physical_source_hash"]) for root in selected)
            entry["physical_roots"] = [{k: root[k] for k in ("physical_source_hash", "area", "behavior",
                                                               "source_category", "position", "origin", "outputs")}
                                       for root in selected]
        else:
            available = [(root, output["slot"]) for root in candidates for output in root["outputs"]
                         if (int(root["physical_source_hash"]), output["slot"]) not in used_outputs]
            reviewed_order = REVIEWED_OUTPUT_ORDER.get((source.course_name, source.source_id))
            if reviewed_order is not None:
                order = {token: index for index, token in enumerate(reviewed_order)}
                if set(order) != {
                    (int(root["physical_source_hash"]), slot) for root, slot in available
                }:
                    raise ValueError(f"reviewed output order does not match available outputs for "
                                     f"{source.course_name}/{source.source_id}")
                available.sort(key=lambda token: order[(int(token[0]["physical_source_hash"]), token[1])])
            elif source.source_id == "red_coin" and source.course_name in REVIEWED_RED_HASH_ORDER:
                order = {hash_: index for index, hash_ in enumerate(REVIEWED_RED_HASH_ORDER[source.course_name])}
                available.sort(key=lambda token: order[int(token[0]["physical_source_hash"])])
            selected = available[:needed]
            if len(selected) != needed:
                raise ValueError(f"cannot propose {source.course_name}/{source.source_id}: need {needed}, "
                                 f"have {len(available)}")
            used_outputs.update((int(root["physical_source_hash"]), slot) for root, slot in selected)
            entry["physical_outputs"] = [
                {"physical_source_hash": root["physical_source_hash"], "slot": slot,
                 "value": root["outputs"][slot]["value"], "area": root["area"],
                 "behavior": root["behavior"], "source_category": root["source_category"],
                 "position": root["position"], "origin": root["origin"]}
                for root, slot in selected]
        groups.append(entry)
    OVERRIDE_PATH.write_text(json.dumps({"schema_version": 1, "groups": groups}, indent=2) + "\n")
    print(f"wrote {OVERRIDE_PATH} with {len(groups)} explicit groups")
    return 0


if __name__ == "__main__":
    if "--propose-overrides" in sys.argv:
        sys.argv.remove("--propose-overrides")
        raise SystemExit(propose_overrides())
    raise SystemExit(main())
