#!/usr/bin/env python3
"""Generate short, procedural OPL2 sound-effect VGM clips for RPStarHopper's
reserved SFX channels (7 and 8 -- channels 0-6 are music-only, see
compose.py's ROLE_TO_CHANNEL / musicgen/README.md).

Unlike generate_music.py, this writes raw VGM bytes directly (register
writes + waits), with no Furnace/.fur round-trip at all. These are short,
hand-designed envelope/frequency-sweep clips -- the classic arcade-SFX
technique of rewriting the frequency register every few milliseconds to
sweep pitch, and shaping volume with the operator's own attack/decay/
sustain/release -- not tracker compositions, so a pattern-grid tracker GUI
doesn't help author them. This script *is* the editable source; there's no
separate hand-tunable project file the way music tracks have a .fur.

Channel assignment (see src/sfx.h): two independent one-shot, priority-
queued streams, one per channel, so a burst of enemy events can never
interrupt a player cue (or vice versa) the way sharing one channel used to.
  - Channel 7 (sfx_play_player()): everything triggered by the player --
    fire, hit, destroyed, pickup, extra life, level-clear fanfare, and the
    low-energy warning beep (retriggered periodically by sfx.c while
    health is low -- not a looped VGM, the game code retriggers this same
    short clip on a timer, matching a classic arcade pulse rather than a
    sustained drone).
  - Channel 8 (sfx_play_enemy()): everything triggered by an enemy --
    fire, destroyed.

Usage:
  python3 tools/generate_sfx.py            # writes every SFX to music/sfx/
  python3 tools/generate_sfx.py --list
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path
from typing import Callable, List, Optional, Tuple

MUSIC_SFX_DIR = Path(__file__).resolve().parent.parent / "music" / "sfx"
SRC_DIR = Path(__file__).resolve().parent.parent / "src"
XRAM_BLOB_PATH = MUSIC_SFX_DIR / "sfx_xram.bin"
LAYOUT_HEADER_PATH = SRC_DIR / "sfx_layout.h"

# The VGM format's own time unit is always 44100 Hz, independent of
# whatever rate the OPL2 chip is actually clocked at -- see vgm.c's
# wait_samples handling (735 samples == one 60 Hz frame, 44100/60).
VGM_SAMPLE_RATE = 44100

# OPL2's internal sample rate for the F-Number/Block frequency formula --
# clock (3579545 Hz) / 72, the standard constant used by every OPL2
# frequency-calculation reference (also what the emulator's emu8950 core
# uses internally, see rp6502/src/core/aud/opl.c's OPL_CLOCK_RATE comment).
OPL_INTERNAL_RATE = 49716.0

# Register offsets for operators 0/1 (modulator/carrier) of OPL2 channels
# 6/7/8 -- the non-contiguous slot layout every OPL2 reference table uses
# (channels 0-2 -> slots 0-2/3-5, 3-5 -> 6-8/9-11, 6-8 -> 12-14/15-17).
# Only 7/8 are listed: this project's music (compose.py) owns 0-6, and 6
# isn't available to SFX at all.
MOD_OFFSET = {7: 0x11, 8: 0x12}
CAR_OFFSET = {7: 0x14, 8: 0x15}


def freq_to_fnum_block(freq_hz: float) -> Tuple[int, int]:
    """Standard OPL2 F-Number/Block encoding: picks the smallest Block
    (0-7) whose F-Number fits the chip's 10-bit field (1-1023) for the
    given frequency -- F-Number's own inverse relationship with Block is
    what makes a single small loop like this work for both the low
    (~100 Hz) and high (~3 kHz) ends of this file's SFX."""
    fnum = 1023
    for block in range(8):
        fnum = round(freq_hz * (2 ** (20 - block)) / OPL_INTERNAL_RATE)
        if 0 < fnum <= 1023:
            return fnum, block
    return max(1, min(1023, fnum)), 7


class SfxBuilder:
    """Accumulates (reg, val, wait_samples_after) VGM events for a
    one-shot clip on a single OPL2 channel (7 or 8), then serializes them
    into a minimal VGM file vgm.c can play (magic + zeroed header up to
    the data offset, then 0x5A reg/val writes and 0x61 16-bit waits, ending
    with 0x66 -- no loop tag, so the player just reaches end-of-track)."""

    def __init__(self, channel: int):
        assert channel in (7, 8), "SFX only ever uses channels 7/8 -- see module docstring"
        self.channel = channel
        self.mod = MOD_OFFSET[channel]
        self.car = CAR_OFFSET[channel]
        self.events: List[Tuple[Optional[int], Optional[int], int]] = []

    def _write(self, reg: int, val: int) -> None:
        self.events.append((reg, val & 0xFF, 0))

    def patch(self, *, mod_am=0, mod_vib=0, mod_eg_sustain=1, mod_ksr=0, mod_mult=1,
              mod_ksl=0, mod_tl=0, mod_ar=15, mod_dr=8, mod_sl=0, mod_rr=8, mod_wave=0,
              car_am=0, car_vib=0, car_eg_sustain=1, car_ksr=0, car_mult=1,
              car_ksl=0, car_tl=0, car_ar=15, car_dr=8, car_sl=0, car_rr=8, car_wave=0,
              feedback=0, connection=0) -> None:
        """Full 2-operator patch setup. Field names spell out the OPL2
        register layout directly (any YM3812 reference): 0x20 = AM/Vibrato/
        EG-type(sustain)/KSR/Multiple, 0x40 = KSL/Total-Level (0=loudest,
        63=silent), 0x60 = Attack-Rate/Decay-Rate, 0x80 = Sustain-Level/
        Release-Rate, 0xE0 = Waveform (0-3 on OPL2), 0xC0 = Feedback/
        Connection (0=FM, i.e. modulator shapes carrier; 1=additive)."""
        def op20(am, vib, sustain, ksr, mult):
            return (am << 7) | (vib << 6) | (sustain << 5) | (ksr << 4) | (mult & 0x0F)

        def op40(ksl, tl):
            return (ksl << 6) | (tl & 0x3F)

        def op60(ar, dr):
            return (ar << 4) | (dr & 0x0F)

        def op80(sl, rr):
            return (sl << 4) | (rr & 0x0F)

        self._write(0x20 + self.mod, op20(mod_am, mod_vib, mod_eg_sustain, mod_ksr, mod_mult))
        self._write(0x20 + self.car, op20(car_am, car_vib, car_eg_sustain, car_ksr, car_mult))
        self._write(0x40 + self.mod, op40(mod_ksl, mod_tl))
        self._write(0x40 + self.car, op40(car_ksl, car_tl))
        self._write(0x60 + self.mod, op60(mod_ar, mod_dr))
        self._write(0x60 + self.car, op60(car_ar, car_dr))
        self._write(0x80 + self.mod, op80(mod_sl, mod_rr))
        self._write(0x80 + self.car, op80(car_sl, car_rr))
        self._write(0xE0 + self.mod, mod_wave & 0x07)
        self._write(0xE0 + self.car, car_wave & 0x07)
        self._write(0xC0 + self.channel, ((feedback & 0x07) << 1) | (connection & 0x01))

    def note_on(self, freq_hz: float) -> None:
        """Strikes (or re-tunes, if key-on is already set -- see sweep())
        this channel's only voice at freq_hz. Key-on stays 1 across a
        sweep's repeated calls, so the envelope is never re-triggered mid-
        glide -- only an explicit key_off() between calls makes two
        note_on()s read as separate, re-plucked notes."""
        fnum, block = freq_to_fnum_block(freq_hz)
        self._write(0xA0 + self.channel, fnum & 0xFF)
        self._write(0xB0 + self.channel, 0x20 | (block << 2) | ((fnum >> 8) & 0x03))

    def key_off(self) -> None:
        self._write(0xB0 + self.channel, 0x00)

    def wait_ms(self, ms: float) -> None:
        samples = round(ms * VGM_SAMPLE_RATE / 1000.0)
        if samples <= 0:
            return
        if self.events:
            reg, val, w = self.events[-1]
            self.events[-1] = (reg, val, w + samples)
        else:
            self.events.append((None, None, samples))

    def sweep(self, start_hz: float, end_hz: float, duration_ms: float, steps: int = 12) -> None:
        """Linearly sweeps frequency from start_hz to end_hz over
        duration_ms, re-tuning every duration_ms/steps. OPL2 has no
        hardware pitch envelope -- a sweep is just rewriting 0xA0/0xB0
        repeatedly over time, the same technique real OPL2-era games used
        for laser/explosion effects."""
        step_ms = duration_ms / steps
        for i in range(steps + 1):
            t = i / steps
            freq = start_hz + (end_hz - start_hz) * t
            self.note_on(freq)
            if i < steps:
                self.wait_ms(step_ms)

    def tremolo(self, cycles: int, curve: Tuple[int, ...], step_ms: float) -> None:
        """Repeatedly steps the carrier's Total Level (0=loudest, 63=
        silent) through `curve`, `cycles` times -- a volume flutter on a
        single held pitch (call note_on() once beforehand), not a pitch
        sweep. This is the technique behind RPPacman's own extra-life cue
        (its sfxextralife, music/tracks/PacManCE_19.BIN there): inspected
        directly (a reg/val/delay dump of that file) to confirm it holds
        one note throughout and never touches 0xA0/0xB0 again after the
        initial key-on -- all the character comes from rhythmically
        stepping 0x40+car up and back down."""
        for _ in range(cycles):
            for tl in curve:
                self._write(0x40 + self.car, tl & 0x3F)
                self.wait_ms(step_ms)

    def to_command_bytes(self) -> bytes:
        """The raw (reg,val)/wait/end command stream with no VGM file
        header -- what actually gets preloaded into XRAM (see
        SFX_XRAM_HEADER's docstring below) and what to_vgm_bytes() wraps
        with a header for the standalone .vgm files."""
        out = bytearray()
        for reg, val, wait in self.events:
            if reg is not None:
                out += bytes([0x5A, reg, val])
            remaining = wait
            while remaining > 0xFFFF:
                out += bytes([0x61]) + struct.pack("<H", 0xFFFF)
                remaining -= 0xFFFF
            if remaining > 0:
                out += bytes([0x61]) + struct.pack("<H", remaining)
        out += bytes([0x66])  # end of sound data; no loop tag, so the reader just stops
        return bytes(out)

    def to_vgm_bytes(self) -> bytes:
        header = bytearray(0x40)
        header[0:4] = b"Vgm "
        return bytes(header) + self.to_command_bytes()


# ---------------------------------------------------------------------------
# The 8 SFX. Names are the ROM asset ids (see CMakeLists.txt / src/sfx.h),
# kept to 8.3-safe lengths like the music tracks (see tracks.py's
# docstring for why).
# ---------------------------------------------------------------------------


def sfx_plyrfire() -> SfxBuilder:
    """Player laser: a clean descending chirp -- low feedback and a
    single-multiple sine-ish tone (no abs-sine grit) so it reads as clean
    rather than scratchy sitting on top of the full OPL2 soundtrack, and
    pitched an octave below the original version for better balance
    against the music. The lowest-priority SFX (fired constantly), so it's
    also the cheapest/shortest."""
    b = SfxBuilder(7)
    b.patch(mod_mult=1, mod_tl=28, mod_ar=15, mod_dr=10, mod_sl=0, mod_rr=10, mod_wave=0,
            car_mult=1, car_tl=0, car_ar=15, car_dr=9, car_sl=2, car_rr=10, car_wave=0,
            feedback=1, connection=0)
    b.sweep(900, 350, 70, steps=8)
    b.key_off()
    return b


def sfx_enmyfire() -> SfxBuilder:
    """Enemy laser: the same clean, low-feedback approach as the player's
    own (see sfx_plyrfire), pitched a further octave down so the two stay
    easy to tell apart by ear without either one sounding scratchy. Lives
    on OPL2 channel 8 (see the module docstring), not 7 -- it has to be
    baked into the file itself, not just decided by which software queue
    plays it, or the two channels would still fight over the same
    physical synth channel's registers."""
    b = SfxBuilder(8)
    b.patch(mod_mult=1, mod_tl=26, mod_ar=15, mod_dr=10, mod_sl=0, mod_rr=10, mod_wave=0,
            car_mult=1, car_tl=0, car_ar=15, car_dr=9, car_sl=2, car_rr=10, car_wave=0,
            feedback=1, connection=0)
    b.sweep(450, 200, 90, steps=8)
    b.key_off()
    return b


def sfx_enmydie() -> SfxBuilder:
    """Enemy destroyed: a short, gritty downward burst -- high feedback
    and a quarter-sine waveform for noise-like texture OPL2 doesn't have a
    real noise channel for. Channel 8 (enemy), like sfx_enmyfire."""
    b = SfxBuilder(8)
    b.patch(mod_mult=4, mod_tl=10, mod_ar=15, mod_dr=12, mod_sl=4, mod_rr=12, mod_wave=3,
            car_mult=3, car_tl=0, car_ar=15, car_dr=11, car_sl=3, car_rr=11, car_wave=3,
            feedback=7, connection=0)
    b.sweep(500, 120, 160, steps=10)
    b.key_off()
    return b


def sfx_plyrhit() -> SfxBuilder:
    """Player takes damage but survives: a sharp, very short low thud --
    deliberately not melodic, so it never gets confused with a laser or an
    explosion at a glance-listen."""
    b = SfxBuilder(7)
    b.patch(mod_mult=1, mod_tl=8, mod_ar=15, mod_dr=14, mod_sl=0, mod_rr=14, mod_wave=2,
            car_mult=1, car_tl=0, car_ar=15, car_dr=13, car_sl=2, car_rr=13, car_wave=2,
            feedback=7, connection=0)
    b.sweep(220, 90, 80, steps=6)
    b.key_off()
    return b


def sfx_plyrdie() -> SfxBuilder:
    """Player destroyed: a longer, noisier descending explosion -- top
    priority, the one cue that should never get cut off by anything else
    on channel 7."""
    b = SfxBuilder(7)
    b.patch(mod_mult=5, mod_tl=8, mod_ar=15, mod_dr=8, mod_sl=6, mod_rr=6, mod_wave=3,
            car_mult=2, car_tl=0, car_ar=15, car_dr=7, car_sl=5, car_rr=5, car_wave=3,
            feedback=7, connection=0)
    b.sweep(700, 60, 650, steps=20)
    b.key_off()
    return b


def sfx_pickup() -> SfxBuilder:
    """Power-up/energy/speed pickup collected: a bright ascending "swoop"
    -- one shared chime for all three pickup kinds, kept simple rather
    than authoring three near-identical variants."""
    b = SfxBuilder(7)
    b.patch(mod_mult=1, mod_tl=10, mod_ar=15, mod_dr=8, mod_sl=3, mod_rr=8, mod_wave=0,
            car_mult=1, car_tl=0, car_ar=15, car_dr=7, car_sl=4, car_rr=7, car_wave=0,
            feedback=3, connection=0)
    b.sweep(440, 1320, 140, steps=10)
    b.key_off()
    return b


def sfx_tally() -> SfxBuilder:
    """Bonus-stage score/kill tally tick: the same ascending swoop as
    sfx_pickup(), quieted by +6 TL on the carrier (FM/connection=0, so the
    carrier's TL alone sets overall loudness -- see patch()'s docstring).
    A dedicated clip rather than reusing PickUp directly, since this one
    retriggers rapidly during the tally and needed to come down in the mix
    without touching the volume of an actual pickup being collected."""
    b = SfxBuilder(7)
    b.patch(mod_mult=1, mod_tl=10, mod_ar=15, mod_dr=8, mod_sl=3, mod_rr=8, mod_wave=0,
            car_mult=1, car_tl=6, car_ar=15, car_dr=7, car_sl=4, car_rr=7, car_wave=0,
            feedback=3, connection=0)
    b.sweep(440, 1320, 140, steps=10)
    b.key_off()
    return b


def sfx_lvlclear() -> SfxBuilder:
    """Level-complete celebration: a short 4-note ascending major fanfare
    (C5-E5-G5-C6), each note explicitly re-struck (key_off between
    note_on calls) so they read as distinct plucked notes rather than a
    glide. Additive (connection=1) for a fuller, more "chime"-like tone
    than the FM-only stingers."""
    # Additive (connection=1): both operators feed the output directly,
    # not just the carrier -- so both TLs need to come down for a real
    # loudness gain here, not just the carrier's.
    b = SfxBuilder(7)
    b.patch(mod_mult=1, mod_tl=4, mod_ar=15, mod_dr=6, mod_sl=4, mod_rr=6, mod_wave=0,
            car_mult=2, car_tl=0, car_ar=15, car_dr=5, car_sl=5, car_rr=5, car_wave=0,
            feedback=3, connection=1)
    for freq in (523.25, 659.25, 783.99, 1046.50):  # C5, E5, G5, C6
        b.note_on(freq)
        b.wait_ms(140)
        b.key_off()
        b.wait_ms(20)
    return b


def sfx_lowenrgy() -> SfxBuilder:
    """Low-energy warning: a short double-beep on channel 7 (player -- it's
    the player's own health, and sfx_play_player() is what retriggers it).
    Not looped in the file itself -- sfx.c's sfx_update() retriggers this
    same clip on a timer while health stays low (a classic arcade pulsing
    alert, not a sustained drone)."""
    b = SfxBuilder(7)
    b.patch(mod_mult=2, mod_tl=10, mod_ar=15, mod_dr=10, mod_sl=4, mod_rr=10, mod_wave=1,
            car_mult=1, car_tl=0, car_ar=15, car_dr=9, car_sl=5, car_rr=9, car_wave=1,
            feedback=3, connection=0)
    b.note_on(660)
    b.wait_ms(90)
    b.key_off()
    b.wait_ms(60)
    b.note_on(660)
    b.wait_ms(90)
    b.key_off()
    return b


def sfx_extralife() -> SfxBuilder:
    """Extra life awarded: directly inspired by RPPacman's own extra-life
    cue (see tremolo()'s docstring for how that file was inspected).
    Unlike every other SFX here, this isn't a pitch sweep -- it's a single
    sustained bright tone with a rapid volume tremolo/flutter, which is
    what gives that cue its distinctive shimmer. The rarest and highest-
    priority event in the game deserves the longest, most deliberate
    flourish of the set."""
    b = SfxBuilder(7)
    b.patch(mod_mult=2, mod_tl=10, mod_ar=15, mod_dr=9, mod_sl=5, mod_rr=5, mod_wave=0,
            car_mult=1, car_tl=0, car_ar=15, car_dr=9, car_sl=0, car_rr=4, car_wave=0,
            feedback=7, connection=0)
    b.note_on(740)  # F#5 -- bright, sits above the busiest part of the music mix
    b.tremolo(cycles=6, curve=(0, 1, 2, 3, 5, 8, 12, 16, 12, 8, 5, 3, 2, 1, 0), step_ms=9)
    b.key_off()
    return b


def sfx_victory() -> SfxBuilder:
    """Level-7 victory fanfare: a longer, grander sibling of sfx_lvlclear's
    4-note riff -- same additive/chime patch for a consistent "fanfare"
    voice across the game, but a longer climbing run (C5-E5-G5-C6-G5-C6)
    into a held, tremolo-shimmered top note (see tremolo()'s docstring)
    so the whole thing resolves into a shimmer instead of just stopping,
    for the one screen in the game that should feel like the biggest
    payoff."""
    b = SfxBuilder(7)
    b.patch(mod_mult=1, mod_tl=4, mod_ar=15, mod_dr=6, mod_sl=4, mod_rr=6, mod_wave=0,
            car_mult=2, car_tl=0, car_ar=15, car_dr=5, car_sl=5, car_rr=5, car_wave=0,
            feedback=3, connection=1)
    for freq in (523.25, 659.25, 783.99, 1046.50, 783.99, 1046.50):  # C5 E5 G5 C6 G5 C6
        b.note_on(freq)
        b.wait_ms(130)
        b.key_off()
        b.wait_ms(20)
    b.note_on(1318.51)  # E6 -- final held note
    b.tremolo(cycles=8, curve=(0, 2, 5, 9, 14, 9, 5, 2), step_ms=10)
    b.key_off()
    return b


SFX: List[Tuple[str, str, Callable[[], SfxBuilder]]] = [
    ("PlyrFire", "Player firing (lowest priority -- fired constantly)", sfx_plyrfire),
    ("EnmyFire", "Enemy firing", sfx_enmyfire),
    ("EnmyDie", "Enemy destroyed", sfx_enmydie),
    ("PlyrHit", "Player takes damage but survives", sfx_plyrhit),
    ("PlyrDie", "Player destroyed", sfx_plyrdie),
    ("PickUp", "Energy/speed/power pickup collected", sfx_pickup),
    ("Tally", "Bonus-stage score/kill tally tick (PickUp, -6 TL)", sfx_tally),
    ("LvlClear", "Level-complete celebration fanfare", sfx_lvlclear),
    ("LowEnrgy", "Low-energy warning beep (channel 7, retriggered by sfx.c)", sfx_lowenrgy),
    ("XtraLife", "Extra life awarded (RPPacman-inspired tremolo shimmer)", sfx_extralife),
    ("Victory", "Level-7 win celebration fanfare", sfx_victory),
]


def cmd_list() -> None:
    for name, desc, _ in SFX:
        print(f"{name:<10} {desc}")


def write_xram_bundle() -> None:
    """Concatenates every SFX's headerless command stream (see
    SfxBuilder.to_command_bytes()) into one blob, music/sfx/sfx_xram.bin,
    and writes src/sfx_layout.h with each one's byte offset into it.

    This is what actually ships in the game: CMakeLists.txt loads this
    blob straight into XRAM at XRAM_SFX_DATA (the sfx_data member of
    src/xram.h's layout, read by rp6502_map()) as part of the ROM image, the same way it already does for sprite/tile
    bitmap data -- so sfx.c plays a clip directly out of XRAM instead of
    open()-ing a ROM: file on every single trigger (see sfx.c's module
    docstring for why that mattered: the RP6502 ROM: filesystem does a
    linear directory scan on every open(), a fixed cost regardless of file
    size, paid on every bullet fired). The individual music/sfx/*.vgm
    files this script also writes are no longer loaded by the game; they
    stay for standalone auditioning (any VGM player) and for this script's
    own structural round-trip.
    """
    offset = 0
    lines = [
        "#ifndef SFX_LAYOUT_H",
        "#define SFX_LAYOUT_H",
        "",
        "// Auto-generated by tools/generate_sfx.py from music/sfx/*.vgm's",
        "// command streams -- do not edit by hand, regenerate instead:",
        "//   python3 tools/generate_sfx.py",
        "//",
        "// Byte offsets into the XRAM_SFX_DATA region (src/xram.h) of each",
        "// clip's command stream. src/sfx.h's SFX_*_ADDR macros add these",
        "// to XRAM_SFX_DATA to get the absolute XRAM address",
        "// sfx_play_player()/sfx_play_enemy() take.",
    ]
    blob = bytearray()
    for name, _desc, builder_fn in SFX:
        data = builder_fn().to_command_bytes()
        macro = name.upper()
        lines.append(f"#define SFX_{macro}_OFFSET {offset}")
        lines.append(f"#define SFX_{macro}_LEN    {len(data)}")
        blob += data
        offset += len(data)

    # SFX_DATA_SIZE sizes the sfx_data member of src/xram.h's
    # xram_layout_t. sfx_data is the layout's last member today, but
    # anything placed after it -- mode configs and palettes above all --
    # must sit at an even XRAM address, and rp6502_map() fails the build
    # on any odd XRAM_* address. One clip landing at an odd length (this
    # bundle's is whatever the sum of all clips' individual byte counts
    # happens to be, not something authored) is enough to trip that, so
    # pad the whole bundle to an even size here rather than requiring
    # every clip to stay individually even. The pad byte sits past every
    # clip's own 0x66 end marker, so no reader ever reaches it.
    if offset % 2 != 0:
        blob += bytes([0x00])
        offset += 1

    lines.append("")
    lines.append(f"#define SFX_DATA_SIZE {offset}")
    lines.append("")
    lines.append("#endif")
    lines.append("")

    XRAM_BLOB_PATH.parent.mkdir(parents=True, exist_ok=True)
    XRAM_BLOB_PATH.write_bytes(bytes(blob))
    LAYOUT_HEADER_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(f"sfx_xram.bin ({len(blob)} bytes) -> {XRAM_BLOB_PATH}")
    print(f"sfx_layout.h -> {LAYOUT_HEADER_PATH}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="list all SFX and exit")
    parser.add_argument("--out-dir", default=str(MUSIC_SFX_DIR), help=f"output directory (default: {MUSIC_SFX_DIR})")
    args = parser.parse_args()

    if args.list:
        cmd_list()
        return 0

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, desc, builder_fn in SFX:
        data = builder_fn().to_vgm_bytes()
        out_path = out_dir / f"{name}.vgm"
        out_path.write_bytes(data)
        print(f"{name}.vgm ({len(data)} bytes) -> {out_path}  {desc}")

    write_xram_bundle()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
