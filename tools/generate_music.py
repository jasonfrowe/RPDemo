#!/usr/bin/env python3
"""Generate Furnace (.fur) tracker projects for RPStarHopper's OPL2 music.

Procedurally composes OPL2 tracks for each of the game's music slots, in
one of nine styles (--style, see musicgen/compose.py's STYLE_NAMES and
module docstring for the full rundown of each: 0 silpheed/orchestral,
1 kraftwerk/motorik electronic (default), 2 daftpunk/house-funk, 3 trance,
4 bigroom/electro-house, 5 dubstep/halftime, 6 techno/minimal-progressive,
7 synthwave, 8 dnb/jungle), and writes them as Furnace 0.6.8.1-compatible .fur
files, using RPTracker's 256-patch instrument bank. Open the result in
Furnace to audition and tweak by hand; pass --export-vgm to also render
straight to music/RESOURCE.NNN.vgm using Furnace's own headless exporter
(equivalent to File > Export > VGM in the GUI). Without that flag, this
tool never touches music/RESOURCE.NNN.vgm or CMakeLists.txt.

IMPORTANT: exported VGM must be plain OPL2 (YM3812), never OPL3/dual-chip.
RPDemo's VGM player (src/vgm.c) only understands a minimal opcode set and
will desync on OPL3 port-1 writes (0x5E/0x5F) or other commands outside that
set. Every .fur this tool writes is plain OPL2, so this only matters if you
hand-edit a file to add a second chip or switch to OPL3.

Usage:
  python tools/generate_music.py --list
  python tools/generate_music.py --track all
  python tools/generate_music.py --track boss
  python tools/generate_music.py --track boss --seed 1234
  python tools/generate_music.py --track boss --export-vgm

Every run prints a "regenerate:" command with the exact --seed/--vol/--patch
needed to reproduce that roll (instrument picks resolved to their actual
hex values even if you didn't pass --patch) -- copy it and edit one number
to hand-tune a roll you like without re-rolling the rest of the song:
  python tools/generate_music.py --track boss --seed 1234 \\
      --vol 0,0,1,2,-3,-1,2 --patch 52,53,27,FD,FE,FF,5E
--vol/--patch take 7 comma-separated values in lead,arp,bass,pad,kick,
snare,hat order (musicgen/compose.py's ROLE_TO_CHANNEL). --vol values are
integer offsets from each role's base volume
(can be negative; final volume is still clamped into OPL2's 0-63 range).
--patch values are 2-digit hex instrument indices (00-FF) into RPTracker's
256-patch bank, overriding whichever instrument that role would otherwise
have picked. Both only make sense with a single --track, not --track all.
"""

from __future__ import annotations

import argparse
import random
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from musicgen import compose, tracks  # noqa: E402
from musicgen.furwriter import write_fur  # noqa: E402
from musicgen.instruments import load_bank  # noqa: E402

FURNACE_BIN = "/Applications/Furnace.app/Contents/MacOS/furnace"
MUSIC_DIR = Path(__file__).resolve().parent.parent / "music"
DEFAULT_OUT_DIR = MUSIC_DIR / "fur"
DEFAULT_VGM_OUT_DIR = MUSIC_DIR

# lead, arp, bass, pad, kick, snare, hat -- the canonical order --vol/--patch
# values are given/printed in, taken directly from compose.py's role->channel
# mapping so it can never drift out of sync with the actual role set.
ROLE_ORDER = list(compose.ROLE_TO_CHANNEL.keys())


def _parse_vol(spec: str) -> dict:
    parts = [p.strip() for p in spec.split(",")]
    if len(parts) != len(ROLE_ORDER):
        raise ValueError(f"--vol needs {len(ROLE_ORDER)} comma-separated values ({','.join(ROLE_ORDER)}), got {len(parts)}")
    try:
        values = [int(p) for p in parts]
    except ValueError:
        raise ValueError(f"--vol values must be integers, got {spec!r}")
    return dict(zip(ROLE_ORDER, values))


def _parse_patch(spec: str) -> dict:
    parts = [p.strip() for p in spec.split(",")]
    if len(parts) != len(ROLE_ORDER):
        raise ValueError(f"--patch needs {len(ROLE_ORDER)} comma-separated hex values ({','.join(ROLE_ORDER)}), got {len(parts)}")
    try:
        values = [int(p, 16) for p in parts]
    except ValueError:
        raise ValueError(f"--patch values must be 2-digit hex bytes (e.g. FD), got {spec!r}")
    for v in values:
        if not (0 <= v <= 255):
            raise ValueError(f"--patch value {v:#x} out of range 00-FF")
    return dict(zip(ROLE_ORDER, values))


def _validate_with_furnace(path: Path) -> None:
    if not Path(FURNACE_BIN).exists():
        print(f"  (skipping validation: Furnace not found at {FURNACE_BIN})")
        return
    result = subprocess.run(
        [FURNACE_BIN, "-loglevel", "error", "-info", str(path)],
        capture_output=True, text=True, timeout=30,
    )
    text = result.stdout + result.stderr
    real_errors = [
        line for line in text.splitlines()
        if "ERROR" in line and "could not bind text domain" not in line
    ]
    if real_errors or "SONG INFORMATION" not in text:
        print(f"  WARNING: Furnace validation of {path.name} looked wrong:")
        for line in text.splitlines():
            if line.strip():
                print(f"    {line}")
    else:
        print("  validated OK (furnace -info)")


def _export_vgm(fur_path: Path, vgm_path: Path) -> None:
    if not Path(FURNACE_BIN).exists():
        print(f"  WARNING: can't export VGM, Furnace not found at {FURNACE_BIN}")
        return
    vgm_path.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [FURNACE_BIN, "-loglevel", "error", "-subsong", "0", "-loops", "1", "-vgmout", str(vgm_path), str(fur_path)],
        capture_output=True, text=True, timeout=60,
    )
    text = result.stdout + result.stderr
    real_errors = [
        line for line in text.splitlines()
        if "ERROR" in line and "could not bind text domain" not in line
    ]
    if real_errors or not vgm_path.exists() or vgm_path.stat().st_size == 0:
        print(f"  WARNING: VGM export of {fur_path.name} failed:")
        for line in text.splitlines():
            if line.strip():
                print(f"    {line}")
        return
    size = vgm_path.stat().st_size
    print(f"  exported VGM -> {vgm_path} ({size} bytes)")


def generate_one(spec: "tracks.TrackSpec", seed: int, bank, out_dir: Path,
                  export_vgm: bool = False, vgm_out_dir: Path = DEFAULT_VGM_OUT_DIR,
                  vol_overrides: dict = None, patch_overrides: dict = None, style: int = compose.STYLE_KRAFTWERK) -> Path:
    song, ins = compose.generate_track(
        name=f"{spec.resource} - {spec.description}", mood=spec.mood, seed=seed, bank=bank,
        vol_overrides=vol_overrides, patch_overrides=patch_overrides, style=style,
    )
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{spec.resource}.fur"
    write_fur(str(out_path), song)
    print(f"{spec.resource} ({', '.join(spec.aliases)}) -> {out_path}  [seed={seed}, style={style} ({compose.STYLE_NAMES[style]})]  {spec.description}")
    _validate_with_furnace(out_path)
    if export_vgm:
        _export_vgm(out_path, vgm_out_dir / f"{spec.resource}.vgm")

    vol_overrides = vol_overrides or {}
    vol_str = ",".join(str(vol_overrides.get(role, 0)) for role in ROLE_ORDER)
    patch_str = ",".join(f"{ins[role]:02X}" for role in ROLE_ORDER)
    print(f"  regenerate: python3 tools/generate_music.py --track {spec.aliases[0]} --seed {seed} --style {style} "
          f"--vol {vol_str} --patch {patch_str}")
    return out_path


def cmd_list() -> None:
    for spec in tracks.TRACKS:
        aliases = ", ".join(spec.aliases)
        print(f"{spec.resource}  ({aliases:<20}) {spec.description}")


def _fuse_negative_values(argv: list[str]) -> list[str]:
    """`--vol -4,2,...` (space form) reads as `--vol` with no value to argparse,
    since it only recognizes a following token as a value when it doesn't look
    like another option -- and a leading '-' makes it look like one. None of
    this parser's flags start with a digit, so `--vol`/`--patch` followed by a
    token matching -digit... is unambiguously that flag's value; fuse it into
    the `--vol=...` form argparse already handles correctly.
    """
    fused = []
    i = 0
    while i < len(argv):
        tok = argv[i]
        if tok in ("--vol", "--patch") and i + 1 < len(argv) and re.match(r"^-\d", argv[i + 1]):
            fused.append(f"{tok}={argv[i + 1]}")
            i += 2
            continue
        fused.append(tok)
        i += 1
    return fused


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--track", help="track alias/resource id, or 'all' (e.g. boss, level1, RESOURCE.001)")
    parser.add_argument("--seed", type=int, help="RNG seed, for reproducing a specific roll (only valid with a single --track)")
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR), help=f"output directory (default: {DEFAULT_OUT_DIR})")
    parser.add_argument("--export-vgm", action="store_true",
                         help="also render straight to VGM via Furnace's headless exporter "
                              f"(default destination: {DEFAULT_VGM_OUT_DIR}/RESOURCE.NNN.vgm, "
                              "overwriting the existing placeholder)")
    parser.add_argument("--vgm-out-dir", default=str(DEFAULT_VGM_OUT_DIR),
                         help=f"VGM output directory, only used with --export-vgm (default: {DEFAULT_VGM_OUT_DIR})")
    parser.add_argument("--vol", help=f"{len(ROLE_ORDER)} comma-separated integer volume offsets, "
                                       f"{','.join(ROLE_ORDER)} order (only valid with a single --track)")
    parser.add_argument("--patch", help=f"{len(ROLE_ORDER)} comma-separated hex instrument indices (00-FF), "
                                         f"{','.join(ROLE_ORDER)} order (only valid with a single --track)")
    style_choices = ", ".join(f"{k}={v}" for k, v in sorted(compose.STYLE_NAMES.items()))
    parser.add_argument("--style", type=int, default=compose.STYLE_KRAFTWERK,
                         help=f"composition style: {style_choices} (default: {compose.STYLE_KRAFTWERK})")
    parser.add_argument("--list", action="store_true", help="list all tracks and exit")
    args = parser.parse_args(_fuse_negative_values(sys.argv[1:]))

    if args.list:
        cmd_list()
        return 0

    if args.style not in compose.STYLE_NAMES:
        parser.error(f"--style must be one of {style_choices}, got {args.style}")

    if not args.track:
        parser.error("--track is required (or use --list)")

    out_dir = Path(args.out_dir)
    vgm_out_dir = Path(args.vgm_out_dir)

    if args.track.lower() == "all":
        if args.seed is not None:
            parser.error("--seed only makes sense with a single --track, not --track all")
        if args.vol or args.patch:
            parser.error("--vol/--patch only make sense with a single --track, not --track all")
        bank = load_bank()
        for spec in tracks.TRACKS:
            generate_one(spec, random.randrange(2**31), bank, out_dir,
                         export_vgm=args.export_vgm, vgm_out_dir=vgm_out_dir, style=args.style)
        return 0

    try:
        spec = tracks.resolve(args.track)
    except KeyError as e:
        parser.error(str(e))
        return 2

    try:
        vol_overrides = _parse_vol(args.vol) if args.vol else None
        patch_overrides = _parse_patch(args.patch) if args.patch else None
    except ValueError as e:
        parser.error(str(e))
        return 2

    seed = args.seed if args.seed is not None else random.randrange(2**31)
    bank = load_bank()
    generate_one(spec, seed, bank, out_dir, export_vgm=args.export_vgm, vgm_out_dir=vgm_out_dir,
                 vol_overrides=vol_overrides, patch_overrides=patch_overrides, style=args.style)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
