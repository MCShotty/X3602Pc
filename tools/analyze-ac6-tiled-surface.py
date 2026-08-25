#!/usr/bin/env python3

import argparse
import hashlib
from pathlib import Path

from PIL import Image


def tiled_address_2d(x: int, y: int, pitch: int, bytes_per_pixel: int) -> int:
    bytes_log2 = bytes_per_pixel.bit_length() - 1
    pitch_macro_tiles = (pitch + 31) >> 5
    outer_blocks = (((y >> 5) * pitch_macro_tiles) + (x >> 5)) << 6
    inner_blocks = (((y >> 1) & 7) << 3) | (x & 7)
    outer_inner_bytes = (outer_blocks | inner_blocks) << bytes_log2
    bank = (y >> 4) & 1
    pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1)
    return (
        ((y & 1) << 4)
        | (pipe << 6)
        | (bank << 11)
        | (outer_inner_bytes & 0xF)
        | (((outer_inner_bytes >> 4) & 1) << 5)
        | (((outer_inner_bytes >> 5) & 7) << 8)
        | ((outer_inner_bytes >> 8) << 12)
    )


def endian_permutation(mode: int) -> tuple[int, int, int, int]:
    return {
        0: (0, 1, 2, 3),
        1: (1, 0, 3, 2),
        2: (3, 2, 1, 0),
        3: (2, 3, 0, 1),
    }[mode]


def untile(
    source: bytes,
    width: int,
    height: int,
    pitch: int,
    bytes_per_pixel: int,
    endian_mode: int,
) -> bytes:
    if bytes_per_pixel != 4:
        raise ValueError("Only the AC6 32-bpp path is currently supported")
    permutation = endian_permutation(endian_mode)
    output = bytearray(width * height * bytes_per_pixel)
    for y in range(height):
        output_row = y * width * bytes_per_pixel
        for x in range(width):
            source_offset = tiled_address_2d(
                x, y, pitch, bytes_per_pixel
            )
            output_offset = output_row + x * bytes_per_pixel
            if source_offset + bytes_per_pixel > len(source):
                continue
            source_pixel = source[
                source_offset : source_offset + bytes_per_pixel
            ]
            output[output_offset : output_offset + bytes_per_pixel] = bytes(
                source_pixel[index] for index in permutation
            )
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--pitch", type=int, required=True)
    parser.add_argument("--bytes-per-pixel", type=int, default=4)
    parser.add_argument("--endian-mode", type=int, choices=range(4), default=2)
    args = parser.parse_args()

    source = args.source.read_bytes()
    linear = untile(
        source,
        args.width,
        args.height,
        args.pitch,
        args.bytes_per_pixel,
        args.endian_mode,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(linear)
    png_path = args.output.with_suffix(".png")
    Image.frombytes(
        "RGBA", (args.width, args.height), linear
    ).save(png_path)
    print(
        f"raw={args.output} bytes={len(linear)} "
        f"sha256={hashlib.sha256(linear).hexdigest().upper()}"
    )
    print(f"png={png_path}")


if __name__ == "__main__":
    main()
