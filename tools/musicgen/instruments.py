"""Loads RPTracker's 256-entry OPL2 instrument bank (gm_bank) and converts it
into Furnace OPL instruments.

Parses RPTracker/src/instruments.c directly (regex over the C source) rather
than duplicating the data, so this always reflects whatever patch bank is
actually shipping in RPTracker.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import List

from .furwriter import FurInstrument

# RPDemo and RPTracker are sibling checkouts under rp6502/.
RPTRACKER_INSTRUMENTS_C = Path(__file__).resolve().parents[3] / "RPTracker" / "src" / "instruments.c"

_ENTRY_RE = re.compile(
    r"\[(\d+)\]\s*=\s*\{\s*"
    r"\.m_ave=0x([0-9A-Fa-f]+),\s*\.m_ksl=0x([0-9A-Fa-f]+),\s*\.m_atdec=0x([0-9A-Fa-f]+),\s*"
    r"\.m_susrel=0x([0-9A-Fa-f]+),\s*\.m_wave=0x([0-9A-Fa-f]+),\s*"
    r"\.c_ave=0x([0-9A-Fa-f]+),\s*\.c_ksl=0x([0-9A-Fa-f]+),\s*\.c_atdec=0x([0-9A-Fa-f]+),\s*"
    r"\.c_susrel=0x([0-9A-Fa-f]+),\s*\.c_wave=0x([0-9A-Fa-f]+),\s*"
    r"\.feedback=0x([0-9A-Fa-f]+)\s*\}"
)


@dataclass
class OplPatchRaw:
    index: int
    m_ave: int
    m_ksl: int
    m_atdec: int
    m_susrel: int
    m_wave: int
    c_ave: int
    c_ksl: int
    c_atdec: int
    c_susrel: int
    c_wave: int
    feedback: int


def _extract_block(source: str, start_marker: str, end_marker: str = "};") -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


def load_raw_bank(path: Path = RPTRACKER_INSTRUMENTS_C) -> List[OplPatchRaw]:
    source = path.read_text(encoding="utf-8")
    block = _extract_block(source, "const OPL_Patch gm_bank[256]")

    patches = [None] * 256
    for m in _ENTRY_RE.finditer(block):
        idx = int(m.group(1))
        vals = [int(g, 16) for g in m.groups()[1:]]
        patches[idx] = OplPatchRaw(idx, *vals)

    # C designated-initializer arrays zero-fill any index without an explicit
    # designator; RPTracker's gm_bank has at least one such gap (index 190),
    # so mirror that instead of treating it as a parse failure.
    for i, p in enumerate(patches):
        if p is None:
            patches[i] = OplPatchRaw(i, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    return patches  # type: ignore[return-value]


def load_patch_names(path: Path = RPTRACKER_INSTRUMENTS_C) -> List[str]:
    source = path.read_text(encoding="utf-8")
    block = _extract_block(source, "const char* const patch_names[256]")
    names = re.findall(r'"((?:[^"\\]|\\.)*)"', block)
    if len(names) != 256:
        raise ValueError(f"expected 256 patch names, found {len(names)}")
    return [n.replace('\\"', '"') for n in names]


def load_bank() -> List[FurInstrument]:
    """Returns all 256 patches converted to Furnace OPL instruments, named
    per patch_names[]."""
    raw = load_raw_bank()
    names = load_patch_names()
    result = []
    for p, name in zip(raw, names):
        result.append(
            FurInstrument.from_opl_registers(
                name=name,
                m_ave=p.m_ave, m_ksl=p.m_ksl, m_atdec=p.m_atdec, m_susrel=p.m_susrel, m_wave=p.m_wave,
                c_ave=p.c_ave, c_ksl=p.c_ksl, c_atdec=p.c_atdec, c_susrel=p.c_susrel, c_wave=p.c_wave,
                feedback=p.feedback,
            )
        )
    return result


# Role -> GM program-number ranges (0-based, matching patch_names[0:128]'s
# standard General MIDI layout) used by the composer to pick genre-appropriate
# instruments without needing to hand-curate 256 patches.
#
# Calibrated against the actual Silpheed (1989) DOS score, captured as a
# Roland MT-32 MIDI transcription (studied for structure/instrumentation
# only -- see tools/generate_music.py docstring). That score leans heavily
# on orchestral/mallet-percussion/organ textures layered together (Celesta,
# Glockenspiel, Vibraphone, Dulcimer, Violin/Viola/Contrabass, Rock/Church/
# Reed Organ, Harpsichord/Clavinet), not just synth lead/bass/pad -- these
# pools are widened accordingly.
ROLE_RANGES = {
    "lead": [(80, 87), (24, 31), (40, 40)],         # synth leads, driven guitars, solo violin
    "bass": [(32, 39), (42, 43)],                    # acoustic/finger/pick/slap/synth bass, cello/contrabass
    "pad": [(16, 20), (88, 95), (48, 51)],           # organs, synth pads, string ensembles
    "pluck": [(8, 15), (104, 111)],                  # celesta/bells/marimba/dulcimer, sitar/banjo/kalimba
    "keys": [(0, 1), (4, 7)],                        # piano, electric piano, harpsichord, clavinet
    "brass_stab": [(56, 63)],                        # trumpet..synth brass
    "perc": [(253, 255)],                            # RPTracker's custom kit: Bass Drum/Snare/Hat

    # Narrower, purely-synth pools for the electronic/Kraftwerk-leaning
    # composer (compose.py's motorik drums + sequenced arp/lead) -- deliberately
    # exclude the orchestral/acoustic instruments the ranges above mix in.
    "lead_synth": [(80, 87)],                        # GM Lead 1-8: square, sawtooth, calliope, chiff, charang, voice, fifths, bass+lead
    "bass_synth": [(38, 39)],                        # GM Synth Bass 1 & 2
    "pad_synth": [(88, 95)],                          # GM Pad 1-8: new age, warm, polysynth, choir, bowed, metallic, halo, sweep
}


def role_candidates(role: str) -> List[int]:
    indices: List[int] = []
    for lo, hi in ROLE_RANGES[role]:
        indices.extend(range(lo, hi + 1))
    return indices
