"""Compare pinned XeO3 terrain post-VS stream output to a float32 CPU model.

This diagnoses the captured shader, not universal Xenos floating-point behavior.
The input is five little-endian floats per expanded vertex: position and clip.
"""

import argparse
import hashlib
import json
import math
import struct
from functools import lru_cache
from pathlib import Path


def f32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


def mad(left, right, addend):
    return f32(left * right + addend)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vertices", required=True, type=Path)
    parser.add_argument("--draw", required=True, type=int, choices=(999, 1000, 1001))
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--require-match", action="store_true",
                        help="Exit nonzero if positions or clipping flags disagree")
    parser.add_argument("--capture-root", type=Path,
                        default=Path("out/pix/readback-revalidation"))
    parser.add_argument("--replay", type=Path,
                        default=Path("out/pix/ac6-vpos-exposure-gameplay-export-build"))
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Use a new result path to preserve earlier evidence")
    draw_index = args.draw - 999
    vertex_count, expected_base = ((522, 0), (8838, 160), (24414, 2840))[draw_index]
    data = args.vertices.read_bytes()
    if len(data) != vertex_count * 20:
        raise ValueError("Stream output length does not match the pinned draw")
    all_constants = (args.capture_root / "terrain-all-inputs/terrain-all-vs-constants.bin").read_bytes()
    cb = all_constants[draw_index * 4096:(draw_index + 1) * 4096]
    c = lambda index: struct.unpack_from("<4f", cb, index * 16)
    states = (args.capture_root / "terrain-all-inputs/terrain-all-bufferstate.bin").read_bytes()
    state = struct.unpack_from("<7I", states, draw_index * 256)
    if state != (expected_base, 0, 0, 0xAA, 0x30105, 0xFFFF, 0):
        raise ValueError(f"Unexpected pinned buffer state: {state}")
    pattern_bytes = (args.capture_root / "terrain-pattern-readback/guest-range.bin").read_bytes()
    pattern = list(struct.iter_unpack(">HH", pattern_bytes))
    upload = (args.replay / "pso533-vertex-upload.bin").read_bytes()
    records = list(struct.iter_unpack(">4f", upload[:98304]))
    height_bytes = (args.capture_root / "terrain-height-readback/guest-range.bin").read_bytes()
    heights = struct.unpack(">312650f", height_bytes)
    index_bytes = (args.replay / "pso533-index-upload.bin").read_bytes()
    indices = struct.unpack(">12288H", index_bytes)
    reset = state[5]
    bias = f32(0.00025)

    @lru_cache(maxsize=None)
    def position(vid):
        # Follow the native HLSL assignments, including its signed divisor,
        # fractional-index calculation and the existing vfetch bias.
        x = f32(vid + c(254)[0])
        y = f32(vid + c(254)[1])
        row = math.trunc(f32(y * c(254)[2]))
        sign_probe = f32(x * c(254)[3])
        divisor = mad(1.0 if sign_probe >= -sign_probe else 0.0,
                      c(251)[0], c(253)[2])
        product = f32(x * f32(1.0 / divisor))
        fraction = f32(product - math.floor(product))
        pattern_index = int(f32(f32(divisor * fraction) + bias))
        record_index = int(f32(f32(row + c(64)[1]) + bias))
        if not 0 <= pattern_index < len(pattern) or not 0 <= record_index < len(records):
            raise ValueError(f"Vertex fetch out of range at VID {vid}")
        # Endian mode 2 reverses the packed ushort components. The subsequent
        # .zy swizzle reverses them again before applying the grid offsets.
        px, pz = pattern[pattern_index]
        rx, rz, _, _ = records[record_index]
        gx = mad(rx, c(252)[1], px)
        gz = mad(rz, c(252)[1], pz)
        hx = mad(rx, c(252)[1], c(252)[0])
        hz = mad(rz, c(252)[1], c(252)[0])
        height_index = int(f32(f32(mad(gz, c(252)[2], gx) + c(64)[0]) + bias))
        flag_index = int(f32(f32(mad(hz, c(252)[2], hx) + c(64)[0]) + bias))
        if not 0 <= height_index < len(heights) or not 0 <= flag_index < len(heights):
            raise ValueError(f"Height fetch out of range at VID {vid}")
        if heights[flag_index] > c(251)[3]:
            world = c(133)[:3]
        else:
            world = (
                f32(f32(f32(f32(gx + c(253)[1]) * c(222)[0]) * c(255)[0]) + c(133)[0]),
                f32(heights[height_index] + c(133)[1]),
                f32(f32(f32(f32(gz + c(253)[1]) * c(222)[1]) * c(255)[0]) + c(133)[2]),
            )
        projected = tuple(mad(world[0], c(218)[j],
                              mad(world[1], c(219)[j],
                                  mad(world[2], c(220)[j], c(221)[j])))
                          for j in range(4))
        return projected, {"pattern_index": pattern_index, "record_index": record_index,
                           "height_index": height_index, "world": world}

    max_error = [0.0] * 4
    mismatches = []
    mismatch_count = 0
    clip_mismatches = 0
    rejected_vertices = 0
    segment_start = 0
    observed = list(struct.iter_unpack("<5f", data))
    for primitive_index in range(vertex_count // 3):
        if primitive_index > 0 and indices[primitive_index - 1] == reset:
            segment_start = primitive_index
        positions = (segment_start, primitive_index + 1, primitive_index + 2)
        triangle_indices = tuple(indices[p] for p in positions)
        rejected = indices[primitive_index] == reset or reset in triangle_indices
        for corner in range(3):
            host_index = primitive_index * 3 + corner
            vid = expected_base + (0 if rejected else triangle_indices[corner])
            predicted, details = position(vid)
            actual = observed[host_index]
            rejected_vertices += int(rejected)
            clip_mismatches += int(actual[4] != (-1.0 if rejected else 0.0))
            errors = [abs(a - b) if math.isfinite(a) else math.inf
                      for a, b in zip(actual[:4], predicted)]
            max_error = [max(a, b) for a, b in zip(max_error, errors)]
            different = any(error > 0.005 + abs(expected) * 0.000002
                            for error, expected in zip(errors, predicted))
            mismatch_count += int(different)
            if different and len(mismatches) < 16:
                mismatches.append({"host_local_index": host_index, "guest_vid": vid,
                    "expected": predicted,
                    "actual": [v if math.isfinite(v) else str(v) for v in actual],
                    "rejected": rejected, **details})
    result = {
        "draw": args.draw, "vertices": vertex_count, "base_vertex": expected_base,
        "stream_output_sha256": hashlib.sha256(data).hexdigest().upper(),
        "expected_rejected_vertices": rejected_vertices,
        "clip_flag_mismatches": clip_mismatches,
        "position_mismatch_count": mismatch_count,
        "max_absolute_error": [v if math.isfinite(v) else str(v) for v in max_error],
        "first_mismatches": mismatches,
        "model": "Native shader float32 inputs and fused-mad projection; diagnostic tolerance only",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as handle:
        json.dump(result, handle, indent=2, allow_nan=False)
        handle.write("\n")
    print(json.dumps({k: v for k, v in result.items() if k != "first_mismatches"}))
    if args.require_match and (mismatch_count or clip_mismatches):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
