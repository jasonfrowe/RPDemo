#!/usr/bin/env python3

import argparse
import os
import re
import sys
from PIL import Image


VALID_SIZES = {8, 16, 32, 64, 128, 256, 512}


# ---------------------------------------------------------------------------
# Color encoding
# ---------------------------------------------------------------------------

def encode_rgb555(r: int, g: int, b: int) -> int:
    """Encode RGB as RP6502 RGB555 with alpha bit set (opaque)."""
    return (((b >> 3) << 11) | ((g >> 3) << 6) | (r >> 3) | (1 << 5))


def encode_rgb555_direct(r: int, g: int, b: int, a: int) -> int:
    """Encode RGBA as RP6502 RGB555; transparent (a < 128) encodes as 0."""
    if a < 128:
        return 0
    return encode_rgb555(r, g, b)


# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------

def infer_bpp_from_image(input_path: str) -> int:
    """Infer output bpp from source image type and color usage."""
    with Image.open(input_path) as im:
        if im.mode == "1":
            return 1

        if im.mode == "P":
            used_colors = len(set(im.getdata()))
            if used_colors <= 2:
                return 1
            if used_colors <= 4:
                return 2
            if used_colors <= 16:
                return 4
            return 8

        # Non-paletted images are treated as truecolor output.
        return 16

def output_path(input_file: str, bpp: int, out_dir: str) -> str:
    stem = os.path.splitext(os.path.basename(input_file))[0]
    return os.path.join(out_dir, f"{stem}_{bpp}bpp.bin")


def palette_bin_path(input_file: str, bpp: int, out_dir: str) -> str:
    stem = os.path.splitext(os.path.basename(input_file))[0]
    return os.path.join(out_dir, f"{stem}_{bpp}bpp_palette.bin")


def palette_header_path(input_file: str, bpp: int, out_dir: str) -> str:
    stem = os.path.splitext(os.path.basename(input_file))[0]
    return os.path.join(out_dir, f"{stem}_{bpp}bpp_palette.h")


def symbol_name(input_file: str, bpp: int) -> str:
    stem = os.path.splitext(os.path.basename(input_file))[0]
    name = re.sub(r"[^0-9A-Za-z_]", "_", stem).lower()
    if not name or name[0].isdigit():
        name = f"palette_{name}"
    return f"{name}_{bpp}bpp"


def ensure_parent(path: str) -> None:
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)


# ---------------------------------------------------------------------------
# Indexed image preparation and packing
# ---------------------------------------------------------------------------

def to_indexed(im: Image.Image, bpp: int) -> tuple:
    """Return (rgba_im, indexed_im) quantized to 2^bpp colors."""
    max_colors = 1 << bpp
    rgba_im = im.convert("RGBA")
    if im.mode == "P":
        use_im = im.copy()
        if use_im.getpalette() is None or max(use_im.getdata()) >= max_colors:
            use_im = rgba_im.convert("P", palette=Image.ADAPTIVE, colors=max_colors)
    else:
        use_im = rgba_im.convert("P", palette=Image.ADAPTIVE, colors=max_colors)
    return rgba_im, use_im


def pack_row(indices: list, bpp: int) -> bytes:
    out = bytearray()
    if bpp == 8:
        out.extend(i & 0xFF for i in indices)
    elif bpp == 4:
        for i in range(0, len(indices), 2):
            out.append(((indices[i] & 0x0F) << 4) | (indices[i + 1] & 0x0F))
    elif bpp == 2:
        for i in range(0, len(indices), 4):
            out.append(
                ((indices[i] & 0x03) << 6)
                | ((indices[i + 1] & 0x03) << 4)
                | ((indices[i + 2] & 0x03) << 2)
                | (indices[i + 3] & 0x03)
            )
    elif bpp == 1:
        for i in range(0, len(indices), 8):
            out.append(
                ((indices[i] & 0x01) << 7)
                | ((indices[i + 1] & 0x01) << 6)
                | ((indices[i + 2] & 0x01) << 5)
                | ((indices[i + 3] & 0x01) << 4)
                | ((indices[i + 4] & 0x01) << 3)
                | ((indices[i + 5] & 0x01) << 2)
                | ((indices[i + 6] & 0x01) << 1)
                | (indices[i + 7] & 0x01)
            )
    else:
        raise ValueError(f"Unsupported bpp for pack_row: {bpp}")
    return bytes(out)


# ---------------------------------------------------------------------------
# Palette export
# ---------------------------------------------------------------------------

def extract_palette(use_im: Image.Image, bpp: int) -> list:
    """RP6502 colors for the image's palette; the PNG's transparent index is 0."""
    max_colors = 1 << bpp
    raw = use_im.getpalette() or []
    transparency = use_im.info.get("transparency")
    colors = []
    for i in range(max_colors):
        base = i * 3
        rgb = tuple(raw[base:base + 3]) if base + 2 < len(raw) else (0, 0, 0)
        if isinstance(transparency, int):
            alpha = 0 if i == transparency else 255
        elif isinstance(transparency, bytes) and i < len(transparency):
            alpha = transparency[i]
        else:
            alpha = 255
        colors.append(encode_rgb555_direct(*rgb, alpha))
    return colors


def write_palette_bin(path: str, colors: list) -> None:
    ensure_parent(path)
    with open(path, "wb") as f:
        for color in colors:
            f.write(color.to_bytes(2, "little"))


def write_palette_header(path: str, symbol: str, colors: list, source_path: str) -> None:
    ensure_parent(path)
    guard = re.sub(r"[^0-9A-Za-z]", "_", symbol.upper()) + "_H"
    with open(path, "w", encoding="ascii") as f:
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"// Palette extracted from {source_path}\n")
        f.write(f"static const uint16_t {symbol}[{len(colors)}] = {{\n")
        for color in colors:
            f.write(f"    0x{color:04X},\n")
        f.write("};\n\n")
        f.write(f"#endif // {guard}\n")


def _write_palette(use_im: Image.Image, bpp: int, input_path: str, out_dir: str) -> None:
    colors = extract_palette(use_im, bpp)
    sym = symbol_name(input_path, bpp)
    bin_p = palette_bin_path(input_path, bpp, out_dir)
    hdr_p = palette_header_path(input_path, bpp, out_dir)
    write_palette_bin(bin_p, colors)
    print(f"Palette:    {bin_p}")
    write_palette_header(hdr_p, sym, colors, input_path)
    print(f"Palette C:  {hdr_p} ({sym})")


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

def convert(
    input_path: str,
    out_dir: str,
    bpp: int,
    mode: str,
    extract_pal: bool,
    inferred_bpp: bool,
) -> int:
    out_path = output_path(input_path, bpp, out_dir)
    with Image.open(input_path) as im:
        width, height = im.size
        rgba_im = im.convert("RGBA")

        print(f"Processing: {input_path} ({width}x{height})")
        mode_line = f"{mode}, {bpp}bpp"
        if inferred_bpp:
            mode_line += " (auto)"
        print(f"Mode:       {mode_line}")
        print(f"Output:     {out_path}")

        ensure_parent(out_path)

        if mode == "bitmap":
            if bpp == 16:
                total_bytes = width * height * 2
                print(f"Total bytes:{total_bytes}")
                with open(out_path, "wb") as f:
                    for y in range(height):
                        for x in range(width):
                            r, g, b, a = rgba_im.getpixel((x, y))
                            f.write(encode_rgb555_direct(r, g, b, a).to_bytes(2, "little"))
            else:
                _, use_im = to_indexed(im, bpp)
                total_bytes = (width * height * bpp + 7) // 8
                print(f"Total bytes:{total_bytes}")
                with open(out_path, "wb") as f:
                    for y in range(height):
                        row = [use_im.getpixel((x, y)) for x in range(width)]
                        f.write(pack_row(row, bpp))
                if extract_pal:
                    _write_palette(use_im, bpp, input_path, out_dir)

        else:  # tile mode
            tile_size = height
            if tile_size not in VALID_SIZES:
                raise ValueError(f"Tile height ({tile_size}) must be one of {sorted(VALID_SIZES)}")
            if width % tile_size != 0:
                raise ValueError(
                    f"Image width ({width}) must be a multiple of tile size ({tile_size})"
                )

            frames = width // tile_size
            if bpp == 16:
                bytes_per_frame = tile_size * tile_size * 2
            else:
                bytes_per_row = (tile_size * bpp + 7) // 8
                bytes_per_frame = bytes_per_row * tile_size

            total_bytes = bytes_per_frame * frames
            print(f"Tile size:  {tile_size}x{tile_size}, {frames} frame(s)")
            print(f"Frame bytes:{bytes_per_frame}")
            print(f"Total bytes:{total_bytes}")

            if bpp == 16:
                with open(out_path, "wb") as f:
                    for frame in range(frames):
                        base_x = frame * tile_size
                        for y in range(tile_size):
                            for x in range(base_x, base_x + tile_size):
                                r, g, b, a = rgba_im.getpixel((x, y))
                                f.write(encode_rgb555_direct(r, g, b, a).to_bytes(2, "little"))
            else:
                _, use_im = to_indexed(im, bpp)
                with open(out_path, "wb") as f:
                    for frame in range(frames):
                        base_x = frame * tile_size
                        for y in range(tile_size):
                            row = [use_im.getpixel((x, y)) for x in range(base_x, base_x + tile_size)]
                            f.write(pack_row(row, bpp))
                if extract_pal:
                    _write_palette(use_im, bpp, input_path, out_dir)

    print("Done.")
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert images to RP6502 binary sprite/tile/bitmap format."
    )
    parser.add_argument("input_file", help="Input image (PNG or similar).")
    parser.add_argument(
        "--bpp",
        type=int,
        choices=[1, 2, 4, 8, 16],
        default=None,
        help="Bits per pixel. If omitted, infer from input image. 16 = direct RGB555 (no palette).",
    )
    parser.add_argument(
        "--mode",
        choices=["tile", "bitmap"],
        default="tile",
        help=(
            "tile (default): horizontal strip of equal-sized frames, height = tile size. "
            "bitmap: full scanline output, no frame splitting."
        ),
    )
    parser.add_argument(
        "--out-dir",
        default="images",
        metavar="DIR",
        help="Output directory for generated files. Default: images",
    )
    parser.add_argument(
        "--extract-palette",
        action="store_true",
        help="Export palette as _palette.bin and _palette.h (indexed bpp only).",
    )

    args = parser.parse_args()

    bpp = args.bpp
    inferred_bpp = False
    if bpp is None:
        bpp = infer_bpp_from_image(args.input_file)
        inferred_bpp = True

    if args.extract_palette and bpp == 16:
        raise SystemExit("Error: --extract-palette is not valid for 16bpp (no palette)")

    try:
        return convert(
            input_path=args.input_file,
            out_dir=args.out_dir,
            bpp=bpp,
            mode=args.mode,
            extract_pal=args.extract_palette,
            inferred_bpp=inferred_bpp,
        )
    except FileNotFoundError:
        print(f"Error: file not found: {args.input_file}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
