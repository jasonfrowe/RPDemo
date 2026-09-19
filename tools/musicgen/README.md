# RPStarHopper music generator

Procedurally composes original, royalty-free OPL2 tracks for each of
RPStarHopper's music slots, in one of nine styles (`--style`, see below),
and writes them straight out as
[Furnace](https://github.com/tildearrow/furnace) tracker `.fur` project
files. Open the result in Furnace to audition and tweak by hand; pass
`--export-vgm` to also render straight to `music/RESOURCE.NNN.vgm` using
Furnace's own headless exporter.

(An earlier version of this tool leaned orchestral/Silpheed-inspired, and a
separate attempt to generate melodies with a small VAE trained on real
Silpheed MIDI didn't pan out -- see `tools/musicgen_vae/README.md` on the
`VAE-music-generation` branch for that writeup. This is a from-scratch
pivot to a fully procedural, rule-based electronic direction instead.)

Without `--export-vgm`, this tool never touches `music/RESOURCE.NNN.vgm` or
`CMakeLists.txt` — the workflow stops at handing you an editable `.fur`
file to audition and export yourself, in the GUI, whenever you're happy
with it.

## Quick start

```sh
python3 tools/generate_music.py --list                  # show all 9 track slots
python3 tools/generate_music.py --track all              # (re)generate every track, fresh roll
python3 tools/generate_music.py --track boss             # regenerate just one
python3 tools/generate_music.py --track boss --seed 1234 # reproduce a specific roll
python3 tools/generate_music.py --track boss --export-vgm  # also render straight to music/RESOURCE.009.vgm
```

Output goes to `music/fur/RESOURCE.NNN.fur`. Every run prints the seed it
used — save it if you like a particular roll, then pass `--seed` next time
to get the exact same track back (or omit it to roll the dice again). Every
run also prints a ready-to-copy `regenerate:` command with `--seed`,
`--vol`, and `--patch` all filled in from what actually got generated (see
below) — even if you didn't pass `--vol`/`--patch` yourself, so you always
have an exact, reproducible baseline to start hand-tuning from.

`--track` accepts either the resource id (`001`, `RESOURCE.001`) or a
friendly alias (`title`, `boss`, `level1`, ...) — see `--list` for the full
table.

## Hand-tuning a roll: `--vol` / `--patch`

Once a seed's *composition* (melody, progression, arrangement) is one you
like, `--vol` and `--patch` let you adjust the *mix* without re-rolling
anything else:

```sh
python3 tools/generate_music.py --track boss --seed 1234 \
    --vol 0,0,1,2,-3,-1,2 --patch 52,53,27,FD,FE,FF,5E
```

Both take 7 comma-separated values in **lead, arp, bass, pad, kick, snare,
hat** order (`compose.py`'s `ROLE_TO_CHANNEL` key order):

- `--vol`: a per-role integer offset from that role's base volume (can be
  negative), still clamped into OPL2's 0-63 pattern-volume range.
- `--patch`: a 2-digit hex instrument index (`00`-`FF`) into RPTracker's
  256-patch bank, replacing whichever instrument that role would otherwise
  have been assigned. Not restricted to the role's usual synth-only pool —
  any of the 256 patches works, if you want to experiment.

Both only make sense with a single `--track`, not `--track all`. Changing
`--vol`/`--patch` never perturbs the rest of the composition for a given
`--seed` — the same chord progression, arrangement, and melodic content
comes out either way, only the mix/instrumentation changes.

## `--style`: which composer runs

```sh
python3 tools/generate_music.py --track boss --style 5   # dubstep
```

| `--style` | Name | Character |
|---|---|---|
| 0 | `silpheed` | The original orchestral/mallet-percussion composer this tool started as. Evolving per-bar melody (a fresh lead shape every bar, not a repeating hook), 8th-note arp, syncopated kick + 2-and-4 snare backbeat, wide orchestral/mallet-percussion/organ instrument pools. |
| 1 (default) | `kraftwerk` | Motorik/sequenced electronic. Unbroken 16th-note arp sequence, four-on-the-floor bass locked to the kick, a short repeating lead motif (same shape for a whole 4-bar chunk). Drums: "motorik". |
| 2 | `daftpunk` | Syncopated house/funk. Arp plays short off-beat chord stabs (explicit note-off, not left ringing), bass is a syncopated 16th-note funk pattern, lead is the same repeating-hook shape as kraftwerk. Drums: "groove". |
| 3 | `trance` (Tiësto-leaning) | Uplifting trance. Bass is a rolling 16th-note arpeggio (every row filled, cycling root/3rd/5th) instead of a pulse, arp runs a wide two-octave plucked run, lead holds long sustained notes instead of chattering every beat. Drums: "motorik". |
| 4 | `bigroom` (Guetta-leaning) | Anthemic electro-house. Arp hits land only on the off-beat "and" of each beat and cut off fast (a sidechain-pump duck feel), bass is the daftpunk-style syncopated funk pattern, lead stays a simple repeating hook — big room lives and dies on one huge-sounding motif. Drums: "groove". |
| 5 | `dubstep` (Skrillex-leaning) | Aggressive half-time. Bass wobbles between root and an octave up on a chopped, syncopated 16th grid; arp fires irregular off-grid stabs with real silence between them; lead is sparse, punchy hits with lots of rest. Drums: new "halftime" pool (kick/snare at half the hats' rate). |
| 6 | `techno` (deadmau5-leaning) | Minimal/hypnotic progressive house. Bass is the same rolling arpeggio as trance, but the arp is mostly silence — an occasional single accent, not a continuous line — and the lead is sparse too, so the groove carries the track instead of a hook. Drums: "motorik". |
| 7 | `synthwave` | Retro 80s-leaning. Bass is a straight driving 8th-note pulse (not quarter notes), arp is the kraftwerk-style 16th sequence, lead holds long soaring notes like trance. Drums: "motorik". |
| 8 | `dnb` | Fast drum-and-bass/jungle energy. Bass is sparse, long sustained sub notes (mostly one per bar) under a busy syncopated 16th-note lead riff and a wide arpeggio roll. Drums: "groove" (this tool's amen_break/funky_drummer breakbeats). |

Style 0 has its own `_generate_chunk_silpheed` function and the wider
orchestral instrument pools. Styles 1-8 all run through one shared
`_generate_chunk_electronic` function and share instrument pools (OPL2
synth lead/bass/pad GM ranges) — what makes them sound different from each
other is `compose.py`'s `_STYLE_PARAMS` table, which assigns each style a
`drum_tag` (which `drum_patterns.py` pool) plus a `bass_mode`, `arp_mode`,
and `lead_mode`:

- **`bass_mode`**: `four_on_floor` (quarter-note pulse), `syncopated`
  (16th-note funk pattern), `arp` (rolling 16th arpeggio through the chord
  tones), `wobble` (chopped root/octave retriggers), `pulse8` (straight
  driving 8th notes), `legato` (sparse, long sustained notes).
- **`arp_mode`**: `sequencer` (unbroken 16th-note cycle), `stab` (short
  off-beat chord hits with a real note-off), `roll` (wide two-octave run),
  `pump` (off-beat hits simulating a sidechain duck), `chop` (irregular
  off-grid stabs with silence between them), `sparse` (mostly rests, rare
  single accent). **The arp channel isn't always a tight 3-note loop** —
  `sparse`/`chop`/`roll` all break that shape on purpose.
- **`lead_mode`**: `hook` (repeating quarter-note motif, same shape all
  chunk), `soaring` (long sustained notes), `sparse` (a few punchy hits
  with real rests), `syncopated` (16th-note off-grid riff).

Mixing these differently per style (rather than just swapping the drum
pattern) is what keeps the 9 styles from being reskins of one shape — see
`compose.py`'s module docstring for the full per-style rundown, and the
mode branches inside `_generate_chunk_electronic` for exactly what each one
does.

`--export-vgm` **overwrites the existing `music/RESOURCE.NNN.vgm` in
place** (equivalent to Furnace's File > Export > VGM) — it's meant for once
you're happy with a roll, not for casual browsing. Since these files are
tracked in git, an unwanted overwrite is just a `git checkout` away, but
audition in Furnace first if you're not sure yet. Use `--vgm-out-dir` to
render elsewhere instead.

**Every `.fur` this tool writes targets plain OPL2 (YM3812), never
OPL3/dual-chip**, so `--export-vgm` is always safe. This only becomes a
concern if you hand-edit a `.fur` in Furnace to add a second chip or switch
to OPL3 before exporting yourself — RPDemo's VGM player (`src/vgm.c`) only
understands a minimal opcode set and will desync on OPL3 port-1 writes or
anything else outside it.

## Manual FUR → VGM export from the command line

`--export-vgm` just wraps this Furnace CLI command, which you can also run
by hand (no GUI needed) for any `.fur` file, not just ones this tool wrote:

```sh
/Applications/Furnace.app/Contents/MacOS/furnace -loglevel error -subsong 0 -loops 1 -vgmout <output.vgm> <input.fur>
```

For example:

```sh
/Applications/Furnace.app/Contents/MacOS/furnace -loglevel error -subsong 0 -loops 1 -vgmout music/RESOURCE.001.vgm music/fur/RESOURCE.001.fur
```

Flag breakdown:

| Flag | Meaning |
|---|---|
| `-loglevel error` | quiets Furnace's debug/trace startup noise |
| `-subsong 0` | export the first (and in these files, only) subsong |
| `-loops 1` | render one extra pass through the loop before stopping — doesn't move the VGM loop point, just how much audio gets rendered |
| `-vgmout <path>` | output VGM path |
| *(trailing positional arg)* | the input `.fur` file |

Exits 0 on success and writes the file directly. As with `--export-vgm`,
make sure whatever you're exporting targets plain OPL2, not OPL3/dual-chip
(see above) — every `.fur` this tool generates already does.

## Track slots

Mapping confirmed by reading the game's source directly (`src/gameplay.c`,
`src/gameplay_boss.c`, `src/level_bonus.c`, `src/gameplay_game_over.c`,
`src/music.c`):

| Resource | Used for | Aliases | Intensity |
|---|---|---|---|
| `RESOURCE.001` | Title / attract screen | `title` | 0.10 |
| `RESOURCE.005` | Level 1 & 5 | `level1`, `level5` | 0.20 |
| `RESOURCE.003` | Level 2 & 6 | `level2`, `level6` | 0.35 |
| `RESOURCE.008` | Level 3 | `level3` | 0.40 |
| `RESOURCE.002` | Level 4 | `level4` | 0.50 |
| `RESOURCE.010` | Level 7 (late-game) | `level7` | 0.75 |
| `RESOURCE.009` | Boss stage (+ level 8+ fallback) | `boss` | 0.95 |
| `RESOURCE.006` | Bonus / reward round | `bonus`, `reward` | 0.15 |
| `RESOURCE.011` | Game over (win or lose) | `gameover` | 0.55 |

`RESOURCE.004/007/020/021/022` exist in `music/` but aren't wired into
`CMakeLists.txt` or referenced anywhere — dead leftovers, out of scope here.

"Intensity" (0.0-1.0) is each track's position on the game's arc, and drives
how harmonically restless its chord progressions are and whether it
modulates key — see "How a track is composed" below. Adjust it (and the
other `MoodPreset` fields) in `tracks.py` if a track should feel different.

## How a track is composed

Every generated track is built from the same handful of moving parts, all
seeded by `--seed` so a run is reproducible but varies a lot between seeds:

- **7 channels of the OPL2's 9** are used: lead, arpeggio, bass, pad
  (sustained harmony), kick, snare, hat. **Channels 7 and 8 are always left
  silent** — reserved for the game's own sound effects, never used by
  generated music.
- **Instruments** come from RPTracker's 256-patch `gm_bank`
  (`RPTracker/src/instruments.c`, parsed live so this tool never goes stale
  against it). All 256 are embedded in every `.fur` file so you can swap any
  instrument by hand in Furnace; the generator itself picks 7 of them (one
  per channel). Styles 1/2 pick from narrow *synth-only* GM-program ranges
  (`instruments.py`'s `lead_synth`/`bass_synth`/`pad_synth`); style 0 picks
  from the wider orchestral/mallet-percussion pools this tool originally
  used (`lead`/`bass`/`pad`/`pluck`/`keys`) -- plus a fixed kick/snare/hat
  kit either way.
- **Structure**: a short intro layers instruments in one at a time — bass,
  pad, arpeggio, lead, and (if the track uses drums) drums — in a **randomized
  order** per song, so sometimes a track opens with just the drums, sometimes
  with a lone bassline, sometimes the harmony pad, etc. The body alternates
  between a primary chord progression and a contrasting one (with an
  optional drums-free bridge), sized to land the whole track around
  2-3 minutes. The outro then drops layers back out in reverse of however
  they came in, ending on whichever layer opened the track — so the loop
  point (VGM tracks loop) feels continuous instead of cutting to silence.
- **Motorik/sequenced (styles 1-8), not evolving (style 0)**: styles 1-8
  pick one bass/arp/lead mode combination (see the `--style` table above)
  and hold shapes fixed *per 4-bar chunk*, not re-rolled every bar, so they
  read as repeating, sequenced hooks/loops. Style 0 re-rolls the lead's
  shape every bar instead, an evolving melody rather than a loop.
- **Drums** (styles 1-8) are real named beats transcribed from *Pocket
  Operations* (Teenage Engineering's drum-pattern reference book) --
  `musicgen/drum_patterns.py` -- picked once per chunk from that style's
  `drum_tag` pool ("motorik", "groove", or "halftime" -- see the `--style`
  table above), plus the same light per-bar touches regardless of which
  pattern was picked: an occasional syncopated kick push, hat drop-outs,
  and a snare roll on the chunk's last bar. Style 0's drums are a simpler
  hand-rolled syncopated-kick + 2-and-4-snare backbeat, not
  pattern-bank-driven.
- **The occasional semitone "gear change"**: about 1 in 5 tracks (seeded,
  so it varies run to run, not a fixed rule) transpose the second half of
  the final full-band return up a semitone into freshly-generated content
  before the outro -- the classic pop key-change trick, applied sparingly
  rather than every time. See `compose.py`'s `generate_track`
  (`semitone_bump`) and `_plan_sections`'s `A1_UP` chunk.
- **Chord progressions** are drawn from a 3-tier bank (simple → harmonically
  restless), weighted by the track's `intensity`. Title and early levels stay
  almost entirely in the simple tier; boss fights and late levels pull
  heavily from the restless tier. High-intensity tracks (intensity ≥ 0.65)
  also have a chance of **modulating to a different key** for the
  contrasting section — a real "raise the stakes" moment tied to boss
  fights and late levels, not just a different melody in the same key.
- **Tempo** is BPM-driven (not an arbitrary tracker "speed" value) — mostly
  124-140 BPM (house/techno territory, on the high-tempo side to match the
  game's pace), with the game-over track deliberately slower (100 BPM).
- **Mixing**: each role has a fixed volume level (`compose.py`'s
  `ROLE_VOLUME`) so the lead sits up front and the pad/hats sit underneath
  it rather than everything playing at the same raw level. OPL2's pattern
  volume column is 0-63, not 0-127 like most of Furnace's UI implies --
  confirmed directly against Furnace's own source
  (`DIV_CMD_GET_VOLMAX`) -- so if you hand-tune these, stay in that range.
- **No hanging notes across silent sections**: each of the 7 music
  channels' "this role isn't playing right now" pattern
  (`compose.py`'s `EMPTY_PATTERN_IDX`) starts with an explicit note-off,
  not just an empty row. Without that, a note still ringing from the
  previous, non-empty pattern would keep holding right through the
  "silent" one, since nothing ever told it to stop. Channels 7/8 are
  excluded from this -- generated music never touches them at all (they're
  reserved for the game's own sound effects), so their pattern stays
  genuinely empty rather than a tool with no business speaking on those
  channels writing a note-off into them.

None of this is a finished composition — it's a structured, genre-appropriate
starting sketch meant to be opened in Furnace and hand-edited from there.

## File layout

```
tools/
  generate_music.py       # CLI entry point
  musicgen/
    furwriter.py           # low-level .fur binary writer (see below)
    instruments.py         # parses RPTracker's gm_bank, GM-program-range role lookup
    tracks.py               # the 9-track registry + MoodPreset per track
    compose.py              # the procedural composer(s) described above
    drum_patterns.py         # Pocket Operations-transcribed drum pattern bank (styles 1-8)
music/
  fur/
    RESOURCE.NNN.fur        # generated output, one per track slot
```

Pure Python standard library — no dependencies to install.

### `furwriter.py`

Hand-writes the Furnace binary format (targeting format version 228 /
Furnace 0.6.8.1, the installed version) directly from `struct`/`zlib`, since
there's no Python library for it. This was built and verified against the
actual Furnace source (a local checkout's `papers/format.md` plus reading
`src/engine/fileOps/fur.cpp` and `src/engine/instrument.cpp` directly, which
is how two miscounted version-gated compat-flag byte runs got caught), and
every change to it should be re-verified the same way it originally was:

```sh
FUR=/Applications/Furnace.app/Contents/MacOS/furnace
$FUR -loglevel error -info    music/fur/RESOURCE.001.fur   # structural sanity check
$FUR -loglevel error -txtout  /tmp/out.txt music/fur/RESOURCE.001.fur  # human-readable dump
$FUR -loglevel error -subsong 0 -loops 1 -vgmout /tmp/out.vgm music/fur/RESOURCE.001.fur  # full export round-trip
```

(`--export-vgm` runs that same `-vgmout` command for you, straight into
`music/`, once you're happy with a track rather than debugging the writer.)

If you touch the binary writer, also re-check the exported VGM only uses
opcodes RPDemo's player supports (`0x5A`, `0x61-0x63`, `0x66`, `0x67`, a
handful of skip-N-byte opcodes — see `src/vgm.c`); nothing else should ever
appear as long as Furnace is exporting plain OPL2.

## Where this comes from

This tool started out inspired by the real Silpheed (1989) DOS score
(orchestral/mallet-percussion-heavy, per a Roland MT-32 MIDI capture
studied for structure/statistics only) -- that's `--style 0` now. A later
branch tried training a small VAE on that same MIDI to generate melodies
directly -- see `tools/musicgen_vae/README.md` on `VAE-music-generation`
for why that didn't work out (a real data-scale ceiling, not a fixable
bug). The project then pivoted to fully procedural, rule-based electronic
styles instead (no training data at all) -- both because that genre family
is much more naturally rule-describable than "write a convincing melody,"
and because it suits the game's high tempo and OPL2's FM-synth lineage --
starting with `--style 1` (kraftwerk) and `--style 2` (daftpunk).
`drum_patterns.py`'s beats are transcribed directly from *Pocket
Operations* (Teenage Engineering's drum-machine-pattern reference book),
rather than hand-rolled, once a single hand-rolled motorik beat turned out
to feel too static repeated for a whole song.

Styles 3-8 (trance, bigroom, dubstep, techno, synthwave, dnb) followed once
regenerating a song with the same style started sounding too much like the
last one -- each borrows its overall feel from a well-known electronic
artist/genre (Tiësto, Guetta, Skrillex, deadmau5, ...) but isn't trying to
imitate any specific track; it's a different combination of the same
bass/arp/lead "mode" building blocks described under `--style` above, which
is what actually makes them read as distinct rather than just reskins with
a different drumbeat.
