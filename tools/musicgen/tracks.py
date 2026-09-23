"""Registry of the game's music slots and the mood each one should evoke.

Mapping confirmed by reading RPDemo's source directly:
  - src/music.c              default/title track
  - src/gameplay.c           track_for_level() -- one distinct track per
                              level 1-7; level 8+ falls back to Level_07
                              (there's no higher-level track, so the last
                              one just keeps playing)
  - src/gameplay.c           gameplay_reset_to_title_scene() -> title track
  - src/gameplay_boss.c      BOSS_STAGE_MUSIC_TRACK (src/constants.h)
  - src/level_bonus.c        bonus round
  - src/gameplay_game_over.c game over (loss) / victory (win)

`resource` is both the ROM asset name game code opens via
"ROM:<resource>.vgm" (see CMakeLists.txt's
rp6502_asset(... <resource>.vgm music/<resource>.vgm) lines)
and this tool's output filename stem (music/<resource>.vgm,
music/fur_levels/<resource>.fur) -- so it doubles as the single source of
truth tying the generator, the build, and the game's `music_set_track()`
calls together. Previously these were opaque "RESOURCE.NNN" ids (a
leftover of an older, smaller track count that reused one track across two
levels, e.g. level 1 & 5 shared RESOURCE.005) -- renamed to these
self-documenting names once every level got its own distinct track.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class MoodPreset:
    """Musical parameters for a track's mood. Semitone 0 = C.

    `intensity` (0.0-1.0) places the track on the game's arc and drives
    compose.py's choice of chord progressions (title/early levels stay
    simple and diatonic; boss fights and late levels pull from a pool of
    more harmonically restless progressions) and whether the contrasting
    B-theme/bridge modulates to a different key.
    """

    key_root: int
    scale: str
    bpm: float
    intensity: float
    target_seconds: float = 170.0
    lead_octave: int = 5
    arp_octave: int = 4
    bass_octave: int = 2
    use_drums: bool = True


@dataclass
class TrackSpec:
    resource: str  # e.g. "Level_01" -- ROM asset name and output filename stem
    description: str
    aliases: list
    mood: MoodPreset


TRACKS: list = [
    TrackSpec(
        resource="Title",
        description="Title / attract screen",
        aliases=["title"],
        mood=MoodPreset(key_root=0, scale="major", bpm=124, intensity=0.10, lead_octave=5),
    ),
    TrackSpec(
        resource="Level_01",
        description="Level 1",
        aliases=["level1"],
        mood=MoodPreset(key_root=9, scale="mixolydian", bpm=132, intensity=0.20, lead_octave=5),
    ),
    TrackSpec(
        resource="Level_02",
        description="Level 2",
        aliases=["level2"],
        mood=MoodPreset(key_root=2, scale="dorian", bpm=128, intensity=0.30, lead_octave=5),
    ),
    TrackSpec(
        resource="Level_03",
        description="Level 3",
        aliases=["level3"],
        mood=MoodPreset(key_root=7, scale="mixolydian", bpm=134, intensity=0.40, lead_octave=5),
    ),
    TrackSpec(
        resource="Level_04",
        description="Level 4",
        aliases=["level4"],
        mood=MoodPreset(key_root=4, scale="phrygian", bpm=126, intensity=0.50, lead_octave=4),
    ),
    TrackSpec(
        resource="Level_05",
        description="Level 5",
        aliases=["level5"],
        mood=MoodPreset(key_root=0, scale="minor", bpm=130, intensity=0.60, lead_octave=5),
    ),
    TrackSpec(
        resource="Level_06",
        description="Level 6",
        aliases=["level6"],
        mood=MoodPreset(key_root=5, scale="dorian", bpm=132, intensity=0.68, lead_octave=5),
    ),
    TrackSpec(
        resource="Level_07",
        description="Level 7 (also the level 8+ fallback -- there's no higher-level track)",
        aliases=["level7"],
        mood=MoodPreset(key_root=11, scale="minor", bpm=136, intensity=0.75, lead_octave=5),
    ),
    TrackSpec(
        resource="Boss",
        description="Boss battle",
        aliases=["boss"],
        mood=MoodPreset(key_root=9, scale="minor", bpm=140, intensity=0.95, lead_octave=4, arp_octave=3),
    ),
    TrackSpec(
        resource="Bonus",
        description="Bonus / reward round",
        aliases=["bonus", "reward"],
        mood=MoodPreset(key_root=5, scale="major", bpm=128, intensity=0.15, lead_octave=5),
    ),
    TrackSpec(
        resource="Gameover",
        description="Game over (loss only -- see Victory for the win)",
        aliases=["gameover"],
        mood=MoodPreset(key_root=2, scale="minor", bpm=100, intensity=0.55, target_seconds=80.0,
                         lead_octave=4, use_drums=False),
    ),
    TrackSpec(
        resource="Victory",
        description="Victory / You Win ending (level 7 boss defeated)",
        aliases=["victory", "win"],
        mood=MoodPreset(key_root=0, scale="major", bpm=152, intensity=0.75, target_seconds=90.0,
                         lead_octave=5, arp_octave=5),
    ),
]

_ALIAS_INDEX = {}
for _t in TRACKS:
    _ALIAS_INDEX[_t.resource] = _t
    for _a in _t.aliases:
        _ALIAS_INDEX[_a] = _t


def resolve(name: str) -> TrackSpec:
    key = name.strip().lower()
    for candidate_key, spec in _ALIAS_INDEX.items():
        if candidate_key.lower() == key:
            return spec
    raise KeyError(f"unknown track '{name}'. Known: {sorted(set(_ALIAS_INDEX.keys()))}")
