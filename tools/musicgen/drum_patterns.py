"""One-bar (16-step) drum patterns transcribed from *Pocket Operations*
(Teenage Engineering's drum-machine-pattern reference book), used to give
the electronic-style composer's drums real variety instead of one
hand-rolled template repeated for the whole song.

Steps are 1-indexed in the book (matching its own grid notation) and
converted here to 0-indexed rows on the composer's 16-row-per-bar grid
(`compose.ROWS_PER_BAR`). The book prints several distinct percussion
voices per pattern (BD, SN, CL, RS, CH, OH, CY, SH, plus toms/cowbell we
don't use at all) but OPL2 only gives this composer one fixed patch each
for kick/snare/hat -- there's no separate open/closed hi-hat voice to
preserve the distinction on real hardware. So every pattern here only
keeps three rows: `kick` (from the book's BD), `snare` (SN and/or CL/RS,
merged), and `hat` (CH/OH/CY/SH, merged) -- any tom/cowbell/accent row the
original pattern had is simply dropped.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import FrozenSet, List


@dataclass(frozen=True)
class DrumPattern:
    name: str
    kick: List[int]
    snare: List[int]
    hat: List[int]
    tags: FrozenSet[str] = field(default_factory=frozenset)


def _steps(*ones_indexed: int) -> List[int]:
    return [s - 1 for s in ones_indexed]


# "motorik": four-on-the-floor-adjacent, steady, machine-like -- the
# Kraftwerk-leaning style's pool.
# "groove": syncopated house/funk/breakbeat -- the Daft Punk-leaning
# style's pool. A few patterns are basic/steady enough to suit both.
PATTERNS: List[DrumPattern] = [
    DrumPattern("one_and_seven", kick=_steps(1, 7), snare=_steps(5, 13),
                hat=_steps(1, 3, 5, 7, 9, 11, 13, 15), tags=frozenset({"motorik"})),
    DrumPattern("boots_n_cats", kick=_steps(1, 9), snare=_steps(5, 13),
                hat=_steps(1, 3, 5, 7, 9, 11, 13, 15), tags=frozenset({"motorik"})),
    DrumPattern("tiny_house", kick=_steps(1, 5, 9, 13), snare=[],
                hat=_steps(3, 7, 11, 15), tags=frozenset({"motorik", "groove"})),
    DrumPattern("techno", kick=_steps(1, 5, 9, 13, 15), snare=_steps(5, 13),
                hat=_steps(3, 7, 10, 11, 15), tags=frozenset({"motorik"})),
    DrumPattern("new_wave", kick=_steps(1, 7, 9, 10), snare=_steps(5, 13),
                hat=_steps(*range(1, 17)), tags=frozenset({"motorik"})),
    DrumPattern("house", kick=_steps(1, 5, 9, 13), snare=_steps(5, 13),
                hat=_steps(1, 3, 7, 11, 15), tags=frozenset({"motorik", "groove"})),
    DrumPattern("house_2", kick=_steps(1, 5, 9, 13), snare=_steps(5, 13),
                hat=_steps(*range(1, 17)), tags=frozenset({"motorik", "groove"})),
    DrumPattern("french_house", kick=_steps(1, 5, 9, 13), snare=_steps(5, 13),
                hat=_steps(*range(1, 17)), tags=frozenset({"groove"})),
    DrumPattern("brit_house", kick=_steps(1, 5, 9, 13), snare=_steps(5, 13),
                hat=_steps(*range(1, 17)), tags=frozenset({"groove"})),
    DrumPattern("dirty_house", kick=_steps(1, 3, 5, 9, 11, 13, 16), snare=_steps(5, 13),
                hat=_steps(3, 11, 15, 16), tags=frozenset({"groove"})),
    DrumPattern("amen_break", kick=_steps(1, 3, 11, 12), snare=_steps(5, 8, 10, 13, 16),
                hat=_steps(1, 3, 5, 7, 9, 11, 13, 15), tags=frozenset({"groove"})),
    DrumPattern("funky_drummer", kick=_steps(1, 3, 7, 11, 14), snare=_steps(5, 8, 10, 12, 13, 16),
                hat=_steps(*range(1, 17)), tags=frozenset({"groove"})),
    DrumPattern("groove_me", kick=_steps(1, 4, 5, 8, 9, 10, 12, 14, 16), snare=_steps(5, 13),
                hat=_steps(1, 3, 5, 7, 9, 11, 13, 15), tags=frozenset({"groove"})),
    DrumPattern("the_big_beat", kick=_steps(1, 4, 7, 9), snare=_steps(5, 13),
                hat=_steps(5, 13), tags=frozenset({"groove"})),
    DrumPattern("boom_bap", kick=_steps(1, 3, 6, 10, 14), snare=_steps(3, 7, 11, 15),
                hat=_steps(*range(1, 17)), tags=frozenset({"groove"})),

    # "halftime": kick/snare feel half the speed of the hats -- snare/clap
    # lands only on beat 3, not the usual 2-and-4 -- against dense,
    # machine-gun 16th hats. The Skrillex-leaning dubstep/brostep style's pool.
    DrumPattern("dubstep_halftime", kick=_steps(1, 7), snare=_steps(9),
                hat=_steps(1, 4, 6, 9, 11, 12, 15), tags=frozenset({"halftime"})),
    DrumPattern("brostep_stutter", kick=_steps(1, 4, 11), snare=_steps(9),
                hat=_steps(*range(1, 17)), tags=frozenset({"halftime"})),
]


def patterns_for(tag: str) -> List[DrumPattern]:
    matches = [p for p in PATTERNS if tag in p.tags]
    if not matches:
        raise ValueError(f"no drum patterns tagged {tag!r}")
    return matches
