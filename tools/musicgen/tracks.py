"""Registry of the game's music slots and the mood each one should evoke.

Mapping confirmed by reading RPDemo's source directly:
  - src/music.c:12              default/title track
  - src/gameplay.c:46-66        track_for_level()
  - src/gameplay.c:176          gameplay_reset_to_title_scene() -> title track
  - src/gameplay_boss.c:367     BOSS_STAGE_MUSIC_TRACK (src/constants.h:111)
  - src/level_bonus.c:116       bonus round
  - src/gameplay_game_over.c:18 game over (win or lose)
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
    resource: str  # e.g. "RESOURCE.001"
    description: str
    aliases: list
    mood: MoodPreset


TRACKS: list = [
    TrackSpec(
        resource="RESOURCE.001",
        description="Title / attract screen",
        aliases=["title"],
        mood=MoodPreset(key_root=0, scale="major", bpm=124, intensity=0.10, lead_octave=5),
    ),
    TrackSpec(
        resource="RESOURCE.005",
        description="Level 1 & 5",
        aliases=["level1", "level5"],
        mood=MoodPreset(key_root=9, scale="mixolydian", bpm=132, intensity=0.20, lead_octave=5),
    ),
    TrackSpec(
        resource="RESOURCE.003",
        description="Level 2 & 6",
        aliases=["level2", "level6"],
        mood=MoodPreset(key_root=2, scale="dorian", bpm=128, intensity=0.35, lead_octave=5),
    ),
    TrackSpec(
        resource="RESOURCE.008",
        description="Level 3",
        aliases=["level3"],
        mood=MoodPreset(key_root=7, scale="mixolydian", bpm=134, intensity=0.40, lead_octave=5),
    ),
    TrackSpec(
        resource="RESOURCE.002",
        description="Level 4",
        aliases=["level4"],
        mood=MoodPreset(key_root=4, scale="phrygian", bpm=126, intensity=0.50, lead_octave=4),
    ),
    TrackSpec(
        resource="RESOURCE.010",
        description="Level 7 (late-game)",
        aliases=["level7"],
        mood=MoodPreset(key_root=11, scale="minor", bpm=136, intensity=0.75, lead_octave=5),
    ),
    TrackSpec(
        resource="RESOURCE.009",
        description="Boss stage (also level 8+ fallback)",
        aliases=["boss"],
        mood=MoodPreset(key_root=9, scale="minor", bpm=140, intensity=0.95, lead_octave=4, arp_octave=3),
    ),
    TrackSpec(
        resource="RESOURCE.006",
        description="Bonus / reward round",
        aliases=["bonus", "reward"],
        mood=MoodPreset(key_root=5, scale="major", bpm=128, intensity=0.15, lead_octave=5),
    ),
    TrackSpec(
        resource="RESOURCE.011",
        description="Game over (win or lose)",
        aliases=["gameover"],
        mood=MoodPreset(key_root=2, scale="minor", bpm=100, intensity=0.55, target_seconds=80.0,
                         lead_octave=4, use_drums=False),
    ),
]

_ALIAS_INDEX = {}
for _t in TRACKS:
    _ALIAS_INDEX[_t.resource] = _t
    _ALIAS_INDEX[_t.resource.split(".")[1]] = _t  # "001"
    for _a in _t.aliases:
        _ALIAS_INDEX[_a] = _t


def resolve(name: str) -> TrackSpec:
    key = name.strip().lower()
    for candidate_key, spec in _ALIAS_INDEX.items():
        if candidate_key.lower() == key:
            return spec
    raise KeyError(f"unknown track '{name}'. Known: {sorted(set(_ALIAS_INDEX.keys()))}")
