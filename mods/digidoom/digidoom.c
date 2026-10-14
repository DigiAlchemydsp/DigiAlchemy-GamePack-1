/* SPDX-License-Identifier: GPL-2.0-or-later
 * DIGIDOOM: a compact DOOM-style raycaster for the Digitone mk1 / Keys
 * (OS 1.43), drawn on the 128x64 panel.
 *
 * This is NOT the Doom engine. It is a from-scratch renderer in the spirit of
 * the RP2040 "Smallest-Doom" tiny builds: a grid raycaster with ordered-dither
 * shading, animated billboard sprites, a HUD and a title -> play -> game-over
 * flow, driven by the arrow keys, YES and NO.
 *
 * It runs as an elekloader format-2 mod. It subscribes only to core events
 * (ev_draw / ev_tick / ev_key), so it owns no firmware site and combines with
 * any other mod. It draws with the firmware's own FILLRECT / VLINE / TEXTF and
 * the stock 5-px font, with y = 0 the BOTTOM row, so it needs no knowledge of
 * the Bitmap's memory layout. All code and data live in the 128 KB .run/.bss
 * area; no C library, no float.
 */
typedef unsigned char  u8;
typedef signed   char  s8;
typedef unsigned short u16;
typedef short          s16;
typedef unsigned int   u32;
typedef int            s32;

#define FW 128
#define FH 64
#define MAPW 24
#define MAPH 24
#define CELL 32768                      /* 1 world cell in Q15 */
#define MAXENT 8
#define MAXFX  8

/* Digitone firmware routines and the stock 5-px font (OS 1.43). */
#define DN_VLINE    ((void (*)(void *, int, int, int, int))0x400DC92C)
#define DN_FILLRECT ((void (*)(void *, int, int, int, int, int))0x400DD292)
#define DN_TEXTF    ((void (*)(void *, const void *, int, int, int, const char *, ...))0x400DDE68)
#define DN_FONT5    ((const void *)0x402315C8)
#define DN_FW       ((const u8 *)0x402315DC)     /* per-char width */
#define DN_FO       ((const u16 *)0x402316DC)    /* per-char glyph offset */
#define DN_FB       ((const u8 *)0x402318DC)     /* glyph bitmap bytes */

/* top-level flow */
#define S_MENU       0
#define S_DOOM_TITLE 1
#define S_DOOM_PLAY  2
#define S_DOOM_DEAD  3
#define S_INV_PLAY   4
#define S_INV_OVER   5
#define S_SNAKE_PLAY 6
#define S_SNAKE_OVER 7

/* entity state */
#define ES_WALK   0
#define ES_ATTACK 1
#define ES_PAIN   2
#define ES_DIE    3

/* Digitone runtime key codes (devices/digitone.toml). */
#define KEY_UP     15
#define KEY_DOWN   16
#define KEY_LEFT   17
#define KEY_RIGHT  18
#define KEY_YES    13
#define KEY_NO     14
#define KEY_MIDI    2
#define KEY_FUNC    1

#define T_FLOOR 0
#define T_WALL  2
#define T_WALL2 3
#define T_WALL3 4
#define T_DOOR  5

#define E_NONE  0
#define E_IMP   1
#define E_DEMON 2
#define E_ITEM  3

/* Q15 sine, 256 steps (one turn). Reused from the Tone+FX 256-sine. */
static const s16 g_sin[256] = {
      0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
   6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
  12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
  18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
  23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
  27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
  30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
  32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
  32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
  32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
  30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
  27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
  23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
  18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
  12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
   6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
      0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
  -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
 -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
 -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
 -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
 -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
 -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
 -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
 -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
 -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
 -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
 -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
 -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
 -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
 -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
  -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

/* Sprites: 12x16 frames, one u16 per row, bit 15 = leftmost column. */
static const u16 g_imp_w0[16] = {
    0x0F00, 0x1F80, 0x3FC0, 0x7FE0, 0x6F60, 0x7FE0, 0x7FE0, 0x7FE0,
    0x7FE0, 0x3FC0, 0x3FC0, 0x1F80, 0x1980, 0x1980, 0x30C0, 0x6060,
};
static const u16 g_imp_w1[16] = {
    0x0F00, 0x1F80, 0x3FC0, 0x7FE0, 0x6F60, 0x7FE0, 0x7FE0, 0x7FE0,
    0x7FE0, 0x3FC0, 0x3FC0, 0x1F80, 0x1980, 0x30C0, 0x30C0, 0x6060,
};
static const u16 g_imp_at[16] = {
    0x0F00, 0x5FA0, 0x7FE0, 0x7FE0, 0x6F60, 0x7FE0, 0x7FE0, 0x7FE0,
    0x7FE0, 0x3FC0, 0x3FC0, 0x1F80, 0x1980, 0x1980, 0x30C0, 0x6060,
};
static const u16 g_imp_pain[16] = {
    0x0F00, 0x1F80, 0x3FC0, 0x7FE0, 0x6F60, 0x6F60, 0x7FE0, 0x7FE0,
    0x7FE0, 0x3FC0, 0x3FC0, 0x1F80, 0x1980, 0x1980, 0x30C0, 0x6060,
};
static const u16 g_imp_die[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0F00, 0x1F80,
    0x3FC0, 0x7FE0, 0xD6B0, 0x9F90, 0x56A0, 0x3FC0, 0x7FE0, 0xFFF0,
};
static const u16 g_dem_w0[16] = {
    0x8F10, 0xDFB0, 0xFFF0, 0xEF70, 0xFFF0, 0xFFF0, 0xFFF0, 0xFFF0,
    0xFFF0, 0x7FE0, 0x7FE0, 0x3FC0, 0x30C0, 0x30C0, 0x6060, 0x6060,
};
static const u16 g_dem_w1[16] = {
    0x8F10, 0xDFB0, 0xFFF0, 0xEF70, 0xFFF0, 0xFFF0, 0xFFF0, 0xFFF0,
    0xFFF0, 0x7FE0, 0x7FE0, 0x3FC0, 0x30C0, 0x6060, 0x6060, 0xC030,
};
static const u16 g_dem_at[16] = {
    0x8F10, 0xDFB0, 0xBFD0, 0xEF70, 0xFFF0, 0xFFF0, 0xFFF0, 0xFFF0,
    0xFFF0, 0x7FE0, 0x7FE0, 0x3FC0, 0x30C0, 0x30C0, 0x6060, 0x6060,
};
static const u16 g_dem_pain[16] = {
    0x8F10, 0xDFB0, 0xFFF0, 0xEF70, 0xEF70, 0xFFF0, 0xFFF0, 0xFFF0,
    0xFFF0, 0x7FE0, 0x7FE0, 0x3FC0, 0x30C0, 0x30C0, 0x6060, 0x6060,
};
static const u16 g_dem_die[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8F10, 0xDFB0,
    0xFFF0, 0xFFF0, 0xD6B0, 0x9F90, 0x56A0, 0x3FC0, 0x7FE0, 0xFFF0,
};
static const u16 g_item[8] = {
    0x1800, 0x1800, 0x7E00, 0x7E00, 0x7E00, 0x1800, 0x1800, 0x0000,
};

typedef struct { const u16 *f[5]; u8 w, h; s16 hp; } sprdef_t;
static const sprdef_t g_sprdef[4] = {
    { {0, 0, 0, 0, 0}, 0, 0, 0 },
    { {g_imp_w0, g_imp_w1, g_imp_at, g_imp_pain, g_imp_die}, 12, 16, 3 },
    { {g_dem_w0, g_dem_w1, g_dem_at, g_dem_pain, g_dem_die}, 12, 16, 6 },
    { {g_item, g_item, g_item, g_item, g_item}, 8, 8, 1 },
};

/* 0 = floor, nonzero = wall, 5 = door. A small original level. */
static u8 g_map[MAPW * MAPH] = {
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,2,2,2,2,0,0,0,0,0,0,0,0,3,3,3,3,0,0,0,0,1,
 1,0,0,2,0,0,2,0,0,0,0,0,0,0,0,3,0,0,3,0,0,0,0,1,
 1,0,0,2,0,0,2,0,0,0,0,0,0,0,0,3,0,0,3,0,0,0,0,1,
 1,0,0,2,0,0,2,0,0,0,0,0,0,0,0,3,0,0,3,0,0,0,0,1,
 1,0,0,2,2,5,2,0,0,0,0,0,0,0,0,3,3,0,3,0,0,0,0,1,
 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,4,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,4,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,4,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,4,4,4,5,4,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,2,2,2,2,5,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

typedef struct {
    s32 x, y;          /* Q15 */
    s16 type;
    s16 alive;
    s16 hp;
    s16 st;
    s16 anim;
    s16 timer;
} ent_t;

typedef struct { int x, y; s16 ttl; s16 mark; } fx_t;

static s32 p_x, p_y;
static int p_a;
static s32 dir_x, dir_y, plane_x, plane_y;

static u8  key_down[64];
static int active;
static int g_state;
static int g_ticks;
static int g_sel;                      /* game-select highlight */
static int g_dirty;                    /* a redraw is wanted now */
static int g_health, g_ammo, g_flash;
static int zbuf[FW];
static ent_t g_ent[MAXENT];
static int g_nent;
static fx_t g_fx[MAXFX];
static const char *g_marks[4] = { "x", "*", "+", "" };

/* Space Invaders */
#define INV_COLS 6
#define INV_ROWS 4
#define INV_W 6
#define INV_H 5
#define INV_SX 10
#define INV_SY 8
#define SHIP_W 11
#define SHIP_H 5
#define SHIP_Y 5
static u8  inv_alive[INV_COLS * INV_ROWS];
static int inv_ox, inv_oy, inv_dir, inv_ctr, inv_speed;
static int inv_shipx;
static int inv_bx, inv_by, inv_bon;
static int inv_ebx[3], inv_eby[3], inv_ebon[3];
static int inv_score, inv_lives, inv_won, inv_tick;

/* Snake */
#define SN_CELL 4
#define SN_GX 32
#define SN_GY 12
#define SN_MAX 256
static int sn_x[SN_MAX], sn_y[SN_MAX];   /* [0] is the head */
static int sn_len, sn_dx, sn_dy, sn_pdx, sn_pdy;
static int sn_fx, sn_fy, sn_ctr, sn_step, sn_score;
static u32 g_rng = 0x2545f491u;

/* ---- stock-font text helpers (width table + scaled glyphs) --------------- */

static int dgm_strw(const char *s, int sc)
{
    int w = 0;
    for (; *s; s++)
        w += (DN_FW[(u8)*s] + 1) * sc;
    return w;
}

static void dgm_glyph(void *bmp, int x, int ytop, int ch, int sc)
{
    int w = DN_FW[ch & 0xff];
    int off = DN_FO[ch & 0xff];
    int c, r;
    for (c = 0; c < w; c++) {
        int bits = DN_FB[off + c];
        for (r = 0; r < 8; r++) {
            if (bits & (1 << r)) {                  /* bit 0 = top row */
                int px = x + c * sc;
                int py = ytop - r * sc;
                DN_FILLRECT(bmp, px, py, px + sc - 1, py - sc + 1, 1);
            }
        }
    }
}

static void dgm_text(void *bmp, int x, int ytop, const char *s, int sc)
{
    for (; *s; s++) {
        dgm_glyph(bmp, x, ytop, (u8)*s, sc);
        x += (DN_FW[(u8)*s] + 1) * sc;
    }
}

static void dgm_ctext(void *bmp, int ytop, const char *s, int sc)
{
    dgm_text(bmp, (FW - dgm_strw(s, sc)) / 2, ytop, s, sc);
}

/* ---- world helpers ------------------------------------------------------ */

static void recompute_dir(void)
{
    dir_x = g_sin[(p_a + 64) & 255];
    dir_y = g_sin[p_a & 255];
    plane_x = (-dir_y * 21627) >> 15;
    plane_y = ( dir_x * 21627) >> 15;
}

static int cell_of(s32 v) { return (int)(v >> 15); }

static int walkable(s32 x, s32 y)
{
    int cx = cell_of(x), cy = cell_of(y);
    if (cx < 0 || cx >= MAPW || cy < 0 || cy >= MAPH)
        return 0;
    return g_map[cy * MAPW + cx] == T_FLOOR;
}

static void add_ent(int type, int cx, int cy)
{
    if (g_nent >= MAXENT)
        return;
    g_ent[g_nent].x = cx * CELL + CELL / 2;
    g_ent[g_nent].y = cy * CELL + CELL / 2;
    g_ent[g_nent].type = type;
    g_ent[g_nent].alive = 1;
    g_ent[g_nent].hp = g_sprdef[type].hp;
    g_ent[g_nent].st = ES_WALK;
    g_ent[g_nent].anim = 0;
    g_ent[g_nent].timer = 0;
    g_nent++;
}

static void reset_player(void)
{
    int i;
    p_x = 2 * CELL + CELL / 2;
    p_y = 2 * CELL + CELL / 2;
    p_a = 0;
    recompute_dir();
    g_health = 100;
    g_ammo = 50;
    g_flash = 0;
    g_nent = 0;
    add_ent(E_IMP, 6, 2);
    add_ent(E_IMP, 12, 12);
    add_ent(E_DEMON, 17, 8);
    add_ent(E_ITEM, 2, 6);
    for (i = 0; i < MAXFX; i++)
        g_fx[i].ttl = 0;
    for (i = 0; i < 64; i++)
        key_down[i] = 0;
}

/* ---- sprites ------------------------------------------------------------ */

static int project(s32 ex, s32 ey, int *pty, int *psx, int *ph)
{
    int rx = (int)((ex - p_x) >> 7);        /* Q8 */
    int ry = (int)((ey - p_y) >> 7);
    int dx = dir_x >> 7, dy = dir_y >> 7;
    int px = plane_x >> 7, py = plane_y >> 7;
    int det = ((px * dy) - (dx * py)) >> 8;
    int inv, dxy, dyy, tx, ty;

    if (det == 0)
        return 0;
    inv = (1 << 16) / det;                  /* Q8 */
    dxy = ((dy * rx) - (dx * ry)) >> 8;     /* Q8 */
    dyy = ((px * ry) - (py * rx)) >> 8;     /* Q8 */
    tx = (inv * dxy) >> 8;
    ty = (inv * dyy) >> 8;
    if (ty < 16)
        return 0;
    *pty = ty;
    *psx = 64 + (64 * tx) / ty;
    *ph = 10240 / ty;
    if (*ph < 2)
        *ph = 2;
    if (*ph > 220)
        *ph = 220;
    return 1;
}

/* Draw rows [r0, r1) of column cx, ordered-dithered when lvl < 4. */
static void vspan_dither(void *bmp, int cx, int r0, int r1, int lvl, int off)
{
    int row;
    if (r1 <= r0)
        return;
    if (lvl >= 4) {
        DN_VLINE(bmp, cx, FH - r1, FH - 1 - r0, 1);
        return;
    }
    row = r0;
    while (row < r1) {
        int a;
        if (((row + off) & 3) >= lvl) { row++; continue; }
        a = row;
        while (row < r1 && ((row + off) & 3) < lvl)
            row++;
        DN_VLINE(bmp, cx, FH - row, FH - 1 - a, 1);
    }
}

/* One sprite column: source column sc of the frame, rows sampled into
 * [ds, de) and drawn with the distance dither. */
static void draw_col(void *bmp, const u16 *fr, int w, int h,
                     int cx, int sc, int ds, int de, int lvl, int off)
{
    int sr = 0, span = de - ds;
    if (sc < 0) sc = 0;
    if (sc >= w) sc = w - 1;
    while (sr < h) {
        int sr0, r0, r1;
        if (!((fr[sr] >> (15 - sc)) & 1)) { sr++; continue; }
        sr0 = sr;
        while (sr < h && ((fr[sr] >> (15 - sc)) & 1))
            sr++;
        r0 = ds + (sr0 * span) / h;
        r1 = ds + (sr * span) / h;
        if (r1 <= r0) r1 = r0 + 1;
        vspan_dither(bmp, cx, r0, r1, lvl, off);
    }
}

static const u16 *frame_of(const ent_t *e)
{
    const sprdef_t *d = &g_sprdef[e->type];
    int fi;
    if (e->st == ES_ATTACK)      fi = 2;
    else if (e->st == ES_PAIN)   fi = 3;
    else if (e->st == ES_DIE)    fi = 4;
    else                         fi = (e->anim >> 2) & 1;
    return d->f[fi];
}

static void render_sprites(void *bmp)
{
    int i;
    for (i = 0; i < g_nent; i++) {
        const sprdef_t *d;
        const u16 *fr;
        int ty, sx, ph, pw, x0, x1, ds, de, cx, cw, lvl, off, shade;
        if (!g_ent[i].alive)
            continue;
        d = &g_sprdef[g_ent[i].type];
        fr = frame_of(&g_ent[i]);
        if (!fr || !project(g_ent[i].x, g_ent[i].y, &ty, &sx, &ph))
            continue;
        pw = (ph * d->w) / d->h;
        if (pw < 1) pw = 1;
        x0 = sx - (pw >> 1);
        x1 = sx + (pw >> 1);
        cw = x1 - x0;
        if (cw < 1) cw = 1;
        if (x1 < 0 || x0 >= FW)
            continue;
        ds = 32 - (ph >> 1);
        de = 32 + (ph >> 1);
        if (ds < 0) ds = 0;
        if (de > FH) de = FH;
        if (de <= ds)
            continue;
        shade = 16 - (ty >> 8);
        lvl = (shade + 3) >> 2;
        if (lvl < 1) lvl = 1;
        else if (lvl > 4) lvl = 4;
        for (cx = x0; cx <= x1; cx++) {
            int sc;
            if (cx < 0 || cx >= FW)
                continue;
            if (ty >= zbuf[cx])
                continue;                   /* wall is nearer */
            sc = ((cx - x0) * d->w) / cw;
            off = (cx >> 2) & 3;
            draw_col(bmp, fr, d->w, d->h, cx, sc, ds, de, lvl, off);
        }
    }
}

/* ---- walls -------------------------------------------------------------- */

static void render_walls(void *bmp)
{
    int x;
    for (x = 0; x < FW; x++) {
        int cam = ((2 * x - FW + 1) * 32768) / FW;
        int rdx = dir_x + ((plane_x * cam) >> 15);
        int rdy = dir_y + ((plane_y * cam) >> 15);
        int adx = rdx < 0 ? -rdx : rdx;
        int ady = rdy < 0 ? -rdy : rdy;
        int ddx, ddy, mx, my, fx, fy, stepx, stepy, sdx, sdy;
        int side = 0, hit = 0, n = 0, perp, lh, ds, de, shade, lvl, off;

        if (adx == 0) adx = 1;
        if (ady == 0) ady = 1;
        ddx = (1 << 23) / adx;
        ddy = (1 << 23) / ady;

        mx = cell_of(p_x);
        my = cell_of(p_y);
        fx = (int)((p_x & (CELL - 1)) >> 7);
        fy = (int)((p_y & (CELL - 1)) >> 7);

        if (rdx < 0) { stepx = -1; sdx = (fx * ddx) >> 8; }
        else         { stepx =  1; sdx = ((256 - fx) * ddx) >> 8; }
        if (rdy < 0) { stepy = -1; sdy = (fy * ddy) >> 8; }
        else         { stepy =  1; sdy = ((256 - fy) * ddy) >> 8; }

        while (n++ < 128) {
            if (sdx < sdy) { sdx += ddx; mx += stepx; side = 0; }
            else           { sdy += ddy; my += stepy; side = 1; }
            if (mx < 0 || mx >= MAPW || my < 0 || my >= MAPH)
                break;
            if (g_map[my * MAPW + mx]) { hit = 1; break; }
        }

        perp = (side == 0) ? (sdx - ddx) : (sdy - ddy);
        if (perp < 1)
            perp = 1;
        zbuf[x] = perp;

        if (!hit)
            continue;
        lh = 12288 / perp;
        if (lh > 8192)
            lh = 8192;
        ds = 32 - (lh >> 1);
        de = 32 + (lh >> 1);
        if (ds < 0) ds = 0;
        if (de > FH) de = FH;
        if (de <= ds)
            continue;
        shade = 16 - (perp >> 8);
        if (side)
            shade -= 3;
        lvl = (shade + 3) >> 2;
        if (lvl < 1) lvl = 1;
        else if (lvl > 4) lvl = 4;
        if (lvl < 4 && (x & 3) >= lvl)
            continue;
        off = (x >> 2) & 3;
        vspan_dither(bmp, x, ds, de, lvl, off);
    }
}

/* ---- effects ------------------------------------------------------------ */

static void spawn_fx(int x, int y, int mark)
{
    int i;
    for (i = 0; i < MAXFX; i++) {
        if (g_fx[i].ttl <= 0) {
            g_fx[i].x = x;
            g_fx[i].y = y;
            g_fx[i].ttl = 15;
            g_fx[i].mark = mark;
            return;
        }
    }
}

static void render_fx(void *bmp)
{
    int i;
    for (i = 0; i < MAXFX; i++) {
        if (g_fx[i].ttl > 0) {
            DN_FILLRECT(bmp, g_fx[i].x - 1, g_fx[i].y - 6,
                        g_fx[i].x + 5, g_fx[i].y + 1, 0);
            DN_TEXTF(bmp, DN_FONT5, g_fx[i].x, g_fx[i].y, -1, "%s",
                     g_marks[g_fx[i].mark & 3]);
        }
    }
}

/* ---- Space Invaders ----------------------------------------------------- */

static void reset_invaders(void)
{
    int i;
    for (i = 0; i < INV_COLS * INV_ROWS; i++)
        inv_alive[i] = 1;
    inv_ox = 10;
    inv_oy = 46;
    inv_dir = 1;
    inv_ctr = 0;
    inv_speed = 8;
    inv_shipx = (FW - SHIP_W) / 2;
    inv_bon = 0;
    for (i = 0; i < 3; i++)
        inv_ebon[i] = 0;
    inv_score = 0;
    inv_lives = 3;
    inv_won = 0;
    inv_tick = 0;
    for (i = 0; i < 64; i++)
        key_down[i] = 0;
}

static int inv_alive_count(void)
{
    int i, n = 0;
    for (i = 0; i < INV_COLS * INV_ROWS; i++)
        if (inv_alive[i]) n++;
    return n;
}

static void inv_fire(void)
{
    if (!inv_bon) {
        inv_bon = 1;
        inv_bx = inv_shipx + SHIP_W / 2;
        inv_by = SHIP_Y + SHIP_H;
    }
}

static void inv_over(int won)
{
    inv_won = won;
    g_state = S_INV_OVER;
}

static void update_invaders(void)
{
    int i, c, r, miny = 999, alive;
    if (g_state != S_INV_PLAY)
        return;
    inv_tick++;

    if (key_down[KEY_LEFT])  inv_shipx -= 3;
    if (key_down[KEY_RIGHT]) inv_shipx += 3;
    if (inv_shipx < 1) inv_shipx = 1;
    if (inv_shipx > FW - 1 - SHIP_W) inv_shipx = FW - 1 - SHIP_W;

    if (inv_bon) {
        inv_by += 4;
        if (inv_by > FH - 1) {
            inv_bon = 0;
        } else {
            for (i = 0; i < INV_COLS * INV_ROWS; i++) {
                int ax, ay;
                if (!inv_alive[i]) continue;
                c = i % INV_COLS; r = i / INV_COLS;
                ax = inv_ox + c * INV_SX;
                ay = inv_oy - r * INV_SY;
                if (inv_bx >= ax && inv_bx < ax + INV_W &&
                    inv_by >= ay && inv_by < ay + INV_H) {
                    inv_alive[i] = 0;
                    inv_score += 10;
                    inv_bon = 0;
                    break;
                }
            }
        }
    }

    /* the fleet steps; faster as it thins */
    inv_ctr++;
    if (inv_ctr >= inv_speed) {
        int nx;
        inv_ctr = 0;
        alive = inv_alive_count();
        if (alive == 0) { inv_over(1); return; }
        nx = inv_ox + inv_dir * 2;
        if (nx < 2 || nx + (INV_COLS - 1) * INV_SX + INV_W > FW - 2) {
            inv_dir = -inv_dir;
            inv_oy -= 2;
        } else {
            inv_ox = nx;
        }
        inv_speed = 2 + alive / 2;
    }

    /* an alien fires now and then */
    alive = inv_alive_count();
    if (alive > 0 && (inv_tick % 45) == 0) {
        int pick = -1, seen = (inv_tick / 45) % alive;
        for (i = 0; i < INV_COLS * INV_ROWS; i++) {
            if (!inv_alive[i]) continue;
            if (seen-- == 0) { pick = i; break; }
        }
        if (pick >= 0) {
            for (i = 0; i < 3; i++) {
                if (!inv_ebon[i]) {
                    c = pick % INV_COLS; r = pick / INV_COLS;
                    inv_ebx[i] = inv_ox + c * INV_SX + INV_W / 2;
                    inv_eby[i] = inv_oy - r * INV_SY;
                    inv_ebon[i] = 1;
                    break;
                }
            }
        }
    }
    for (i = 0; i < 3; i++) {
        if (!inv_ebon[i]) continue;
        inv_eby[i] -= 3;
        if (inv_eby[i] < SHIP_Y + SHIP_H &&
            inv_ebx[i] >= inv_shipx && inv_ebx[i] < inv_shipx + SHIP_W) {
            inv_ebon[i] = 0;
            if (--inv_lives <= 0) { inv_over(0); return; }
        } else if (inv_eby[i] < 0) {
            inv_ebon[i] = 0;
        }
    }

    /* did the fleet reach the ship? */
    for (i = 0; i < INV_COLS * INV_ROWS; i++) {
        int ay;
        if (!inv_alive[i]) continue;
        r = i / INV_COLS;
        ay = inv_oy - r * INV_SY;
        if (ay < miny) miny = ay;
    }
    if (miny != 999 && miny <= SHIP_Y + SHIP_H + 1)
        inv_over(0);
}

/* ---- Snake -------------------------------------------------------------- */

static int rnd(int m)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return (int)((g_rng >> 16) % (unsigned)m);
}

static int sn_occ(int x, int y)
{
    int i;
    for (i = 0; i < sn_len; i++)
        if (sn_x[i] == x && sn_y[i] == y)
            return 1;
    return 0;
}

static void sn_place_food(void)
{
    int i;
    for (i = 0; i < 500; i++) {
        int x = rnd(SN_GX), y = rnd(SN_GY);
        if (!sn_occ(x, y)) { sn_fx = x; sn_fy = y; return; }
    }
    sn_fx = SN_GX / 2;
    sn_fy = SN_GY / 2;
}

static void reset_snake(void)
{
    int i;
    sn_len = 3;
    sn_x[0] = 8; sn_y[0] = 6;
    sn_x[1] = 7; sn_y[1] = 6;
    sn_x[2] = 6; sn_y[2] = 6;
    sn_dx = 1; sn_dy = 0; sn_pdx = 1; sn_pdy = 0;
    sn_score = 0;
    sn_step = 8;                       /* ticks between moves (starts slow) */
    sn_ctr = 0;
    sn_place_food();
    for (i = 0; i < 64; i++)
        key_down[i] = 0;
}

static void sn_over(void) { g_state = S_SNAKE_OVER; }

static void update_snake(void)
{
    int i, nx, ny, otx, oty, eat;
    if (g_state != S_SNAKE_PLAY)
        return;

    if (key_down[KEY_UP] && sn_dy == 0)    { sn_pdx = 0;  sn_pdy = 1; }
    if (key_down[KEY_DOWN] && sn_dy == 0)  { sn_pdx = 0;  sn_pdy = -1; }
    if (key_down[KEY_LEFT] && sn_dx == 0)  { sn_pdx = -1; sn_pdy = 0; }
    if (key_down[KEY_RIGHT] && sn_dx == 0) { sn_pdx = 1;  sn_pdy = 0; }

    if (++sn_ctr < sn_step)
        return;
    sn_ctr = 0;
    sn_dx = sn_pdx;
    sn_dy = sn_pdy;

    nx = sn_x[0] + sn_dx;
    ny = sn_y[0] + sn_dy;
    if (nx < 0 || nx >= SN_GX || ny < 0 || ny >= SN_GY) { sn_over(); return; }
    eat = (nx == sn_fx && ny == sn_fy);
    for (i = 0; i < sn_len - (eat ? 0 : 1); i++)     /* the tail moves away */
        if (sn_x[i] == nx && sn_y[i] == ny) { sn_over(); return; }

    otx = sn_x[sn_len - 1];
    oty = sn_y[sn_len - 1];
    for (i = sn_len - 1; i > 0; i--) {
        sn_x[i] = sn_x[i - 1];
        sn_y[i] = sn_y[i - 1];
    }
    sn_x[0] = nx;
    sn_y[0] = ny;
    if (eat) {
        if (sn_len < SN_MAX) {                       /* grow by one */
            sn_x[sn_len] = otx;
            sn_y[sn_len] = oty;
            sn_len++;
        }
        sn_score++;
        sn_step = 8 - sn_score / 3;                  /* faster per dot */
        if (sn_step < 3) sn_step = 3;
        sn_place_food();
    }
    g_dirty = 1;                                     /* redraw when it moves */
}

/* ---- game --------------------------------------------------------------- */

static void update_game(void)
{
    int turn = 0, i;
    s32 fwd = 0, side = 0, nx, ny;

    if (g_state != S_DOOM_PLAY)
        return;

    if (key_down[KEY_FUNC]) {
        if (key_down[KEY_LEFT])  side -= 2600;
        if (key_down[KEY_RIGHT]) side += 2600;
    } else {
        if (key_down[KEY_LEFT])  turn -= 3;
        if (key_down[KEY_RIGHT]) turn += 3;
    }
    if (key_down[KEY_UP])   fwd += 3000;
    if (key_down[KEY_DOWN]) fwd -= 3000;

    if (turn) {
        p_a = (p_a + turn) & 255;
        recompute_dir();
    }
    if (fwd || side) {
        nx = p_x + ((dir_x * fwd) >> 15) - ((dir_y * side) >> 15);
        ny = p_y + ((dir_y * fwd) >> 15) + ((dir_x * side) >> 15);
        if (walkable(nx, p_y)) p_x = nx;
        if (walkable(p_x, ny)) p_y = ny;
    }
    if (g_flash > 0)
        g_flash--;

    for (i = 0; i < MAXFX; i++)
        if (g_fx[i].ttl > 0)
            g_fx[i].ttl--;

    for (i = 0; i < g_nent; i++) {
        ent_t *e = &g_ent[i];
        s32 dx, dy;
        if (!e->alive || e->type == E_ITEM)
            continue;
        if (e->timer > 0) {
            e->timer--;
            if (e->timer == 0) {
                if (e->st == ES_DIE) { e->alive = 0; continue; }
                e->st = ES_WALK;
            }
        }
        if (e->st == ES_PAIN || e->st == ES_DIE || e->st == ES_ATTACK) {
            e->anim++;
            continue;
        }
        e->anim++;
        dx = p_x - e->x;
        dy = p_y - e->y;
        if (dx > -14 * CELL && dx < 14 * CELL && dy > -14 * CELL && dy < 14 * CELL) {
            if (dx > -CELL - 4000 && dx < CELL + 4000 &&
                dy > -CELL - 4000 && dy < CELL + 4000) {
                e->st = ES_ATTACK;
                e->timer = 12;
                g_health -= (e->type == E_DEMON) ? 12 : 6;
                if (g_health <= 0) { g_health = 0; g_state = S_DOOM_DEAD; return; }
            } else {
                s32 mx = dx > 0 ? 240 : (dx < 0 ? -240 : 0);
                s32 my = dy > 0 ? 240 : (dy < 0 ? -240 : 0);
                if (walkable(e->x + mx, e->y)) e->x += mx;
                if (walkable(e->x, e->y + my)) e->y += my;
            }
        }
    }

    for (i = 0; i < g_nent; i++) {
        s32 dx, dy;
        if (!g_ent[i].alive || g_ent[i].type != E_ITEM)
            continue;
        dx = p_x - g_ent[i].x;
        dy = p_y - g_ent[i].y;
        if (dx > -CELL && dx < CELL && dy > -CELL && dy < CELL) {
            g_ent[i].alive = 0;
            g_health += 25;
            if (g_health > 100) g_health = 100;
        }
    }
}

static void fire(void)
{
    int i, best = -1, bestty = 1 << 30, bestsx = 64;
    if (g_ammo <= 0)
        return;
    g_ammo--;
    g_flash = 5;
    for (i = 0; i < g_nent; i++) {
        int ty, sx, ph;
        if (!g_ent[i].alive || g_ent[i].type == E_ITEM)
            continue;
        if (g_ent[i].st == ES_DIE)
            continue;
        if (!project(g_ent[i].x, g_ent[i].y, &ty, &sx, &ph))
            continue;
        if (ty > 14 * 256)
            continue;
        if (sx < 56 || sx > 72)
            continue;
        if (ty < bestty) { bestty = ty; best = i; bestsx = sx; }
    }
    if (best >= 0) {
        ent_t *e = &g_ent[best];
        e->hp--;
        e->st = ES_PAIN;
        e->timer = 6;
        spawn_fx(bestsx - 2, 32, 0);            /* hit: "x" */
        if (e->hp <= 0) {
            e->st = ES_DIE;
            e->timer = 20;
        }
    } else {
        spawn_fx(62, 32, 1);                    /* miss: wall puff "*" */
    }
}

static void use_door(void)
{
    s32 tx = p_x + ((dir_x * 22000) >> 15);
    s32 ty = p_y + ((dir_y * 22000) >> 15);
    int cx = cell_of(tx), cy = cell_of(ty);
    if (cx >= 0 && cx < MAPW && cy >= 0 && cy < MAPH &&
        g_map[cy * MAPW + cx] == T_DOOR)
        g_map[cy * MAPW + cx] = T_FLOOR;
}

/* ---- HUD ---------------------------------------------------------------- */

static void render_hud(void *bmp)
{
    int hpw, aw;
    DN_FILLRECT(bmp, 1, 57, 42, 62, 1);
    DN_FILLRECT(bmp, 2, 58, 41, 61, 0);
    hpw = (g_health * 38) / 100;
    if (hpw > 0) DN_FILLRECT(bmp, 2, 58, 1 + hpw, 61, 1);
    DN_FILLRECT(bmp, 85, 57, 126, 62, 1);
    DN_FILLRECT(bmp, 86, 58, 125, 61, 0);
    aw = (g_ammo * 38) / 50;
    if (aw > 38) aw = 38;
    if (aw > 0) DN_FILLRECT(bmp, 86, 58, 85 + aw, 61, 1);
    DN_FILLRECT(bmp, 58, 0, 69, 6, 1);
    DN_FILLRECT(bmp, 61, 6, 66, 12, 1);
    if (g_flash > 0)
        DN_FILLRECT(bmp, 55, 9, 72, 18, 1);
    DN_FILLRECT(bmp, 63, 31, 63, 31, 1);
    DN_FILLRECT(bmp, 64, 30, 64, 30, 1);
    DN_FILLRECT(bmp, 64, 31, 64, 31, 1);
    DN_FILLRECT(bmp, 64, 32, 64, 32, 1);
    DN_FILLRECT(bmp, 65, 31, 65, 31, 1);
}

static void render_title(void *bmp)
{
    int mx, blink = (g_ticks >> 3) & 1;
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);
    DN_FILLRECT(bmp, 0, 0, FW - 1, 0, 1);
    DN_FILLRECT(bmp, 0, FH - 1, FW - 1, FH - 1, 1);
    DN_FILLRECT(bmp, 0, 0, 0, FH - 1, 1);
    DN_FILLRECT(bmp, FW - 1, 0, FW - 1, FH - 1, 1);
    /* demon face */
    DN_FILLRECT(bmp, 50, 26, 77, 45, 1);
    DN_FILLRECT(bmp, 48, 44, 52, 48, 1);
    DN_FILLRECT(bmp, 75, 44, 79, 48, 1);
    DN_FILLRECT(bmp, 56, 35, 62, 41, 0);
    DN_FILLRECT(bmp, 65, 35, 71, 41, 0);
    for (mx = 54; mx < 74; mx += 4)
        DN_FILLRECT(bmp, mx, 29, mx + 1, 34, 0);
    /* centred wordmark */
    dgm_ctext(bmp, 62, "DOOM", 2);
    dgm_ctext(bmp, 22, "KARL MARX EDITION", 1);
    if (blink)
        dgm_ctext(bmp, 12, "PRESS YES", 1);
}

static void render_dead(void *bmp)
{
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);
    dgm_ctext(bmp, 52, "GAME OVER", 2);
    dgm_ctext(bmp, 30, "YOU DIED", 1);
    dgm_ctext(bmp, 20, "YES = RETRY", 1);
    dgm_ctext(bmp, 10, "NO = EXIT", 1);
}

static void render_menu(void *bmp)
{
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);
    DN_FILLRECT(bmp, 0, 0, FW - 1, 0, 1);
    DN_FILLRECT(bmp, 0, FH - 1, FW - 1, FH - 1, 1);
    DN_FILLRECT(bmp, 0, 0, 0, FH - 1, 1);
    DN_FILLRECT(bmp, FW - 1, 0, FW - 1, FH - 1, 1);
    dgm_ctext(bmp, 56, "GAME SELECT", 1);
    dgm_text(bmp, 42, 42, "DOOM", 1);
    dgm_text(bmp, 42, 32, "INVADERS", 1);
    dgm_text(bmp, 42, 22, "SNAKE", 1);
    dgm_text(bmp, 30, g_sel == 0 ? 42 : g_sel == 1 ? 32 : 22, ">", 1);
    dgm_ctext(bmp, 10, "YES=START  NO=EXIT", 1);
}

static void render_invaders(void *bmp)
{
    int i, c, r;
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);
    for (i = 0; i < 20; i++) {                      /* stars */
        int sx = (i * 53 + 7) & 127;
        int sy = (i * 29 + 11) & 63;
        DN_FILLRECT(bmp, sx, sy, sx, sy, 1);
    }
    for (i = 0; i < INV_COLS * INV_ROWS; i++) {
        int ax, ay;
        if (!inv_alive[i]) continue;
        c = i % INV_COLS; r = i / INV_COLS;
        ax = inv_ox + c * INV_SX;
        ay = inv_oy - r * INV_SY;
        DN_FILLRECT(bmp, ax, ay + 1, ax + INV_W - 1, ay + 3, 1);
        DN_FILLRECT(bmp, ax + 1, ay + 4, ax + INV_W - 2, ay + 4, 1);
        DN_FILLRECT(bmp, ax + 1, ay, ax + INV_W - 2, ay, 1);
    }
    DN_FILLRECT(bmp, inv_shipx, SHIP_Y, inv_shipx + SHIP_W - 1, SHIP_Y + 2, 1);
    DN_FILLRECT(bmp, inv_shipx + 3, SHIP_Y + 3, inv_shipx + SHIP_W - 4, SHIP_Y + 4, 1);
    if (inv_bon)
        DN_FILLRECT(bmp, inv_bx, inv_by, inv_bx, inv_by + 1, 1);
    for (i = 0; i < 3; i++)
        if (inv_ebon[i])
            DN_FILLRECT(bmp, inv_ebx[i], inv_eby[i], inv_ebx[i], inv_eby[i] + 1, 1);
    DN_TEXTF(bmp, DN_FONT5, 2, 60, -1, "SCORE %d", inv_score);
    for (i = 0; i < inv_lives; i++)
        DN_FILLRECT(bmp, 100 + i * 8, 60, 107 + i * 8, 61, 1);
}

static void render_inv_over(void *bmp)
{
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);
    dgm_ctext(bmp, 50, inv_won ? "YOU WIN" : "GAME OVER", 2);
    DN_TEXTF(bmp, DN_FONT5, 40, 26, -1, "SCORE %d", inv_score);
    dgm_ctext(bmp, 16, "YES = RETRY", 1);
    dgm_ctext(bmp, 6, "NO = MENU", 1);
}

static void render_snake(void *bmp)
{
    int i;
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);
    for (i = 0; i < sn_len; i++)
        DN_FILLRECT(bmp, sn_x[i] * SN_CELL, sn_y[i] * SN_CELL,
                    sn_x[i] * SN_CELL + SN_CELL - 1,
                    sn_y[i] * SN_CELL + SN_CELL - 1, 1);
    DN_FILLRECT(bmp, sn_fx * SN_CELL + 1, sn_fy * SN_CELL + 1,
                sn_fx * SN_CELL + SN_CELL - 2,
                sn_fy * SN_CELL + SN_CELL - 2, 1);   /* food: a smaller dot */
    DN_TEXTF(bmp, DN_FONT5, 2, 58, -1, "SCORE %d", sn_score);
}

static void render_snake_over(void *bmp)
{
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);
    dgm_ctext(bmp, 46, "GAME OVER", 2);
    DN_TEXTF(bmp, DN_FONT5, 40, 24, -1, "SCORE %d", sn_score);
    dgm_ctext(bmp, 14, "YES = RETRY", 1);
    dgm_ctext(bmp, 4, "NO = MENU", 1);
}

static void render(void *bmp)
{
    if (g_state == S_MENU)       { render_menu(bmp); return; }
    if (g_state == S_DOOM_TITLE) { render_title(bmp); return; }
    if (g_state == S_DOOM_DEAD)  { render_dead(bmp); return; }
    if (g_state == S_INV_PLAY)   { render_invaders(bmp); return; }
    if (g_state == S_INV_OVER)   { render_inv_over(bmp); return; }
    if (g_state == S_SNAKE_PLAY) { render_snake(bmp); return; }
    if (g_state == S_SNAKE_OVER) { render_snake_over(bmp); return; }
    DN_FILLRECT(bmp, 0, 0, FW - 1, FH - 1, 0);        /* black floor/ceiling */
    render_walls(bmp);
    render_sprites(bmp);
    render_fx(bmp);
    render_hud(bmp);
}

/* ---- hooks -------------------------------------------------------------- */

void digidoom_draw(void *bmp, void *ctrl)
{
    (void)ctrl;
    if (!active)
        return;
    render(bmp);
}

void digidoom_tick(void *ctrl)
{
    int want;
    if (!active)
        return;
    g_ticks++;
    update_game();
    update_invaders();
    update_snake();

    /* Redraw gating. Forcing a recompose also makes the firmware redraw every
     * stock view underneath us, so it is the dominant cost. Only ask when the
     * screen actually changes: on input, on a blink cadence, or at the game
     * frame rate (play states at ~15 Hz; static screens not at all). Snake
     * redraws only when it moves (it sets g_dirty). */
    switch (g_state) {
    case S_MENU:
    case S_DOOM_DEAD:
    case S_INV_OVER:
    case S_SNAKE_PLAY:
    case S_SNAKE_OVER:
        want = g_dirty;
        break;
    case S_DOOM_TITLE:
        want = g_dirty || ((g_ticks & 3) == 0);      /* ~7 Hz blink */
        break;
    default:                                         /* S_DOOM_PLAY / S_INV_PLAY */
        want = g_dirty || ((g_ticks & 1) == 0);      /* ~15 Hz */
        break;
    }
    if (want) {
        *((u8 *)ctrl + 0x20) = 1;
        g_dirty = 0;
    }
}

int digidoom_key(void *brain, void *ev)
{
    int key = *(int *)((char *)ev + 12);
    int flags = *(int *)((char *)ev + 16);
    int press = flags & 1;
    int repeat = flags & 8;
    int first = press && !repeat;
    (void)brain;

    if (key >= 0 && key < 64) {
        if (press) {
            if (!repeat)
                key_down[key] = 1;
        } else {
            key_down[key] = 0;
        }
    }
    if (first)
        g_dirty = 1;                    /* a fresh press redraws this tick */

    /* MIDI + YES toggles the launcher on (menu) and off. */
    if (key == KEY_YES && first && key_down[KEY_MIDI]) {
        active = !active;
        if (active) {
            g_state = S_MENU;
            g_sel = 0;
            g_ticks = 0;
        }
        return 1;
    }

    if (!active)
        return 0;

    switch (g_state) {
    case S_MENU:
        if (key == KEY_UP && first && g_sel > 0) g_sel--;
        if (key == KEY_DOWN && first && g_sel < 2) g_sel++;
        if (key == KEY_YES && first) {
            if (g_sel == 0)      { reset_player();   g_state = S_DOOM_TITLE; }
            else if (g_sel == 1) { reset_invaders(); g_state = S_INV_PLAY; }
            else                 { reset_snake();    g_state = S_SNAKE_PLAY; }
        } else if (key == KEY_NO && first) {
            active = 0;
        }
        break;
    case S_DOOM_TITLE:
        if (key == KEY_YES && first) g_state = S_DOOM_PLAY;
        else if (key == KEY_NO && first) g_state = S_MENU;
        break;
    case S_DOOM_PLAY:
        if (key == KEY_YES && first) fire();
        else if (key == KEY_NO && first) use_door();
        break;
    case S_DOOM_DEAD:
        if (key == KEY_YES && first) { reset_player(); g_state = S_DOOM_PLAY; }
        else if (key == KEY_NO && first) g_state = S_MENU;
        break;
    case S_INV_PLAY:
        if (key == KEY_YES && first) inv_fire();
        else if (key == KEY_NO && first) g_state = S_MENU;
        break;
    case S_INV_OVER:
        if (key == KEY_YES && first) { reset_invaders(); g_state = S_INV_PLAY; }
        else if (key == KEY_NO && first) g_state = S_MENU;
        break;
    case S_SNAKE_PLAY:
        if (key == KEY_NO && first) g_state = S_MENU;
        break;
    case S_SNAKE_OVER:
        if (key == KEY_YES && first) { reset_snake(); g_state = S_SNAKE_PLAY; }
        else if (key == KEY_NO && first) g_state = S_MENU;
        break;
    }

    if (key >= KEY_UP && key <= KEY_RIGHT)
        return 1;
    if (key == KEY_YES || key == KEY_NO || key == KEY_FUNC || key == KEY_MIDI)
        return 1;
    return 0;
}
