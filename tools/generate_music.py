#!/usr/bin/env python3
"""Generate Furnace (.fur) tracker projects for RPStarHopper's OPL2 music.

Procedurally composes Silpheed/OPL2-inspired tracks for each of the game's
music slots and writes them as Furnace 0.6.8.1-compatible .fur files, using
RPTracker's 256-patch instrument bank. Open the result in Furnace to
audition and tweak by hand; pass --export-vgm to also render straight to
music/RESOURCE.NNN.vgm using Furnace's own headless exporter (equivalent to
File > Export > VGM in the GUI). Without that flag, this tool never touches
music/RESOURCE.NNN.vgm or CMakeLists.txt.

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
"""

from __future__ import annotations

import argparse
import random
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
                  export_vgm: bool = False, vgm_out_dir: Path = DEFAULT_VGM_OUT_DIR) -> Path:
    song = compose.generate_track(name=f"{spec.resource} - {spec.description}", mood=spec.mood, seed=seed, bank=bank)
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{spec.resource}.fur"
    write_fur(str(out_path), song)
    print(f"{spec.resource} ({', '.join(spec.aliases)}) -> {out_path}  [seed={seed}]  {spec.description}")
    _validate_with_furnace(out_path)
    if export_vgm:
        _export_vgm(out_path, vgm_out_dir / f"{spec.resource}.vgm")
    return out_path


def cmd_list() -> None:
    for spec in tracks.TRACKS:
        aliases = ", ".join(spec.aliases)
        print(f"{spec.resource}  ({aliases:<20}) {spec.description}")


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
    parser.add_argument("--list", action="store_true", help="list all tracks and exit")
    args = parser.parse_args()

    if args.list:
        cmd_list()
        return 0

    if not args.track:
        parser.error("--track is required (or use --list)")

    out_dir = Path(args.out_dir)
    vgm_out_dir = Path(args.vgm_out_dir)

    if args.track.lower() == "all":
        if args.seed is not None:
            parser.error("--seed only makes sense with a single --track, not --track all")
        bank = load_bank()
        for spec in tracks.TRACKS:
            generate_one(spec, random.randrange(2**31), bank, out_dir,
                         export_vgm=args.export_vgm, vgm_out_dir=vgm_out_dir)
        return 0

    try:
        spec = tracks.resolve(args.track)
    except KeyError as e:
        parser.error(str(e))
        return 2

    seed = args.seed if args.seed is not None else random.randrange(2**31)
    bank = load_bank()
    generate_one(spec, seed, bank, out_dir, export_vgm=args.export_vgm, vgm_out_dir=vgm_out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
