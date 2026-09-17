"""Low-level writer for Furnace tracker (.fur) module files.

Targets the "legacy" (pre-240) file format used by Furnace 0.6.8.1 (format
version 228), which is what is installed at /Applications/Furnace.app. The
binary layout implemented here was cross-checked directly against the
Furnace source (src/engine/fileOps/fur.cpp, src/engine/instrument.cpp) and
papers/format.md / papers/newIns.md in a local Furnace checkout, not just
the docs, since the docs are occasionally ahead of what a given release
actually reads.

Files are written uncompressed (no zlib) -- Furnace's loader falls back to
raw bytes when a file isn't zlib data, so this is valid and much easier to
eyeball/debug.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

FORMAT_VERSION = 228
SYSTEM_OPL2 = 0x90  # OPL2 (YM3812) - 9 channels
DIV_INS_OPL = 14

MAX_CHIPS = 32


# ---------------------------------------------------------------------------
# instrument model
# ---------------------------------------------------------------------------


@dataclass
class FurOp:
    """One OPL2 FM operator, in Furnace's OPL feature-block field names.

    Fields map 1:1 onto real OPL2 register bits (confirmed against
    src/engine/platform/opl.cpp register-write code):
      am/vib/sus(EGT)/ksr/mult -> register 0x20+op
      ksl/tl                   -> register 0x40+op
      ar/dr                    -> register 0x60+op
      sl/rr                    -> register 0x80+op
      ws                       -> register 0xE0+op
    """

    am: int = 0
    vib: int = 0
    sus: int = 0  # EGT (envelope type)
    ksr: int = 0
    mult: int = 0
    ksl: int = 0
    tl: int = 0
    ar: int = 0
    dr: int = 0
    sl: int = 0
    rr: int = 0
    ws: int = 0
    kvs: int = 2  # key-velocity sensitivity; 2 = "auto" (Furnace default)


@dataclass
class FurInstrument:
    name: str
    alg: int  # 0 or 1 -- connection/algorithm bit (register 0xC0 bit 0)
    fb: int  # 0-7 -- feedback (register 0xC0 bits 1-3)
    op0: FurOp
    op1: FurOp

    @staticmethod
    def from_opl_registers(name: str, m_ave: int, m_ksl: int, m_atdec: int, m_susrel: int, m_wave: int,
                            c_ave: int, c_ksl: int, c_atdec: int, c_susrel: int, c_wave: int,
                            feedback: int) -> "FurInstrument":
        """Build from raw OPL2 register bytes, as stored in RPTracker's OPL_Patch."""

        def unpack_op(ave: int, ksl_byte: int, atdec: int, susrel: int, wave_byte: int) -> FurOp:
            return FurOp(
                am=(ave >> 7) & 1,
                vib=(ave >> 6) & 1,
                sus=(ave >> 5) & 1,
                ksr=(ave >> 4) & 1,
                mult=ave & 0xF,
                ksl=(ksl_byte >> 6) & 3,
                tl=ksl_byte & 0x3F,
                ar=(atdec >> 4) & 0xF,
                dr=atdec & 0xF,
                sl=(susrel >> 4) & 0xF,
                rr=susrel & 0xF,
                ws=wave_byte & 0x3,
            )

        return FurInstrument(
            name=name,
            alg=feedback & 1,
            fb=(feedback >> 1) & 0x7,
            op0=unpack_op(m_ave, m_ksl, m_atdec, m_susrel, m_wave),
            op1=unpack_op(c_ave, c_ksl, c_atdec, c_susrel, c_wave),
        )


# ---------------------------------------------------------------------------
# pattern / song model
# ---------------------------------------------------------------------------


@dataclass
class RowCell:
    note: Optional[int] = None  # 0=C-(-5) .. 179=B-9, 180=off, 181=release
    ins: Optional[int] = None
    vol: Optional[int] = None

    def is_empty(self) -> bool:
        return self.note is None and self.ins is None and self.vol is None


NOTE_OFF = 180
NOTE_RELEASE = 181


def note_value(octave: int, semitone: int) -> int:
    """semitone: 0=C, 1=C#, ... 11=B. octave: Furnace octave numbering (C-4 = middle-ish)."""
    return (octave + 5) * 12 + semitone


@dataclass
class FurSong:
    name: str
    author: str = "RPStarHopper"
    category: str = "RPStarHopper"
    system_name: str = "OPL2 (YM3812)"
    hz: float = 60.0
    speed: int = 6
    pattern_len: int = 32
    highlight_a: int = 4
    highlight_b: int = 16
    channels: int = 9
    instruments: List[FurInstrument] = field(default_factory=list)
    # orders[channel] -> list of pattern indices, one per order row. All
    # channel lists must be the same length.
    orders: List[List[int]] = field(default_factory=list)
    # (channel, pattern_index) -> {row: RowCell}
    patterns: Dict[Tuple[int, int], Dict[int, RowCell]] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# byte-level helpers
# ---------------------------------------------------------------------------


class Buf:
    def __init__(self) -> None:
        self.data = bytearray()

    def tell(self) -> int:
        return len(self.data)

    def u8(self, v: int) -> None:
        self.data += struct.pack("<B", v & 0xFF)

    def i8(self, v: int) -> None:
        self.data += struct.pack("<b", v)

    def u16(self, v: int) -> None:
        self.data += struct.pack("<H", v & 0xFFFF)

    def i32(self, v: int) -> None:
        self.data += struct.pack("<i", v)

    def u32(self, v: int) -> None:
        self.data += struct.pack("<I", v & 0xFFFFFFFF)

    def f32(self, v: float) -> None:
        self.data += struct.pack("<f", v)

    def raw(self, b: bytes) -> None:
        self.data += b

    def cstr(self, s: str) -> None:
        self.data += s.encode("utf-8") + b"\x00"

    def pad(self, n: int) -> None:
        self.data += bytes(n)

    def patch_u32(self, pos: int, v: int) -> None:
        struct.pack_into("<I", self.data, pos, v & 0xFFFFFFFF)

    def patch_i32(self, pos: int, v: int) -> None:
        struct.pack_into("<i", self.data, pos, v)


# ---------------------------------------------------------------------------
# instrument (INS2) block
# ---------------------------------------------------------------------------


def _feature(out: Buf, code: bytes, payload: bytes) -> None:
    out.raw(code)
    out.u16(len(payload))
    out.raw(payload)


def _fm_op_bytes(op: FurOp) -> bytes:
    b = bytearray(8)
    b[0] = ((op.ksr & 1) << 7) | (op.mult & 0xF)
    b[1] = ((op.sus & 1) << 7) | (op.tl & 0x7F)
    b[2] = ((op.vib & 1) << 5) | (op.ar & 0x1F)
    b[3] = ((op.am & 1) << 7) | ((op.ksl & 3) << 5) | (op.dr & 0x1F)
    b[4] = ((op.kvs & 3) << 5)
    b[5] = ((op.sl & 0xF) << 4) | (op.rr & 0xF)
    b[6] = 0
    b[7] = op.ws & 0x7
    return bytes(b)


def _build_instrument_block(ins: FurInstrument) -> bytes:
    out = Buf()
    out.raw(b"INS2")
    size_pos = out.tell()
    out.u32(0)  # placeholder
    content_start = out.tell()

    out.u16(FORMAT_VERSION)  # format version field; ignored by reader
    out.u16(DIV_INS_OPL)

    _feature(out, b"NA", ins.name.encode("utf-8") + b"\x00")

    fm = bytearray()
    fm.append(0x30 | 2)  # op0+op1 enabled, opCount=2
    fm.append(((ins.alg & 7) << 4) | (ins.fb & 7))
    fm.append(0)  # tremLFO/ams/fms -- unused by OPL
    fm.append(0)  # fmsLFO/amsLFO/ops4-flag/opllPreset -- unused for 2-op OPL
    fm.append(0)  # block (>=224) -- legacy pitch artifact, unused
    fm += _fm_op_bytes(ins.op0)
    fm += _fm_op_bytes(ins.op1)
    _feature(out, b"FM", bytes(fm))

    _feature(out, b"LD", bytes(7))  # fixedFreqMode=0, kick/snare/tom freq=0

    out.raw(b"EN")

    out.patch_u32(size_pos, out.tell() - content_start)
    return bytes(out.data)


# ---------------------------------------------------------------------------
# pattern (PATN) block
# ---------------------------------------------------------------------------


def _encode_pattern_rows(rows: Dict[int, RowCell], pattern_len: int) -> bytes:
    out = Buf()
    row = 0
    while row < pattern_len:
        cell = rows.get(row)
        if cell is None or cell.is_empty():
            run_end = row
            while run_end < pattern_len and (rows.get(run_end) is None or rows[run_end].is_empty()):
                run_end += 1
            remaining = run_end - row
            while remaining > 0:
                if remaining == 1:
                    out.u8(0x00)
                    remaining -= 1
                else:
                    chunk = min(remaining, 129)
                    out.u8(0x80 | (chunk - 2))
                    remaining -= chunk
            row = run_end
            continue

        mask = 0
        body = Buf()
        if cell.note is not None:
            mask |= 1
        if cell.ins is not None:
            mask |= 2
        if cell.vol is not None:
            mask |= 4
        out.u8(mask)
        if cell.note is not None:
            out.u8(cell.note)
        if cell.ins is not None:
            out.u8(cell.ins)
        if cell.vol is not None:
            out.u8(cell.vol)
        row += 1

    out.u8(0xFF)
    return bytes(out.data)


def _build_pattern_block(channel: int, pattern_idx: int, rows: Dict[int, RowCell], pattern_len: int) -> bytes:
    out = Buf()
    out.raw(b"PATN")
    size_pos = out.tell()
    out.u32(0)
    content_start = out.tell()

    out.u8(0)  # subsong index
    out.u8(channel)
    out.u16(pattern_idx)
    out.cstr("")  # pattern name
    out.raw(_encode_pattern_rows(rows, pattern_len))

    out.patch_u32(size_pos, out.tell() - content_start)
    return bytes(out.data)


# ---------------------------------------------------------------------------
# full song assembly
# ---------------------------------------------------------------------------


def build_fur_bytes(song: FurSong) -> bytes:
    if not song.orders or any(len(o) != len(song.orders[0]) for o in song.orders):
        raise ValueError("orders must be a non-empty list with one equal-length list per channel")
    if len(song.orders) != song.channels:
        raise ValueError("orders must have one entry per channel")
    if len(song.instruments) > 256:
        raise ValueError("too many instruments (max 256)")

    orders_len = len(song.orders[0])
    ins_len = len(song.instruments)

    # unique (channel, patternIndex) combinations actually referenced by the
    # order list -- these are the PATN blocks we need to emit.
    pat_keys: List[Tuple[int, int]] = []
    seen = set()
    for ch in range(song.channels):
        for pat_idx in song.orders[ch]:
            key = (ch, pat_idx)
            if key not in seen:
                seen.add(key)
                pat_keys.append(key)

    out = Buf()

    # ---- header ----
    out.raw(b"-Furnace module-")
    out.u16(FORMAT_VERSION)
    out.pad(2)
    song_info_ptr_pos = out.tell()
    out.i32(0)  # placeholder, patched below
    out.pad(8)

    song_info_ptr = out.tell()
    out.patch_i32(song_info_ptr_pos, song_info_ptr)

    # ---- INFO block ----
    out.raw(b"INFO")
    info_size_pos = out.tell()
    out.u32(0)
    info_content_start = out.tell()

    out.u8(0)  # time base (0 -> speed values used as-is)
    out.u8(song.speed)  # speed 1
    out.u8(song.speed)  # speed 2
    out.u8(1)  # initial arpeggio time
    out.f32(song.hz)

    out.u16(song.pattern_len)
    out.u16(orders_len)
    out.u8(song.highlight_a)
    out.u8(song.highlight_b)

    out.u16(ins_len)
    out.u16(0)  # wavetable count
    out.u16(0)  # sample count
    out.i32(len(pat_keys))  # global pattern count

    # chip id list (32 slots)
    out.u8(SYSTEM_OPL2)
    out.pad(MAX_CHIPS - 1)
    # chip volumes / panning (reserved since >=135, still present)
    out.pad(MAX_CHIPS)
    out.pad(MAX_CHIPS)
    # chip flag pointers (32 x i32) -- 0 = no FLAG block, use chip defaults
    out.pad(MAX_CHIPS * 4)

    out.cstr(song.name)
    out.cstr(song.author)
    out.f32(440.0)  # A-4 tuning

    # compatibility flags (>=37): 20 single-byte flags, all "off"/modern
    out.pad(20)

    # pointers to instruments/wavetables/samples/patterns -- placeholders
    ins_ptr_pos = out.tell()
    out.pad(ins_len * 4)
    # (0 wavetables, 0 samples -- their pointer arrays are empty, nothing to write)
    pat_ptr_pos = out.tell()
    out.pad(len(pat_keys) * 4)

    # orders: channel-major, orders_len bytes per channel
    for ch in range(song.channels):
        for pat_idx in song.orders[ch]:
            out.u8(pat_idx)

    # effect columns per channel (must be >=1)
    for _ in range(song.channels):
        out.u8(1)

    # channel show / collapse / names / short names (>=39)
    for _ in range(song.channels):
        out.u8(1)  # show
    for _ in range(song.channels):
        out.u8(0)  # collapse
    for _ in range(song.channels):
        out.cstr("")  # channel name
    for _ in range(song.channels):
        out.cstr("")  # channel short name
    out.cstr("")  # song comment/notes

    out.f32(1.0)  # master volume

    # extended compat flags (>=70): 28 single-byte flags, all modern/off
    out.pad(28)

    # virtual tempo (numerator, denominator)
    out.u16(1)
    out.u16(1)

    # subsong block (>=95): first subsong name/comment, count of *additional*
    # subsongs (0 -- everything lives in this one first subsong)
    out.cstr("")
    out.cstr("")
    out.u8(0)
    out.pad(3)
    # (no additional subsong pointers, count is 0)

    # additional metadata (>=103)
    out.cstr(song.system_name)
    out.cstr(song.category)
    out.cstr("")
    out.cstr("")
    out.cstr("")
    out.cstr("")

    # system output config (>=135): one chip
    out.f32(1.0)  # chip volume
    out.f32(0.0)  # chip panning
    out.f32(0.0)  # chip front/rear balance
    out.i32(0)  # patchbay connection count

    out.u8(1)  # automatic patchbay (>=136)

    # a couple more compat flags (>=138): 8 single-byte flags, all modern/off
    out.pad(8)

    # speed pattern of first song (>=139)
    out.u8(1)  # length
    for _ in range(16):
        out.u8(song.speed)

    # grooves (>=139)
    out.u8(0)  # groove count

    # asset directory pointers (>=156) -- placeholders, patched below once
    # the (empty) ADIR blocks they point to have been written
    asset_dir_ptr_pos = out.tell()
    out.i32(0)
    out.i32(0)
    out.i32(0)

    out.patch_u32(info_size_pos, out.tell() - info_content_start)

    # ---- asset directories (instrument/wavetable/sample) ----
    # Furnace's loader unconditionally seeks to and parses these three once
    # version>=156, even if empty, so they must be real (empty) ADIR blocks.
    for i in range(3):
        adir_off = out.tell()
        out.raw(b"ADIR")
        adir_size_pos = out.tell()
        out.u32(0)
        adir_content_start = out.tell()
        out.i32(0)  # number of directories
        out.patch_u32(adir_size_pos, out.tell() - adir_content_start)
        out.patch_i32(asset_dir_ptr_pos + i * 4, adir_off)

    # ---- instrument blocks ----
    ins_offsets: List[int] = []
    for ins in song.instruments:
        ins_offsets.append(out.tell())
        out.raw(_build_instrument_block(ins))

    # ---- pattern blocks ----
    pat_offsets: List[int] = []
    for ch, pat_idx in pat_keys:
        pat_offsets.append(out.tell())
        rows = song.patterns.get((ch, pat_idx), {})
        out.raw(_build_pattern_block(ch, pat_idx, rows, song.pattern_len))

    # ---- backpatch pointer tables ----
    for i, off in enumerate(ins_offsets):
        out.patch_i32(ins_ptr_pos + i * 4, off)
    for i, off in enumerate(pat_offsets):
        out.patch_i32(pat_ptr_pos + i * 4, off)

    return bytes(out.data)


def write_fur(path: str, song: FurSong) -> None:
    with open(path, "wb") as f:
        f.write(build_fur_bytes(song))
