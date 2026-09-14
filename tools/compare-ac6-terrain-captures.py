"""Compare local pinned AC6 terrain captures without running either emulator.

The captures have different camera positions. Equal draw suffixes are evidence
about inputs, not proof that a draw omitted from one view is a runtime bug.
"""

import argparse
import csv
import hashlib
import json
import math
import struct
from pathlib import Path


def read_sized(path, size):
    data = path.read_bytes()
    if len(data) != size:
        raise ValueError(f"{path}: expected {size} bytes, found {len(data)}")
    return data


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def constants(data, register):
    return struct.unpack_from("<4f", data, register * 16)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xenia", type=Path, required=True)
    parser.add_argument("--xeo3", type=Path, required=True)
    parser.add_argument("--readbacks", type=Path, required=True)
    parser.add_argument("--replay", type=Path, required=True)
    parser.add_argument("--draw-index", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Use a new output path to preserve earlier evidence")

    prefix = "4E4D07D1_33646"
    xenia_draw_file = args.xenia / f"{prefix}.draws.tsv"
    with xenia_draw_file.open(newline="") as handle:
        xenia_draws = [
            row for row in csv.DictReader(handle, delimiter="\t")
            if row["vs_hash"] == "042F34FADAD3F370"
            and row["type"] == "draw" and int(row["command"]) < 800
        ]
    with args.draw_index.open(newline="") as handle:
        xeo3_draws = [
            row for row in csv.DictReader(handle, delimiter="\t")
            if row["ApiObjectId"] == "533"
        ]
    if len(xenia_draws) != 52 or len(xeo3_draws) != 49:
        raise ValueError("This comparison requires the pinned 52/49-draw captures")

    xeo3_constants = read_sized(args.xeo3 / "terrain-all-vs-constants.bin", 49 * 4096)
    xeo3_state = read_sized(args.xeo3 / "terrain-all-bufferstate.bin", 49 * 256)
    uploads = read_sized(args.replay / "pso533-vertex-upload.bin", 270336)
    buffers = {
        "coordinate_pattern": (95, read_sized(
            args.readbacks / "terrain-pattern-readback/guest-range.bin", 160)),
        "tile_layout": (94, uploads[:98304]),
        "vertex_colors": (92, uploads[98304:98304 + 169000]),
        "height_map": (93, read_sized(
            args.readbacks / "terrain-height-readback/guest-range.bin", 1250600)),
    }
    comparisons = {}
    for name, (fetch, actual) in buffers.items():
        reference_path = args.xenia / f"{prefix}.terrain-fetch-{fetch}.bin"
        reference = read_sized(reference_path, len(actual))
        comparisons[name] = {
            "bytes": len(actual), "equal": reference == actual,
            "xenia_sha256": digest(reference), "xeo3_sha256": digest(actual),
            "xenia_source": str(reference_path.resolve()),
        }

    heights = struct.unpack(">312650f", buffers["height_map"][1])
    core_registers = (64, 65, 71, 72, 84, 133, 138, 222, 251, 252, 253, 254, 255)
    matches = []
    for index, (expected, actual) in enumerate(zip(xenia_draws[3:], xeo3_draws)):
        reference = read_sized(args.xenia /
            f"{prefix}.terrain-{expected['command']}-vs-constants.bin", 4096)
        current = xeo3_constants[index * 4096:(index + 1) * 4096]
        state = struct.unpack_from("<7I", xeo3_state, index * 256)
        xenia_count = int(expected["vgt_draw_initiator"], 16) >> 16
        xenia_base = int(expected["vgt_indx_offset"], 16)
        xeo3_count = int(actual["VertexCount"]) // 3 + 2
        matches.append({
            "xenia_command": int(expected["command"]),
            "xeo3_global_id": int(actual["GlobalId"]),
            "draw_count_base_equal": (xenia_count, xenia_base) ==
                (xeo3_count, int(actual["StartVertex"])),
            "state_base_matches_draw": state[0] == int(actual["StartVertex"]),
            "packed_ib_desc": f"0x{state[4]:08X}",
            "vfetch_endianness": f"0x{state[3]:08X}",
            "core_constant_differences": [n for n in core_registers
                if reference[n * 16:n * 16 + 16] != current[n * 16:n * 16 + 16]],
            "tile_origin": constants(current, 133),
            "height_layout_base": constants(current, 64),
        })

    first_xenia = read_sized(args.xenia /
        f"{prefix}.terrain-271-vs-constants.bin", 4096)
    result = {
        "schema_version": 1,
        "scope": "Pinned offline inputs; not gameplay or stable-rendering acceptance",
        "buffers": comparisons,
        "height_map": {
            "count": len(heights), "all_finite": all(map(math.isfinite, heights)),
            "min": min(heights), "max": max(heights),
            "hidden_tile_markers_above_9900": sum(h > 9900 for h in heights),
        },
        "xenia_camera": constants(first_xenia, 130),
        "xeo3_camera": constants(xeo3_constants, 130),
        "camera_equal": constants(first_xenia, 130) == constants(xeo3_constants, 130),
        "extra_xenia_prefix": [{
            "command": int(row["command"]),
            "index_count": int(row["vgt_draw_initiator"], 16) >> 16,
            "base_vertex": int(row["vgt_indx_offset"], 16),
        } for row in xenia_draws[:3]],
        "extra_xenia_tile_origin": constants(first_xenia, 133),
        "matched_draws": matches,
        "all_suffix_draws_match": all(row["draw_count_base_equal"] for row in matches),
        "xeo3_constants_sha256": digest(xeo3_constants),
        "xeo3_state_sha256": digest(xeo3_state),
        "warning": "Different cameras prevent classifying the extra prefix as dropped work.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as handle:
        json.dump(result, handle, indent=2, allow_nan=False)
        handle.write("\n")
    print(json.dumps({
        "output": str(args.output.resolve()),
        "all_four_buffers_equal": all(row["equal"] for row in comparisons.values()),
        "all_49_suffix_draws_match": result["all_suffix_draws_match"],
        "core_constant_difference_draws": sum(bool(row["core_constant_differences"])
                                             for row in matches),
        "camera_equal": result["camera_equal"],
    }))


if __name__ == "__main__":
    main()
