#!/usr/bin/env python3
"""Batch-export every music/fur_levels/*.fur to music/*.vgm via Furnace's
headless exporter -- the quick way to pick up hand-edits made in the
Furnace GUI without re-running generate_music.py (which would re-roll the
composition). Resource names match 1:1 (fur_levels/Boss.fur -> music/
Boss.vgm), same as generate_music.py's own --export-vgm -- see tracks.py's
module docstring for why.

Usage:
  python3 tools/export_vgm.py             # export every .fur
  python3 tools/export_vgm.py Boss Title  # export just these (names or paths)
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

FURNACE_BIN = "/Applications/Furnace.app/Contents/MacOS/furnace"
MUSIC_DIR = Path(__file__).resolve().parent.parent / "music"
FUR_DIR = MUSIC_DIR / "fur_levels"


def export_one(fur_path: Path) -> bool:
    vgm_path = MUSIC_DIR / f"{fur_path.stem}.vgm"
    result = subprocess.run(
        [FURNACE_BIN, "-loglevel", "error", "-subsong", "0", "-loops", "1",
         "-vgmout", str(vgm_path), str(fur_path)],
        capture_output=True, text=True, timeout=60,
    )
    text = result.stdout + result.stderr
    real_errors = [
        line for line in text.splitlines()
        if "ERROR" in line and "could not bind text domain" not in line
    ]
    if real_errors or not vgm_path.exists() or vgm_path.stat().st_size == 0:
        print(f"  FAILED: {fur_path.name}")
        for line in text.splitlines():
            if line.strip():
                print(f"    {line}")
        return False

    print(f"  {fur_path.name} -> {vgm_path.relative_to(MUSIC_DIR.parent)} ({vgm_path.stat().st_size} bytes)")
    return True


def main() -> int:
    if not Path(FURNACE_BIN).exists():
        print(f"Furnace not found at {FURNACE_BIN}")
        return 1

    names = sys.argv[1:]
    if names:
        fur_paths = []
        for name in names:
            p = Path(name)
            if p.suffix != ".fur":
                p = FUR_DIR / f"{name}.fur"
            if not p.exists():
                print(f"not found: {p}")
                return 1
            fur_paths.append(p)
    else:
        fur_paths = sorted(FUR_DIR.glob("*.fur"))

    if not fur_paths:
        print(f"no .fur files found in {FUR_DIR}")
        return 1

    ok = True
    for fur_path in fur_paths:
        ok &= export_one(fur_path)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
