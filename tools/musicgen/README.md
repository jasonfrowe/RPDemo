# RPStarHopper music generator

Procedurally composes original, royalty-free, Kraftwerk-leaning
electronic/synth OPL2 tracks for each of RPStarHopper's music slots, and
writes them straight out as [Furnace](https://github.com/tildearrow/furnace)
tracker `.fur` project files. Open the result in Furnace to audition and
tweak by hand; pass `--export-vgm` to also render straight to
`music/RESOURCE.NNN.vgm` using Furnace's own headless exporter.

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
to get the exact same track back (or omit it to roll the dice again).

`--track` accepts either the resource id (`001`, `RESOURCE.001`) or a
friendly alias (`title`, `boss`, `level1`, ...) — see `--list` for the full
table.

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
  per channel), but from narrow *synth-only* GM-program ranges specifically
  (`instruments.py`'s `lead_synth`/`bass_synth`/`pad_synth`: GM's synth
  lead/synth bass/synth pad programs), not the wider
  orchestral/mallet-percussion pools an earlier, Silpheed-inspired version
  of this tool used -- plus a fixed kick/snare/hat kit.
- **Structure**: a short intro layers instruments in one at a time — bass,
  pad, arpeggio, lead, and (if the track uses drums) drums — in a **randomized
  order** per song, so sometimes a track opens with just the drums, sometimes
  with a lone bassline, sometimes the harmony pad, etc. The body alternates
  between a primary chord progression and a contrasting one (with an
  optional drums-free bridge), sized to land the whole track around
  2-3 minutes. The outro then drops layers back out in reverse of however
  they came in, ending on whichever layer opened the track — so the loop
  point (VGM tracks loop) feels continuous instead of cutting to silence.
- **Motorik/sequenced, not evolving**: unlike a typical arrangement where
  parts vary bar to bar, the arp runs an *unbroken 16th-note sequence*
  cycling through the current chord (the Kraftwerk signature), and the lead
  picks one short motif shape for an *entire 4-bar chunk* rather than
  re-rolling it every bar -- both read as repeating, sequenced hooks/loops,
  not a melody that keeps wandering. Drums are equally mechanical:
  four-on-the-floor kick on every beat, snare on 2 & 4, unbroken 16th-note
  hats -- no syncopation or fills, deliberately, since the steady "machine"
  pulse is the point.
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
    compose.py              # the procedural composer described above
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
studied for structure/statistics only), and a later branch tried training a
small VAE on that same MIDI to generate melodies directly -- see
`tools/musicgen_vae/README.md` on `VAE-music-generation` for why that
didn't work out (a real data-scale ceiling, not a fixable bug). The current
version is a deliberate pivot away from both: fully procedural and
rule-based (no training data at all), leaning electronic/synth
(Kraftwerk-ish: minimal, motorik, arpeggiated) rather than orchestral --
both because that genre is much more naturally rule-describable than "write
a convincing melody," and because it suits the game's high tempo and OPL2's
FM-synth lineage. `instruments.py`'s `lead_synth`/`bass_synth`/`pad_synth`
GM ranges and `compose.py`'s motorik drum/sequenced-arp rules reflect that;
the original wider orchestral instrument pools (`lead`/`bass`/`pad`/`pluck`/
`keys`) are still defined in `instruments.py` but unused by `compose.py` now.
