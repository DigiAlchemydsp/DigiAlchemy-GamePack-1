# Making a game for the Digitone

Everything DIGIDOOM learned about writing a small game that runs on the
**Elektron Digitone mk1 / Digitone Keys (OS 1.43)** screen as an
[elekloader](https://github.com/irpina/elekloader) mod: the tools, the firmware
functions we call, the coordinate/format conventions, and the optimizations that
keep the CPU sane. It is written from the code in `../mods/digidoom/`.

> Not affiliated with or endorsed by Elektron. Flashing modified firmware is at
> your own risk; know the recovery path first (hold **FUNC** on power-up → OS
> UPGRADE → send the stock `.syx`).

---

## 0. The platform in one paragraph

The Digitone's **main CPU** (a ColdFire MCF5441x, m68k) runs the UI, the
sequencer and the audio mix. An elekloader **format-2 mod** is a small
relocatable blob the loader copies into a **128 KB** RAM area at boot; it
subscribes to the firmware's **hook bus** (events) and calls the firmware's own
routines at fixed addresses. There is **no C library and no float**. The screen
is **128×64, 1 bit per pixel**. A game is a state machine on `ev_tick` that
draws on `ev_draw` and reads keys on `ev_key`.

---

## 1. The tools

| tool | command | what |
|---|---|---|
| cross toolchain | `C:\SysGCC\m68k-elf\bin` | `m68k-elf-gcc`/`as`/`ld` (4.8.0) |
| build | `python -m elekloader.sdk.build MODDIR --stock STOCK.syx` | compiles sources → `.elemod` |
| lint | `python -m elekloader.lint MOD.elemod --stock STOCK.syx --with CORE.elemod` | checks it links |
| patch | `python -m elekloader.patch --stock STOCK.syx --mod CORE --mod MOD --out test.syx --version XXXX` | builds a flashable `.syx` |
| emulator | `python -m emu.portable --add test.syx --yes` (digiemu) | boots/settles, saves a snapshot |
| symbol map | `test.syx.map.json` (written by `patch`) | every symbol's absolute address |

Environment:

```powershell
$env:PATH             = "C:\SysGCC\m68k-elf\bin;" + $env:PATH
$env:ELEKLOADER_CROSS = "m68k-elf-"
$env:PYTHONPATH       = "<elekloader checkout>"
```

The SDK compiles your `.c`/`.s` with the device flags
(`-mcpu=54455 -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie`) and
links them into the four sections the loader places: `.run` (RAM code+data),
`.bss`, `.fast` (fast SRAM, rarely needed), `.boot` (core only).

---

## 2. `mod.json` — a game with no sites

A game that only *observes* the UI owns no firmware bytes, so it **combines with
any other mod** (including the FX suite). It requires `core` (the hook bus).

```json
{
 "id": "my-game", "version": "1.0",
 "title": "My game", "category": "Games", "author": "you",
 "license": "GPL-2.0-or-later",
 "description": "One or two sentences.",
 "device": "digitone-mk1", "os": "1.43",
 "sources": ["mygame.c"],
 "subscribe": [
  {"event": "ev_draw", "fn": "my_draw", "order": 60},
  {"event": "ev_tick", "fn": "my_tick", "order": 60},
  {"event": "ev_key",  "fn": "my_key",  "order": 60}
 ],
 "requires": ["core"]
}
```

`order` sorts handlers of one event (lower first). See
`..\elekloader\docs\ADAPTING.md` for the full field list, `..\elekloader-modding-guide\`
for the hook bus, and `FORMAT.md` for the format.

---

## 3. The hook bus (events we use)

Handlers are ordinary C functions: **arguments on the stack**, `d0`/`d1`/`a0`/`a1`
free, the result in `d0`; everything else preserved (the standard m68k SysV
convention, so plain C works).

| event | when | prototype | notes |
|---|---|---|---|
| `ev_tick` | 30 Hz, UI task | `void f(void *ctrl)` | set `*((u8*)ctrl + 0x20) = 1` to request a recompose |
| `ev_draw` | after each frame is composed | `void f(void *bmp, void *ctrl)` | draw over the frame; `y = 0` is the **bottom** row |
| `ev_key` | each key event | `int f(void *brain, void *ev)` | return **nonzero to swallow** the key |
| `ev_enc` | each encoder turn | `int f(void *brain, void *ev)` | `ev+12` = encoder 1–8 (A–H), `ev+16` = signed delta |
| `ev_render_in/out` | audio render, 1500 Hz | `void f(void)` | **interrupt level** — keep it tiny |

`ev_key`/`ev_enc` event layout: `ev+12` = key id (or encoder number),
`ev+16` = flags (keys: `1` press, `8` repeat; releases arrive with `press` clear).

**Digitone runtime key codes** (measured, `digiemu-main\devices\digitone.toml`):

| key | code | key | code |
|---|---|---|---|
| FUNC | 1 | YES | 13 |
| MIDI | 2 | NO | 14 |
| UP | 15 | DOWN | 16 |
| LEFT | 17 | RIGHT | 18 |

Trigs are 26–41. `MIDI+YES` is the launch chord DIGIDOOM uses.

> **Do not patch** the sites core owns (Digitone 1.43: `0x40000538`,
> `0x4001900c`, `0x40019072`, `0x40019d9c`, `0x40019de4`, `0x40072a34`,
> `0x4009d108`, `0x4009e51c`) — subscribe instead.

---

## 4. Drawing — the firmware routines (OS 1.43)

Declare them as function pointers at fixed addresses. `y = 0` is the **bottom**
row for all of them.

```c
typedef unsigned char u8; typedef short s16; typedef unsigned short u16;
typedef int s32;

#define DN_VLINE    ((void (*)(void *, int, int, int, int))0x400DC92C)      /* (bmp,x,y0,y1,c) */
#define DN_FILLRECT ((void (*)(void *, int, int, int, int, int))0x400DD292) /* (bmp,x0,y0,x1,y1,c) */
#define DN_FRAMERECT ((void (*)(void *, int, int, int, int, int))0x400DD076)/* outline */
#define DN_TEXTF    ((void (*)(void *, const void *, int, int, int, const char *, ...))0x400DDE68)
#define DN_FONT5    ((const void *)0x402315C8)     /* the stock 5-px font */
#define DN_FW       ((const u8  *)0x402315DC)      /* per-char width */
#define DN_FO       ((const u16 *)0x402316DC)      /* per-char glyph offset */
#define DN_FB       ((const u8  *)0x402318DC)      /* glyph bitmap bytes */
```

`colour`: `0` clears, `1` sets, a negative inverts.

**Text**: `DN_TEXTF(bmp, DN_FONT5, x, ytop, -1, "SCORE %d", n)` prints a string
(with `%d`/`%s`). `ytop` is the glyph **top**; text runs downward (to smaller y).
For **centered** or **2× scaled** text, use the font tables directly:

```c
static int strw(const char *s, int sc) { int w=0; for(;*s;s++) w += (DN_FW[(u8)*s]+1)*sc; return w; }

static void glyph(void *bmp, int x, int ytop, int ch, int sc)
{
    int w = DN_FW[ch & 0xff], off = DN_FO[ch & 0xff], c, r;
    for (c = 0; c < w; c++) {
        int bits = DN_FB[off + c];
        for (r = 0; r < 8; r++)                 /* bit 0 = top row */
            if (bits & (1 << r)) {
                int px = x + c*sc, py = ytop - r*sc;
                DN_FILLRECT(bmp, px, py, px+sc-1, py-sc+1, 1);
            }
    }
}
static void text(void *bmp, int x, int ytop, const char *s, int sc)
{ for (; *s; s++) { glyph(bmp, x, ytop, (u8)*s, sc); x += (DN_FW[(u8)*s]+1)*sc; } }
static void ctext(void *bmp, int ytop, const char *s, int sc)
{ text(bmp, (128 - strw(s, sc)) / 2, ytop, s, sc); }
```

`strw(s,1)` gives exact width (spaces are narrower than letters), so
`ctext(bmp, y, "DOOM", 2)` is a big centered wordmark.

---

## 5. A minimal game skeleton

```c
static int active, g_state, g_ticks, g_dirty;

void my_draw(void *bmp, void *ctrl)
{
    (void)ctrl;
    if (!active) return;
    DN_FILLRECT(bmp, 0, 0, 127, 63, 0);            /* clear */
    DN_TEXTF(bmp, DN_FONT5, 2, 58, -1, "HELLO %d", g_ticks);
}

void my_tick(void *ctrl)
{
    if (!active) return;
    g_ticks++;
    /* … game logic … */
    if (g_dirty) { *((u8 *)ctrl + 0x20) = 1; g_dirty = 0; }  /* request redraw */
}

int my_key(void *brain, void *ev)
{
    int key = *(int *)((char *)ev + 12);
    int flags = *(int *)((char *)ev + 16);
    int first = (flags & 1) && !(flags & 8);
    (void)brain;
    if (key == 13 /*YES*/ && first) { active = !active; g_dirty = 1; }
    return active && key >= 15 && key <= 18;       /* swallow arrows while playing */
}
```

---

## 6. Coordinates, frames and sprites

- Screen: **128 wide × 64 tall**, `x` right, `y` **up** (`y=0` bottom).
- Panel pixel format (only needed if you write raw bytes): 1 bpp, `1024` bytes,
  `byte = page + 8*column`, `page = row>>3` from the top, `bit 0x80` = first row
  of the page. **You normally don't need this** — `FILLRECT`/`VLINE` are enough.
- Sprites: store each row as a `u16`, **bit 15 = leftmost column**; sample with
  `(row >> (15 - sc)) & 1`. A 12×16 frame is 16 `u16`s = 32 bytes.
- Billboard projection (from `digidoom.c`) uses a per-column z-buffer to hide
  sprites behind walls.

---

## 7. Optimizations (this is the important part)

**1. Gate the redraw — the biggest lever.**
Forcing a recompose (`ctrl+0x20`) also makes the firmware redraw **every stock
view** underneath you, so it is far more expensive than your own draw. Only ask
for a recompose when the screen actually changes:

- on a **fresh key press** (set a `g_dirty` flag),
- on a slow blink cadence for animated titles (`(g_ticks & 3) == 0`),
- at the **game frame rate** while playing — and you can trade smoothness for
  CPU: `(g_ticks & 1)` ≈ 15 Hz, `& 3` ≈ 10 Hz, `& 7` ≈ 7 Hz.
- **not at all** on static screens (menus, game-over, between snake moves).

Measured in DIGIDOOM: menu ≈ **4 draws / 128 ticks**; play ≈ **69 / 129** at
15 Hz. Keeping game logic on `ev_tick` (30 Hz) while redrawing slower keeps
input responsive.

**2. No libc, no float, no 64-bit.** `-ffreestanding` means division/modulo use
the ColdFire divider (fine), but avoid `double`/`float` and 64-bit math (they
pull in `libgcc`, which isn't linked). Use fixed point.

**3. Fixed point that doesn't overflow.** DIGIDOOM's Doom uses **Q15** for
positions/directions (1 cell = 32768) but keeps the DDA's ray deltas and side
distances in **Q8** so the products stay in 32 bits:

```c
int ddx = (1 << 23) / adx;              /* Q8 cells/step, adx = |ray dir| */
int sdx = (fx * ddx) >> 8;              /* fx in 0..255 (Q8 fraction)       */
int perp = sdx - ddx;                   /* Q8; guard perp >= 1 before divide */
int lh = 12288 / perp;                  /* wall height px                    */
```

Multiplying a Q15 fraction by a large Q8 distance is the classic overflow trap;
lowering the distance precision is the usual fix.

**4. Draw with spans, not pixels.** One `VLINE`/`FILLRECT` per run, never a call
per pixel. For **ordered-dither shading** (1-bit "grey"), draw each wall column
as a few short `VLINE` runs whose pattern depends on the shade level
(`vspan_dither` in `digidoom.c`). The run count stays small because far walls
are short.

**5. Keep the budget honest.** Everything lives in `.run` + `.bss` (128 KB for
*all* mods). Const tables (`const`) go in `.run` as rodata — fine, but watch the
total. `.fast` (fast SRAM) is only for the hottest code and needs the FAST AUDIO
table.

**6. Audio mods only.** If you touch `ev_render_in/out`, they run at **interrupt
level** 1500×/s: keep them short, no blocking calls, and save/restore the EMAC
state. See `..\digitone_corea\DSP.md`.

---

## 8. Testing in digiemu

```powershell
$py = "<digiemu-venv>\Scripts\python.exe"
$env:PYTHONPATH = "<digiemu-main>"
& $py -m emu.portable --add test.syx --yes      # boots, settles, saves gui.snap
```

Then a small script resumes the snapshot and drives it. Gotchas (all hit while
building DIGIDOOM):

- set `$env:DT2_SECTIONS` to the firmware's extracted `…\sections` dir;
- `longrun.build(...)` must match the snapshot's saved flags, including
  `unblock_except=(0x4137B93C,)` and `deferred_components=('timers',)`;
- get the device with `devices.load(…\digiemu-main\devices\digitone.toml)`
  (hash `identify` refuses a patched `.syx`);
- **input lag**: `panelin.feed` needs ~2 M instructions before the UI acts;
- read frames by latching at `profile.panel_diff` and
  `panel.read(m, profile.fb_front)` → `panel.write_png`;
- the emulator **breaks on hooking `panel_diff`/`panel_flush`** — draw through
  `ev_draw`.

---

## 9. Pitfalls checklist

- [ ] `y = 0` is the **bottom**; `DN_TEXTF`'s `y` is the glyph **top**.
- [ ] Swallow the keys you use (`ev_key` return nonzero) or the stock UI moves too.
- [ ] Don't patch core-owned sites — subscribe.
- [ ] No `float`, no 64-bit, no `printf`/`malloc`.
- [ ] Guard every divide (`perp >= 1`, `det != 0`, `cw >= 1`).
- [ ] Gate redraws; don't request a recompose every tick for a static screen.
- [ ] One `.elemod` per device/OS; addresses move between releases.
- [ ] Test in digiemu, and know the hardware recovery path.

---

## 10. Reference implementations

| file | shows |
|---|---|
| `../mods/digidoom/digidoom.c` | the whole thing: menu, 3 games, DDA, sprites, text, effects, gating |
| `..\elekloader\examples\hello-marker\hello.c` | the smallest `ev_draw` mod |
| `..\DigiScreen\mods\aba-dn\splash.s` | a full-frame overlay (present-hook technique) |
| `..\digitone_corea\mods\digictl\digictl.c` | pages, `DN_TEXTF`, centered/scaled text, `ev_key`/`ev_enc` |
