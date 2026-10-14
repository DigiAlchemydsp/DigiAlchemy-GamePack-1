# DIGIDOOM — handoff

A tiny **game launcher + three games** that run on the **Elektron Digitone mk1 /
Digitone Keys (OS 1.43)** screen, shipped as one
[elekloader](https://github.com/irpina/elekloader) mod.

Latest build: **v0.21**. Built, linted, patched, and verified in **digiemu**.

This file is the working handoff. Read `README.md` for the user view and
`docs/GAME-MAKING.md` for how to write another game on this platform.

---

## 0. TL;DR state

| | |
|---|---|
| mod id / version | `digidoom` / `0.21` |
| target | Digitone mk1 / Keys, OS 1.43 (main section sha256 `3831a477…`) |
| budget | `.run 11863` + `.bss 3096` = **~15 KB of 128 KB**; 0 `.fast` |
| sites | **0** — subscribes to core events only (combinable with any mod) |
| imports | none (only `core`) |
| builds | `digidoom-0.21.elemod`, patched `test-digidoom-0.21.syx` (version `0.1L`) |
| verified | boots to a live UI; menu opens; DOOM/INVADERS/SNAKE all launch and play |

Three games behind a `MIDI+YES` **game-select menu**:

- **DOOM** — from-scratch DOOM-style raycaster (dither walls, billboard sprites,
  HUD, weapon, doors, monsters; title `DOOM — KARL MARX EDITION`).
- **INVADERS** — very lightweight Space Invaders (6×4 fleet, ship, bullets,
  lives, score).
- **SNAKE** — grid snake that grows and speeds up with every dot.

---

## 1. Repo layout

```
mods/digidoom/digidoom.c   the launcher + all three games (one file, ~1000 lines)
mods/digidoom/mod.json     the elemod manifest (ev_draw/ev_tick/ev_key, no sites)
mods/digidoom/out/         built .elemod(s) + digidoom.work/ (objects, mod.ld)
digidoom-0.21.elemod       a convenience copy of the latest mod
testout/                   patched .syx + emulator screenshots + *.map.json
README.md                  user-facing overview
docs/GAME-MAKING.md        how to write a game on this platform
HANDOFF.md                 this file
```

`Smallest-Doom-rp2040/` (the RP2040 Chocolate-Doom fork) sits next to this
folder as the *reference*, not as a build target — see §7.

---

## 2. Environment (paths on this machine)

| what | where |
|---|---|
| m68k toolchain | `C:\SysGCC\m68k-elf\bin` (gcc 4.8.0) |
| elekloader | `C:\Users\benan\Music\ELEKTRON\elekloader` |
| core (1.43) | `…\elekloader\mods\core-dn1\out\core-2.0a.elemod` |
| stock 1.43 | `C:\Users\benan\Music\ELEKTRON\Digitone_and_Digitone_Keys_OS1.43.syx` |
| digiemu | `C:\Users\benan\Music\ELEKTRON\digiemu-main` |
| patched-Unicorn venv | `C:\Users\benan\AppData\Local\Temp\opencode\digiemu-venv` |

---

## 3. Build / lint / patch

```powershell
$env:PATH             = "C:\SysGCC\m68k-elf\bin;" + $env:PATH
$env:ELEKLOADER_CROSS = "m68k-elf-"
$env:PYTHONPATH       = "C:\Users\benan\Music\ELEKTRON\elekloader"
$stock = "C:\Users\benan\Music\ELEKTRON\Digitone_and_Digitone_Keys_OS1.43.syx"
$core  = "C:\Users\benan\Music\ELEKTRON\elekloader\mods\core-dn1\out\core-2.0a.elemod"

python -m elekloader.sdk.build mods\digidoom --stock $stock
python -m elekloader.lint  mods\digidoom\out\digidoom-0.21.elemod --stock $stock --with $core
python -m elekloader.patch --stock $stock --mod $core `
    --mod mods\digidoom\out\digidoom-0.21.elemod `
    --out testout\test-digidoom-0.21.syx --version 0.1L
```

Expect `BUILT …`, `OK: links as core 2.0a > digidoom 0.21`, and `WROTE …` after
`verified: …`. `patch` also writes `testout\test-digidoom-0.21.syx.map.json`
(every symbol's absolute address — use it, never guess RAM offsets).

Recovery on hardware: hold **FUNC** while powering on for the startup menu,
press **TRIG 4** (OS UPGRADE), then send the stock `.syx`.

---

## 4. Testing in digiemu (the recipe that works here)

`emu.portable --add` builds the firmware's snapshots and settles the UI:

```powershell
$py = "C:\Users\benan\AppData\Local\Temp\opencode\digiemu-venv\Scripts\python.exe"
$env:PYTHONPATH = "C:\Users\benan\Music\ELEKTRON\digiemu-main"
& $py -m emu.portable --add testout\test-digidoom-0.21.syx --yes
# -> …\digiemu-main\portable\firmware\dn1-0.1l-<hash>\snapshots\Digitone_OS0.1L\gui.snap
```

Then resume that snapshot, inject panel keys, and read frames. A small script
does it (see `testout` screenshots for the outputs). The gotchas we hit:

- **`emu.config` needs the extracted sections**: set
  `$env:DT2_SECTIONS` to the firmware's `…\sections` dir.
- **Manifest must match**: `longrun.build(...)` must use the flags the snapshot
  was saved with, including `unblock_except=(0x4137B93C,)` (the frame
  semaphore) and `deferred_components=('timers',)`.
- **Identify the device from the toml**, not by hash: `devices.load(
  …\digiemu-main\devices\digitone.toml)`. `devices.identify` refuses a patched
  `.syx`.
- **Input lag**: `panelin.feed` needs ~2 M instructions before the UI task acts
  on a key. Space the `press`/`release` feeds accordingly.
- **Untorn frames**: latch at `profile.panel_diff` and read with
  `panel.read(m, profile.fb_front)`; write with `panel.write_png`.
- **The emulator cannot tolerate hooking the panel-present entry points**
  (`panel_diff`/`panel_flush`) — the *proven* DigiScreen overlay mod also fails
  to reach a live UI there. That is why this mod renders through `ev_draw`
  (§5), not a present hook. On hardware both are the same technique.

---

## 5. Architecture

The mod is a **format-2 elekloader mod** with **no sites**; it only subscribes to
the core hook bus (so it combines with any other mod):

| event | rate | use |
|---|---|---|
| `ev_draw(bmp, ctrl)` | per composed frame | draw the current screen |
| `ev_tick(ctrl)` | 30 Hz | advance game logic; request a recompose |
| `ev_key(brain, ev)` | per key event | input; return nonzero to swallow |

- **Coordinates**: the firmware's `y = 0` is the **bottom** row.
- **Drawing**: only `DN_VLINE`, `DN_FILLRECT`, `DN_TEXTF` (+ the stock 5-px
  font). No knowledge of the Bitmap's memory layout is needed.
- **No libc / no float**: integer only; fixed point Q15 for Doom, small ints for
  the arcade games.
- **State machine** (`g_state`): `S_MENU → S_DOOM_TITLE/PLAY/DEAD`,
  `S_INV_PLAY/OVER`, `S_SNAKE_PLAY/OVER`. `render()` and `digidoom_key()`
  dispatch on it.
- **Redraw gating** (`digidoom_tick`): the dominant cost is the recompose (it
  also redraws every stock view). The mod asks for a recompose only on a fresh
  key press, on a ~7 Hz title blink, or at ~15 Hz during play; static screens
  (menu, game-over, snake-between-moves) cost ~nothing. Measured: menu ≈ 4
  draws / 128 ticks; play ≈ 69 / 129.
- **Sprites**: 1-bpp bitmaps as `u16` rows, **bit 15 = leftmost**; projected as
  billboards with a per-column z-buffer (`zbuf`), distance-dithered.
- **Walls**: grid DDA; overflow-safe at Q15 by keeping ray deltas in Q8
  (`ddx = (1<<23)/|rdx|`); ordered-dither (4×4) drawn as short `VLINE` runs.
- **Text**: `DN_TEXTF` for strings/`%d`; `dgm_strw`/`dgm_glyph`/`dgm_text` for
  **centered** and **2× scaled** text from the font tables (`DN_FW` `0x402315DC`,
  `DN_FO` `0x402316DC`, `DN_FB` `0x402318DC`).
- **Effects**: a small `g_fx[]` list draws a black patch + a glyph (`x`/`*`) on
  hits/impacts.

Key codes (measured, `devices/digitone.toml`): `MIDI`=2, `YES`=13, `NO`=14,
`UP`=15, `DOWN`=16, `LEFT`=17, `RIGHT`=18, `FUNC`=1. `ev+12` = key id,
`ev+16` = flags (`1` press, `8` repeat, `0x10` …).

---

## 6. Known issues / open items

- **Sound**: none. The mod is display/UI only; the FM engine is untouched.
- **INVADERS** has no shields and no UFO.
- **SNAKE** has no border, no high score, no wrap option.
- **DOOM** has one map, one imp + one demon, one weapon.
- **Redraw rate** is fixed at ~15 Hz for play; `(g_ticks & 1)` in
  `digidoom_tick` can be `& 3` / `& 7` for more headroom.
- The **Digitone Keys** keyboard/wheels are untested (only the mk1 panel).
- **OS 1.44**: not ported (all addresses are 1.43; see `devices/digitone_mk1.py`).

---

## 7. Why this is from-scratch, not a Doom port

The reference is `Smallest-Doom-rp2040` (a Chocolate Doom fork). A literal port
is impossible on the Digitone: it needs a **1.8 MB WAD** mmap'd from flash and
MBs of zone RAM, while a mod gets **128 KB total** and has no bulk-storage path.
So DIGIDOOM keeps the *spirit* (a grid raycaster) and the ideas (DDA, camera
plane, distance shading), not the code.

---

## 8. Next steps (suggested)

1. SNAKE border + high score; INVADERS shields/UFO; a second DOOM map.
2. A shared high-score store (per-pattern or a named resource).
3. Optional: hook the panel present on hardware to render without forcing the
   stock recompose (bigger CPU win; cannot be tested in this emulator).
4. Port to OS 1.44 (re-find the font/primitive addresses).

---

## 9. Repo hygiene — never distribute firmware

This repo ships **our** code and assets only. The flashable `.syx` is built
locally from the user's own stock OS (`elekloader.patch`); it is **never**
committed. `.gitignore` excludes `*.syx` (+ `.json`/`.map.json`), the
third-party `Smallest-Doom-rp2040/` tree and its archive, and build artifacts.
`NOTICE` records what is (and isn't) redistributed.

Two guards back it up:

```powershell
# one-time: enable the committed pre-commit hook
git config core.hooksPath scripts/hooks

# any time: verify nothing firmware-ish is tracked
sh scripts/check-no-firmware.sh
```

Both fail if a `*.syx`/`*.whx`/`*.whd` file or the reference tree is ever
tracked. Note: a `gh` token without the `workflow`/`delete_repo` scopes cannot
push a `.github/workflows/*` file or delete the repo — that is why the guard is
a hook + script, not CI.

## 10. Reference docs

- `README.md` — user overview, controls, screenshots.
- `docs/GAME-MAKING.md` — the platform API, tools and optimizations.
- `..\digitone_corea\DSP.md`, `HANDOFF.md` — the Core A fabric and the FX suite.
- `..\elekloader-modding-guide\` — the hook bus and format-2 mods.
- `..\elekloader\docs\ADAPTING.md`, `FORMAT.md` — the loader's rules.
