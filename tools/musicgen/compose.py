"""Procedural, seeded composition of a Kraftwerk-leaning electronic/synth
track on OPL2.

Not a general music engine -- a small set of rules (scale + chord
progressions + fixed channel roles, arranged into an intro/theme/bridge/
outro structure) that produces a several-minute, layered, genre-appropriate
sketch every time it's run with a given seed. It's meant to be a starting
point to audition and hand-edit in Furnace, not a finished composition.

Motorik/sequenced, not orchestral: the arp channel runs an unbroken
16th-note sequence (the Kraftwerk signature), drums are a steady
four-on-the-floor kick + 2-and-4 snare + unbroken 16th-note hats (no
syncopation/fills -- the "machine" pulse is the point), the bass is a
driving quarter-note pulse locked to the kick, and the lead is a short
repeating motif (same shape for an entire 4-bar chunk, not re-rolled every
bar) rather than an evolving melody. Instruments are drawn from OPL2's
synth lead/bass/pad GM ranges specifically (see instruments.py's
`lead_synth`/`bass_synth`/`pad_synth`), not the wider
orchestral/mallet-percussion pools an earlier version of this tool used.

Arrangement approach: the song is built from a handful of 4-bar "chunks" of
real musical content (two takes on the A progression, two on the B
progression, two for a contrasting bridge). The order list then places those
few chunks many times over, and -- critically -- can expose a different
subset of channels each time the same chunk plays. That's how the intro
"layers in" one instrument at a time and how the outro fades back down,
without needing to generate distinct content for every repeat.

Variety comes from three independent seeded choices per song, so two tracks
rarely feel alike even with similar moods:
  - which of 5 layers (drums/bass/pad/arp/lead) is introduced first, second,
    etc. in the intro (and dropped in reverse order in the outro)
  - which chord progressions get used, weighted by the mood's `intensity`
    (title/early levels stay in simple, diatonic territory; boss fights and
    late levels pull from a pool of more harmonically restless ones)
  - for high-intensity tracks only, whether the contrasting B-theme/bridge
    modulates to a different key -- a real "raise the stakes" moment tied to
    boss fights and late levels, not just a different melody in the same key
"""

from __future__ import annotations

import random
from dataclasses import dataclass
from typing import Dict, List, Set, Tuple

from .furwriter import FurInstrument, FurSong, RowCell, note_value
from .instruments import role_candidates
from .tracks import MoodPreset

SCALES = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
}

# fixed channel assignment -- deliberately only 7 of the OPL2's 9 hardware
# channels (indices 0-6). Channels 7-8 are reserved for the game's own sound
# effects and must never be touched by generated music.
ROLE_TO_CHANNEL = {"lead": 0, "arp": 1, "bass": 2, "pad": 6, "kick": 3, "snare": 4, "hat": 5}

# OPL2's pattern volume column is 0-63 (63 = instrument's own baked-in
# level, unattenuated; 0 = silent -- see Furnace's DIV_CMD_GET_VOLMAX for a
# normal FM channel, which returns 63, not the 0-127 most other columns in
# Furnace's UI suggest). A rough mix: lead upfront, bass/arp driving but
# under it, pad minimal support, drums punchy but not burying the sequencer.
ROLE_VOLUME = {"lead": 56, "arp": 48, "bass": 58, "pad": 40, "kick": 63, "snare": 54, "hat": 38}

CHANNEL_COUNT = 9
MUSIC_CHANNELS = 7
EFFECT_CHANNELS = {7, 8}
assert set(ROLE_TO_CHANNEL.values()) == set(range(MUSIC_CHANNELS))
assert EFFECT_CHANNELS == set(range(CHANNEL_COUNT)) - set(range(MUSIC_CHANNELS))

DRUM_ROLES = {"kick", "snare", "hat"}
MELODIC_ROLES = {"lead", "arp", "bass", "pad"}
FULL_ROLES = MELODIC_ROLES | DRUM_ROLES

ROWS_PER_BEAT = 4  # 16th-note grid
BEATS_PER_BAR = 4
ROWS_PER_BAR = ROWS_PER_BEAT * BEATS_PER_BAR  # 16
CHUNK_BARS = 4
CHUNK_ROWS = ROWS_PER_BAR * CHUNK_BARS  # 64

EMPTY_PATTERN_IDX = 6  # A1,A2,B1,B2,BR1,BR2 = 0..5; 6 = shared silent pattern

# Generic diatonic-degree chord progressions (0=tonic .. 6=leading tone),
# mode-agnostic -- chord quality follows whichever scale the mood uses.
# Tier 1: simple, close-to-home motion (I-IV-V territory) for light moods.
# Tier 2: adds ii/vi-type detours.
# Tier 3: wide, restless leaps (deceptive-cadence-like movement) reserved
# for high-intensity moods -- boss fights, late levels.
PROGRESSION_BANK: List[Tuple[int, List[int]]] = [
    (1, [0, 3, 4, 0]),
    (1, [0, 4, 0, 4]),
    (1, [0, 3, 0, 4]),
    (1, [0, 4, 3, 0]),
    (2, [0, 5, 3, 4]),
    (2, [0, 2, 3, 4]),
    (2, [5, 3, 0, 4]),
    (2, [0, 3, 5, 4]),
    (2, [0, 5, 4, 0]),
    (3, [0, 6, 2, 5]),
    (3, [0, 6, 3, 4]),
    (3, [0, 2, 6, 5]),
    (3, [0, 6, 5, 3]),
    (3, [0, 4, 6, 2]),
    (3, [0, 6, 4, 2]),
]


def compute_speed(bpm: float, hz: float = 60.0) -> int:
    """Ticks-per-row for a 16th-note grid at the given BPM."""
    ticks_per_row = (hz * 60.0) / (bpm * ROWS_PER_BEAT)
    return max(1, round(ticks_per_row))


def _select_progressions(rng: random.Random, intensity: float) -> Tuple[List[int], List[int]]:
    """Weighted pick of two distinct progressions. Higher intensity biases
    toward the more harmonically restless (tier 3) pool; light moods stay
    almost entirely in tier 1."""

    def weight(tier: int) -> float:
        if tier == 1:
            return 1.0
        if tier == 2:
            return max(0.15, intensity * 1.8)
        return max(0.0, (intensity - 0.35)) * 2.2

    idxs = list(range(len(PROGRESSION_BANK)))
    chosen: List[List[int]] = []
    for _ in range(2):
        weights = [weight(PROGRESSION_BANK[i][0]) + 0.02 for i in idxs]
        pick = rng.choices(idxs, weights=weights, k=1)[0]
        chosen.append(PROGRESSION_BANK[pick][1])
        idxs.remove(pick)
    return chosen[0], chosen[1]


def _scale_note(root: int, scale: str, degree: int, octave: int) -> int:
    """degree may be negative or >= len(scale); wraps into further octaves."""
    ivs = SCALES[scale]
    n = len(ivs)
    oct_add, deg = divmod(degree, n)
    semitone = (root + ivs[deg]) % 12
    return note_value(octave + oct_add, semitone)


def _pick_instrument(rng: random.Random, bank: List[FurInstrument], role: str) -> int:
    candidates = role_candidates(role)
    # skip obviously-silent/degenerate patches (both operators' mult==0 tends
    # to mean "empty slot", e.g. the gm_bank gap at index 190)
    usable = [i for i in candidates if not (bank[i].op0.mult == 0 and bank[i].op1.mult == 0)]
    return rng.choice(usable or candidates)


def _set(rows: Dict[str, Dict[int, RowCell]], role: str, row: int, note: int, ins: Dict[str, int]) -> None:
    rows[role][row] = RowCell(note=note, ins=ins[role], vol=ROLE_VOLUME[role])


def _generate_chunk(rng: random.Random, mood: MoodPreset, ins: Dict[str, int],
                     progression: List[int], root: int) -> Dict[str, Dict[int, RowCell]]:
    """Generates CHUNK_BARS (4) bars of real content for every role. Which
    roles actually get heard in the final song is decided later, per
    section, by the arrangement plan -- this always writes all of them.
    `root` is passed in separately from mood.key_root so a chunk can be
    generated in a modulated key for high-intensity tracks.

    Motorik/Kraftwerk-leaning: the lead's `shape` and the arp's `arp_kind`
    are each picked once for the *whole* chunk, not re-rolled every bar --
    a repeating, sequenced hook/pattern is the point, not a melody that
    keeps evolving bar to bar."""
    scale = mood.scale
    rows: Dict[str, Dict[int, RowCell]] = {role: {} for role in ROLE_TO_CHANNEL}

    shape = rng.choice(["ascend", "descend", "static", "skip", "call_response"])
    arp_kind = rng.choice(["up", "updown", "alternate"])

    for bar in range(CHUNK_BARS):
        base = bar * ROWS_PER_BAR
        degree = progression[bar % len(progression)]
        chord_tones = [degree, degree + 2, degree + 4]  # root/3rd/5th, scale degrees

        # bass: driving quarter-note pulse on every beat (four-on-the-floor,
        # locked to the kick), root throughout except a lifted 4th beat for
        # forward momentum into the next bar
        for i in range(BEATS_PER_BAR):
            beat_degree = degree if i < BEATS_PER_BAR - 1 else degree + 2
            _set(rows, "bass", base + i * ROWS_PER_BEAT,
                 _scale_note(root, scale, beat_degree, mood.bass_octave), ins)

        # arp: relentless 16th-note sequence -- the Kraftwerk signature.
        # arp_kind is fixed for the whole chunk; only which chord tones it
        # cycles through changes bar to bar (following the progression).
        if arp_kind == "up":
            cycle = chord_tones
        elif arp_kind == "updown":
            cycle = chord_tones + chord_tones[-2:0:-1]
        else:  # alternate: bounce between root and each upper tone
            cycle = [degree, chord_tones[1], degree, chord_tones[2]]
        for r in range(ROWS_PER_BAR):
            _set(rows, "arp", base + r, _scale_note(root, scale, cycle[r % len(cycle)], mood.arp_octave), ins)

        # pad: one sustained chord tone per bar (root, alternating up to the
        # 5th every other bar) -- a minimal held synth-pad layer under the
        # sequencer, not the focus.
        pad_degree = chord_tones[0] if bar % 2 == 0 else chord_tones[2]
        _set(rows, "pad", base + 0, _scale_note(root, scale, pad_degree, mood.arp_octave - 1), ins)

        # lead: quarter-note hook, same shape all chunk long so it reads as
        # a repeating motif/loop rather than a wandering melody
        beat_rows = [i * ROWS_PER_BEAT for i in range(BEATS_PER_BAR)]
        for i, r in enumerate(beat_rows):
            if rng.random() < 0.06:
                continue
            if shape == "ascend":
                deg = degree + i
            elif shape == "descend":
                deg = degree + (BEATS_PER_BAR - 1 - i)
            elif shape == "static":
                deg = chord_tones[i % len(chord_tones)]
            elif shape == "skip":
                deg = chord_tones[(i * 2) % len(chord_tones)]
            else:  # call_response
                deg = degree if i % 2 == 0 else degree + 4
            _set(rows, "lead", base + r, _scale_note(root, scale, deg, mood.lead_octave), ins)
            if rng.random() < 0.15 and r + 2 < ROWS_PER_BAR:
                deg2 = deg + (1 if rng.random() < 0.5 else -1)
                _set(rows, "lead", base + r + 2, _scale_note(root, scale, deg2, mood.lead_octave), ins)

        # drums: motorik -- four-on-the-floor kick every beat, snare
        # backbeat on 2 & 4, unbroken 16th-note hats (the "machine" pulse)
        for i in range(BEATS_PER_BAR):
            _set(rows, "kick", base + i * ROWS_PER_BEAT, note_value(3, 0), ins)
        _set(rows, "snare", base + 1 * ROWS_PER_BEAT, note_value(3, 0), ins)
        _set(rows, "snare", base + 3 * ROWS_PER_BEAT, note_value(3, 0), ins)
        for r in range(ROWS_PER_BAR):
            _set(rows, "hat", base + r, note_value(3, 0), ins)

    return rows


@dataclass
class _Section:
    chunk_key: str
    active_roles: Set[str]


def _reveal_order(rng: random.Random, mood: MoodPreset) -> List[str]:
    """A random permutation of the 5 layer-units for this song, e.g.
    sometimes ["drums","lead","bass","pad","arp"] -- so which instrument
    opens the track varies song to song instead of always being bass."""
    units = ["drums", "bass", "pad", "arp", "lead"] if mood.use_drums else ["bass", "pad", "arp", "lead"]
    order = list(units)
    rng.shuffle(order)
    return order


def _roles_for_unit(unit: str) -> Set[str]:
    return set(DRUM_ROLES) if unit == "drums" else {unit}


def _plan_sections(rng: random.Random, mood: MoodPreset) -> List[_Section]:
    full = FULL_ROLES if mood.use_drums else MELODIC_ROLES
    order = _reveal_order(rng, mood)

    plan: List[_Section] = []

    # layered intro: same 4 bars (chunk A1), one layer unit added at a time,
    # in a randomized order (see _reveal_order).
    cumulative: Set[str] = set()
    for unit in order:
        cumulative |= _roles_for_unit(unit)
        plan.append(_Section("A1", set(cumulative)))

    rows_per_chunk = CHUNK_ROWS
    rows_per_sec = mood.bpm / 60.0 * ROWS_PER_BEAT
    target_rows = mood.target_seconds * rows_per_sec

    rows_added = len(plan) * rows_per_chunk
    verse_a = [_Section("A1", set(full)), _Section("A2", set(full))]
    verse_b = [_Section("B1", set(full)), _Section("B2", set(full))]
    # bridge always drops drums for contrast (a no-op if the track has none)
    bridge = [_Section("BR1", set(MELODIC_ROLES)), _Section("BR2", set(MELODIC_ROLES))]

    # structural variety: does this song even have a bridge, where roughly
    # does it land, and which verse comes first -- higher-intensity tracks
    # are more likely to include the contrasting bridge at all.
    include_bridge = rng.random() < (0.5 + mood.intensity * 0.4)
    bridge_at = rng.uniform(0.3, 0.55)
    start_with_b = rng.random() < 0.5

    did_bridge = not include_bridge
    toggle = 1 if start_with_b else 0
    # leave headroom for the closing "return + fade" section below (fixed
    # size, computed up front -- NOT len(plan), which keeps growing as this
    # loop runs and would otherwise make the target unreachable)
    closing_steps = 2 + (len(order) - 1)
    while rows_added < target_rows - closing_steps * rows_per_chunk:
        if not did_bridge and rows_added >= target_rows * bridge_at:
            plan.extend(bridge)
            rows_added += rows_per_chunk * len(bridge)
            did_bridge = True
            continue
        pool = verse_a if toggle % 2 == 0 else verse_b
        plan.extend(pool)
        rows_added += rows_per_chunk * len(pool)
        toggle += 1

    # return to the A theme in full
    plan.append(_Section("A1", set(full)))
    plan.append(_Section("A2", set(full)))

    # outro: drop layers in the reverse order they were introduced, but stop
    # short of total silence -- the last remaining layer is whichever one
    # opened the song, so the loop point (outro -> back to intro) feels
    # continuous rather than cutting to dead air.
    remaining = set(cumulative)
    for unit in reversed(order[1:]):
        remaining -= _roles_for_unit(unit)
        plan.append(_Section("A1", set(remaining)))

    return plan


def generate_track(name: str, mood: MoodPreset, seed: int, bank: List[FurInstrument]) -> FurSong:
    rng = random.Random(seed)

    ins: Dict[str, int] = {
        "lead": _pick_instrument(rng, bank, "lead_synth"),
        "arp": _pick_instrument(rng, bank, "lead_synth"),
        "bass": _pick_instrument(rng, bank, "bass_synth"),
        "pad": _pick_instrument(rng, bank, "pad_synth"),
    }
    if mood.use_drums:
        kick_ins, snare_ins, hat_ins = role_candidates("perc")  # Bass Drum, Snare, Hat (fixed order)
        ins["kick"], ins["snare"], ins["hat"] = kick_ins, snare_ins, hat_ins
    else:
        ins["kick"] = ins["snare"] = ins["hat"] = 0

    prog_a, prog_b = _select_progressions(rng, mood.intensity)

    # High-intensity tracks (boss fights, late levels) get a real "raise the
    # stakes" moment: the contrasting B-theme/bridge modulates to a
    # different key instead of just using a different melody in the same key.
    b_root = mood.key_root
    if mood.intensity >= 0.65 and rng.random() < 0.65:
        b_root = (mood.key_root + rng.choice([1, 2, -1, -2])) % 12

    chunk_progression = {"A1": prog_a, "A2": prog_a, "B1": prog_b, "B2": prog_b, "BR1": prog_b, "BR2": prog_b}
    chunk_root = {"A1": mood.key_root, "A2": mood.key_root, "B1": b_root, "B2": b_root, "BR1": b_root, "BR2": b_root}
    chunk_order = ["A1", "A2", "B1", "B2", "BR1", "BR2"]
    chunk_rows_by_key = {
        key: _generate_chunk(rng, mood, ins, chunk_progression[key], chunk_root[key]) for key in chunk_order
    }
    pattern_idx_by_key = {key: i for i, key in enumerate(chunk_order)}

    bpm = max(40.0, mood.bpm + rng.uniform(-4, 4))
    speed = compute_speed(bpm)

    song = FurSong(
        name=name,
        hz=60.0,
        speed=speed,
        pattern_len=CHUNK_ROWS,
        instruments=bank,
        channels=CHANNEL_COUNT,
    )
    song.orders = [[] for _ in range(CHANNEL_COUNT)]
    for ch in range(CHANNEL_COUNT):
        song.patterns[(ch, EMPTY_PATTERN_IDX)] = {}

    unused_channels = set(range(CHANNEL_COUNT)) - set(ROLE_TO_CHANNEL.values())
    for section in _plan_sections(rng, mood):
        pidx = pattern_idx_by_key[section.chunk_key]
        for role, ch in ROLE_TO_CHANNEL.items():
            if role in section.active_roles:
                song.orders[ch].append(pidx)
                song.patterns[(ch, pidx)] = chunk_rows_by_key[section.chunk_key][role]
            else:
                song.orders[ch].append(EMPTY_PATTERN_IDX)
        for ch in unused_channels:
            song.orders[ch].append(EMPTY_PATTERN_IDX)

    return song
