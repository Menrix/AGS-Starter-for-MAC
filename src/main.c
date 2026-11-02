/*
 * AGS Starter – ein kleiner Retro-Starter fuer AGS-Spiele (Adventure Game Studio) auf dem Mac.
 *
 * Das ganze Menue wird in ein Pixelbild mit 384x240 Punkten gezeichnet und von SDL
 * ganzzahlig hochskaliert (wie bei ScummVM). Wird das Fenster groesser gezogen, wird
 * einfach gezoomt – nichts verrutscht.
 *
 * Bedienung (wie bei klassischen Spiele-Startern):
 *   - Spiel in der Liste anklicken = auswaehlen
 *   - START (oder Doppelklick / Enter) = spielen
 *   - Knoepfe loesen erst beim LOSLASSEN aus; mit gedrueckter Maus wegziehen = abbrechen
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "font8x8_basic.h"
#include "font8x8_ext_latin.h"
#include "font_small.h"
#include "scene.h"
#include "header_anim.h"
#include <math.h>

/* ------------------------------------------------------------------ Grundlagen */

#define APP_VERSION "4.0"
#define LW 384               /* logische Breite  */
#define LH 240               /* logische Hoehe   */
#define ZOOM 3               /* Start-Zoom       */

static Uint32 fb[LW * LH];   /* unser Pixelbild  */
#define KEY 0x010203         /* "durchsichtig": hier scheint das Header-Bild durch */
static int have_header = 0;

/* Farben (0xRRGGBB) – abgestimmt auf das Header-Bild:
   Nachthimmel-Blau, Sonnenuntergang-Orange, Beige wie die alten Computergehaeuse */
#define COL_BG        0x14203f   /* Nachthimmel dunkel   */
#define COL_PANEL     0x1d2c58   /* Dialog-Flaeche       */
#define COL_LIST      0x0c1020   /* Listen-Hintergrund   */
#define COL_LIGHT     0xac9178   /* Rahmen (Beige-Braun) */
#define COL_DARK      0x070a14
#define COL_TEXT      0xefe6cf   /* Creme                */
#define COL_DIM       0x9aa6cc   /* Himmelblau gedaempft */
#define COL_AMBER     0xf2b27a   /* Sonnenuntergang hell */
#define COL_SKY       0xa9c0f0   /* Satzzeile            */
#define COL_ROWHOVER  0x2a3a6c
#define COL_ROWSEL    0xe0915e   /* Auswahl: Sonnen-Orange */
#define COL_ROWSELTX  0x141c38
#define COL_BTN       0xd9ceb4   /* Knopf: Computer-Beige */
#define COL_BTNHOV    0xece4cf
#define COL_BTNDOWN   0xb3a588
#define COL_BTNLIGHT  0xfaf5e8
#define COL_BTNDARK   0x6e5f48
#define COL_BTNTEXT   0x2a2236
#define COL_GO        0x4f9a3c   /* START: Dschungel-Gruen */
#define COL_GOHOV     0x66b450
#define COL_GODOWN    0x3a7a2c
#define COL_GOLIGHT   0xb0e090
#define COL_GODARK    0x173a12
#define COL_STATUS    0x0c1020
#define COL_TRACK     0x1a2648

static void px(int x, int y, Uint32 c) {
    if (x >= 0 && y >= 0 && x < LW && y < LH) fb[y * LW + x] = c;
}
static void fill(int x, int y, int w, int h, Uint32 c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) px(i, j, c);
}
static void hline(int x, int y, int w, Uint32 c) { fill(x, y, w, 1, c); }
static void vline(int x, int y, int h, Uint32 c) { fill(x, y, 1, h, c); }
static void darken_all(void) {   /* fuer Dialoge: Hintergrund abdunkeln */
    for (int i = 0; i < LW * LH; i++) {
        Uint32 c = fb[i];
        if (c == KEY) continue;   /* Header wird beim Zusammensetzen abgedunkelt */
        fb[i] = ((((c >> 16) & 255) / 3) << 16) | ((((c >> 8) & 255) / 3) << 8) | ((c & 255) / 3);
    }
}

/* ------------------------------------------------------------------ Schrift */

/* naechstes Zeichen aus UTF-8 lesen; macOS-Dateinamen sind "zerlegt" (o + ¨), das setzen wir wieder zusammen */
static unsigned compose(unsigned base, unsigned mark) {
    static const struct { unsigned b, m, r; } T[] = {
        {'a',0x308,0xe4},{'o',0x308,0xf6},{'u',0x308,0xfc},{'A',0x308,0xc4},{'O',0x308,0xd6},{'U',0x308,0xdc},
        {'e',0x308,0xeb},{'i',0x308,0xef},{'y',0x308,0xff},{'E',0x308,0xcb},{'I',0x308,0xcf},
        {'a',0x301,0xe1},{'e',0x301,0xe9},{'i',0x301,0xed},{'o',0x301,0xf3},{'u',0x301,0xfa},
        {'A',0x301,0xc1},{'E',0x301,0xc9},{'I',0x301,0xcd},{'O',0x301,0xd3},{'U',0x301,0xda},
        {'a',0x300,0xe0},{'e',0x300,0xe8},{'i',0x300,0xec},{'o',0x300,0xf2},{'u',0x300,0xf9},
        {'n',0x303,0xf1},{'N',0x303,0xd1},{'a',0x303,0xe3},{'o',0x303,0xf5},
        {'c',0x327,0xe7},{'C',0x327,0xc7},{'a',0x30a,0xe5},{'A',0x30a,0xc5},
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
        if (T[i].b == base && T[i].m == mark) return T[i].r;
    return base;
}
static unsigned utf8_raw(const char **ps) {
    const unsigned char *s = (const unsigned char *)*ps;
    unsigned c = s[0];
    int n = 0;
    if (c < 0x80) n = 0;
    else if ((c & 0xe0) == 0xc0) { c &= 0x1f; n = 1; }
    else if ((c & 0xf0) == 0xe0) { c &= 0x0f; n = 2; }
    else if ((c & 0xf8) == 0xf0) { c &= 0x07; n = 3; }
    else { *ps += 1; return '?'; }
    for (int i = 1; i <= n; i++) {
        if ((s[i] & 0xc0) != 0x80) { *ps += i; return '?'; }
        c = (c << 6) | (s[i] & 0x3f);
    }
    *ps += n + 1;
    return c;
}
static unsigned next_char(const char **ps) {
    unsigned c = utf8_raw(ps);
    while (**ps) {   /* folgende Akzent-Zeichen anhaengen */
        const char *peek = *ps;
        unsigned m = utf8_raw(&peek);
        if (m >= 0x300 && m <= 0x36f) { c = compose(c, m); *ps = peek; }
        else break;
    }
    return c;
}

/* eigene Zusatz-Zeichen: Pfeile */
static const unsigned char glyph_play[8]  = {0x02,0x06,0x0e,0x1e,0x0e,0x06,0x02,0x00};
static const unsigned char glyph_up[8]    = {0x00,0x08,0x1c,0x3e,0x7f,0x00,0x00,0x00};
static const unsigned char glyph_down[8]  = {0x00,0x00,0x7f,0x3e,0x1c,0x08,0x00,0x00};
/* eigene Grossbuchstaben mit Umlaut (die Originalschrift hat dafuer zu wenig Platz) */
static const unsigned char glyph_AE[8] = {0x12,0x0c,0x1e,0x33,0x3f,0x33,0x33,0x00};
static const unsigned char glyph_OE[8] = {0x12,0x1e,0x33,0x33,0x33,0x33,0x1e,0x00};
static const unsigned char glyph_UE[8] = {0x12,0x00,0x33,0x33,0x33,0x33,0x3f,0x00};

static const unsigned char *glyph(unsigned c) {
    if (c == 0xc4) return glyph_AE;
    if (c == 0xd6) return glyph_OE;
    if (c == 0xdc) return glyph_UE;
    if (c < 128) return (const unsigned char *)font8x8_basic[c];
    if (c >= 0xa0 && c <= 0xff) return (const unsigned char *)font8x8_ext_latin[c - 0xa0];
    if (c == 0x25b6) return glyph_play;
    if (c == 0x25b2) return glyph_up;
    if (c == 0x25bc) return glyph_down;
    return (const unsigned char *)font8x8_basic['?'];
}
static void draw_glyph(int x, int y, unsigned c, Uint32 col, int scale) {
    const unsigned char *g = glyph(c);
    for (int r = 0; r < 8; r++)
        for (int b = 0; b < 8; b++)
            if (g[r] & (1 << b)) fill(x + b * scale, y + r * scale, scale, scale, col);
}
static int text_len(const char *s) {   /* Anzahl Zeichen */
    int n = 0;
    while (*s) { next_char(&s); n++; }
    return n;
}
/* Text zeichnen; maxchars>0 kuerzt mit ".." */
static void text_ex(int x, int y, const char *s, Uint32 col, int scale, int maxchars) {
    int n = text_len(s), i = 0;
    while (*s) {
        unsigned c = next_char(&s);
        if (maxchars > 0 && n > maxchars && i >= maxchars - 2) {
            draw_glyph(x, y, '.', col, scale); draw_glyph(x + 5 * scale, y, '.', col, scale);
            return;
        }
        draw_glyph(x, y, c, col, scale);
        x += 8 * scale; i++;
    }
}
static void text(int x, int y, const char *s, Uint32 col) { text_ex(x, y, s, col, 1, 0); }
static void text_shadow(int x, int y, const char *s, Uint32 col, int scale) {
    text_ex(x + scale, y + scale, s, COL_DARK, scale, 0);
    text_ex(x, y, s, col, scale, 0);
}
static void text_center(int cx, int y, const char *s, Uint32 col, int scale, int maxw) {
    int n = text_len(s), maxc = maxw / (8 * scale);
    if (n > maxc) n = maxc;
    text_ex(cx - n * 4 * scale, y, s, col, scale, maxc);
}

/* kleine, duenne Schrift (Spleen 5x8): 5 Pixel pro Zeichen */
#define SW 5
static void small_glyph(int x, int y, unsigned c, Uint32 col) {
    if (c < 32 || c > 255) c = '?';
    const unsigned char *g = font_small[c - 32];
    for (int r = 0; r < 8; r++)
        for (int b = 0; b < 5; b++)
            if (g[r] & (0x80 >> b)) px(x + b, y + r, col);
}
static void small_text(int x, int y, const char *s, Uint32 col, int maxchars) {
    int n = text_len(s), i = 0;
    while (*s) {
        unsigned c = next_char(&s);
        if (maxchars > 0 && n > maxchars && i >= maxchars - 2) { small_glyph(x, y, '.', col); small_glyph(x + 3, y, '.', col); return; }
        small_glyph(x, y, c, col); x += SW; i++;
    }
}
static void small_center(int cx, int y, const char *s, Uint32 col, int maxw) {
    int n = text_len(s), maxc = maxw / SW;
    if (n > maxc) n = maxc;
    small_text(cx - n * SW / 2, y, s, col, maxc);
}

/* ------------------------------------------------------------------ Rahmen */

static void bevel(int x, int y, int w, int h, Uint32 face, Uint32 light, Uint32 dark, int inset) {
    fill(x, y, w, h, face);
    Uint32 a = inset ? dark : light, b = inset ? light : dark;
    hline(x, y, w, a); vline(x, y, h, a);
    hline(x, y + h - 1, w, b); vline(x + w - 1, y, h, b);
    /* schwarzer Aussenrand fuer Pixel-Look */
    hline(x - 1, y - 1, w + 2, COL_DARK); hline(x - 1, y + h, w + 2, COL_DARK);
    vline(x - 1, y, h, COL_DARK); vline(x + w, y, h, COL_DARK);
}
/* Gruppen-Rahmen mit Ueberschrift im Rand */
static void groupbox(int x, int y, int w, int h, const char *title, Uint32 inner) {
    fill(x, y, w, h, inner);
    hline(x, y, w, COL_LIGHT); hline(x, y + h - 1, w, COL_LIGHT);
    vline(x, y, h, COL_LIGHT); vline(x + w - 1, y, h, COL_LIGHT);
    hline(x + 1, y + 1, w - 2, COL_DARK); vline(x + 1, y + 1, h - 2, COL_DARK);
    if (title && *title) {
        int tw = text_len(title) * 8;
        fill(x + 6, y - 4, tw + 6, 9, COL_BG);
        text(x + 9, y - 4, title, COL_AMBER);
    }
}

/* ------------------------------------------------------------------ Einstellungen & Pfade */

static char home[1024], conf_dir[1200], conf_file[1300], log_file[1300];
static char games_dir[2048] = "";
static char engine[2048] = "";       /* eingebaute Engine (in der App) */
static char engine_ver[32] = "?";

static void mkdirs(const char *p) {
    char tmp[1300]; snprintf(tmp, sizeof tmp, "%s", p);
    for (char *q = tmp + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
    mkdir(tmp, 0755);
}
/* Wert aus der Einstellungsdatei: Backslash-Escapes (vom alten Starter) aufloesen */
static void unescape(char *s) {
    char *r = s, *w = s;
    if (r[0] == '\'' ) { r++; size_t n = strlen(r); if (n && r[n-1] == '\'') r[n-1] = 0; }
    while (*r) { if (*r == '\\' && r[1]) r++; *w++ = *r++; }
    *w = 0;
}
static void load_settings(void) {
    FILE *f = fopen(conf_file, "r");
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "GAMES_DIR=", 10)) {
            snprintf(games_dir, sizeof games_dir, "%s", line + 10);
            unescape(games_dir);
            size_t n = strlen(games_dir);
            while (n > 1 && games_dir[n-1] == '/') games_dir[--n] = 0;
        }
    }
    fclose(f);
}
static void save_settings(void) {
    mkdirs(conf_dir);
    FILE *f = fopen(conf_file, "w");
    if (!f) return;
    fprintf(f, "GAMES_DIR=%s\n", games_dir);
    fclose(f);
}
static void logf_(const char *fmt, const char *a, const char *b) {
    FILE *f = fopen(log_file, "a");
    if (!f) return;
    fprintf(f, fmt, a ? a : "", b ? b : "");
    fclose(f);
}

static int is_mac_program(const char *path) {
    unsigned char m[4];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(m, 1, 4, f);
    fclose(f);
    if (n != 4) return 0;
    unsigned v = (m[0] << 24) | (m[1] << 16) | (m[2] << 8) | m[3];
    return v == 0xcffaedfe || v == 0xcefaedfe || v == 0xfeedfacf || v == 0xfeedface ||
           v == 0xcafebabe || v == 0xbebafeca;
}
/* Versionsnummer "3.x.y.z" in der Engine-Datei suchen */
static void read_engine_version(void) {
    snprintf(engine_ver, sizeof engine_ver, "?");
    FILE *f = fopen(engine, "rb");
    if (!f) return;
    static char buf[1 << 16];
    size_t n; char win[64] = {0}; int wl = 0;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            char c = buf[i];
            if ((c >= '0' && c <= '9') || c == '.') { if (wl < 63) win[wl++] = c; }
            else {
                win[wl] = 0;
                int dots = 0; for (int k = 0; k < wl; k++) dots += win[k] == '.';
                if (wl >= 7 && dots == 3 && win[0] == '3' && win[1] == '.' && win[wl-1] != '.') {
                    snprintf(engine_ver, sizeof engine_ver, "%s", win); fclose(f); return;
                }
                wl = 0;
            }
        }
    }
    fclose(f);
}

/* AppleScript-Dialog (Ordner/Datei waehlen) – liefert Pfad oder "" */
static int osa(const char *script, char *out, size_t outsz) {
    out[0] = 0;
    char cmd[4096];
    snprintf(cmd, sizeof cmd, "osascript -e 'activate' -e '%s' 2>/dev/null", script);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    if (!fgets(out, (int)outsz, p)) out[0] = 0;
    pclose(p);
    out[strcspn(out, "\r\n")] = 0;
    return out[0] != 0;
}

/* ------------------------------------------------------------------ Spiele suchen */

typedef struct { char name[256]; char path[2048]; } Game;
static Game *games = NULL;
static int ngames = 0, cap = 0;

static int ends_with_ci(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && !strcasecmp(s + a - b, suf);
}
static void add_game(const char *name, const char *path) {
    if (ngames == cap) { cap = cap ? cap * 2 : 32; games = realloc(games, cap * sizeof(Game)); }
    snprintf(games[ngames].name, sizeof games[0].name, "%s", name);
    snprintf(games[ngames].path, sizeof games[0].path, "%s", path);
    ngames++;
}
static int is_dir(const char *p) { struct stat st; return !stat(p, &st) && S_ISDIR(st.st_mode); }
static int is_file(const char *p) { struct stat st; return !stat(p, &st) && S_ISREG(st.st_mode); }
static int exe_has_game(const char *p) {   /* aeltere AGS-Spiele: Kennung "SIGE" am Dateiende */
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    char t[17] = {0};
    if (fseek(f, -16, SEEK_END) == 0) fread(t, 1, 16, f);
    fclose(f);
    for (int i = 0; i < 13; i++) if (!memcmp(t + i, "SIGE", 4)) return 1;
    return 0;
}
/* in einem Ordner nach Spielen schauen; name_from_dir: Ordnername als Spielname */
static void scan_one(const char *dir, const char *dirname_for_name) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    char p[2400], found_ags[2400] = "", found_exe[2400] = "";
    char ags_file[256] = "", exe_file[256] = "";
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        if (!is_file(p)) continue;
        if (ends_with_ci(e->d_name, ".ags")) {
            if (!dirname_for_name) {   /* direkt im Spiele-Ordner: jede .ags ist ein Spiel */
                char nm[256]; snprintf(nm, sizeof nm, "%s", e->d_name); nm[strlen(nm) - 4] = 0;
                add_game(nm, p);
            } else if (!found_ags[0]) { snprintf(found_ags, sizeof found_ags, "%s", p); snprintf(ags_file, sizeof ags_file, "%s", e->d_name); }
        } else if (ends_with_ci(e->d_name, ".exe") && strcasecmp(e->d_name, "winsetup.exe") &&
                   strncasecmp(e->d_name, "unins", 5) && !found_exe[0] && dirname_for_name) {
            if (exe_has_game(p)) { snprintf(found_exe, sizeof found_exe, "%s", p); snprintf(exe_file, sizeof exe_file, "%s", e->d_name); }
        }
    }
    closedir(d);
    if (dirname_for_name) {
        if (found_ags[0]) add_game(dirname_for_name, found_ags);
        else if (found_exe[0]) add_game(dirname_for_name, found_exe);
    }
}
static int cmp_game(const void *a, const void *b) {
    return strcasecmp(((const Game *)a)->name, ((const Game *)b)->name);
}
static void scan_games(void) {
    ngames = 0;
    if (!games_dir[0] || !is_dir(games_dir)) return;
    scan_one(games_dir, NULL);
    DIR *d = opendir(games_dir);
    if (!d) return;
    struct dirent *e; char p[2400];
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s/%s", games_dir, e->d_name);
        if (is_dir(p) && !ends_with_ci(e->d_name, ".app")) scan_one(p, e->d_name);
    }
    closedir(d);
    qsort(games, ngames, sizeof(Game), cmp_game);
}

/* ------------------------------------------------------------------ Oberflaeche */

enum { B_NONE = -1, B_START, B_FOLDER, B_RELOAD, B_SETTINGS, B_INFO, B_QUIT, B_SCRUP, B_SCRDN,
       B_D1, B_D2, B_D3, B_COUNT };
typedef struct { int x, y, w, h; const char *label; int kind; } Btn;   /* kind: 0 normal, 1 PLAY (gruen), 2 klein, 3 dunkel */
static Btn btn[B_COUNT];

/* Layout */
#define SCENE_Y   0
#define SENT_Y    66
#define LIST_X    8
#define LIST_Y    84
#define LIST_W    246
#define LIST_H    104
#define ROW_H     12
#define ROW_Y0    (LIST_Y + 7)
#define VIS_ROWS  ((LIST_H - 9) / ROW_H)
#define COL_X     266
#define COL_W     110
#define START_Y   196
#define STAT_Y    226

static int sel = 0, top_row = 0, hover = B_NONE, pressed = B_NONE, hover_row = -1, pressed_row = -1;
static int dialog = 0;                /* 0 keiner, 1 Einstellungen, 2 Info, 3 Willkommen */
static char msg[256] = "";            /* kurze Meldung in der Satzzeile */
static Uint32 msg_until = 0;
static int running = 1;

static void set_msg(const char *s) { snprintf(msg, sizeof msg, "%s", s); msg_until = SDL_GetTicks() + 4000; }

static void layout(void) {
    for (int i = 0; i < B_COUNT; i++) btn[i] = (Btn){0, 0, 0, 0, NULL, 0};
    btn[B_START]    = (Btn){LIST_X, START_Y, LIST_W, 22, "PLAY", 1};
    int y = LIST_Y;
    btn[B_FOLDER]   = (Btn){COL_X, y, COL_W, 20, "OPEN FOLDER", 0}; y += 26;
    btn[B_RELOAD]   = (Btn){COL_X, y, COL_W, 20, "RESCAN", 0};     y += 26;
    btn[B_SETTINGS] = (Btn){COL_X, y, COL_W, 20, "SETTINGS", 0}; y += 26;
    btn[B_INFO]     = (Btn){COL_X, y, COL_W, 20, "ABOUT", 0};
    btn[B_QUIT]     = (Btn){COL_X, START_Y, COL_W, 22, "QUIT", 3};
    btn[B_SCRUP]    = (Btn){LIST_X + LIST_W - 11, LIST_Y + 2, 9, 9, "\xe2\x96\xb2", 2};
    btn[B_SCRDN]    = (Btn){LIST_X + LIST_W - 11, LIST_Y + LIST_H - 11, 9, 9, "\xe2\x96\xbc", 2};
    if (dialog) {   /* Dialog-Knoepfe */
        int dx = 52, dw = 280, by = 168;
        if (dialog == 1) {
            btn[B_D1] = (Btn){dx + 12, by - 52, dw - 24, 18, "CHOOSE GAMES FOLDER", 0};
            btn[B_D2] = (Btn){dx + 12, by - 28, dw - 24, 18, "UPDATE AGS ENGINE", 0};
            btn[B_D3] = (Btn){dx + 12, by,      dw - 24, 18, "DONE", 1};
        } else if (dialog == 2) {
            btn[B_D3] = (Btn){dx + (dw - 100) / 2, by, 100, 18, "OK", 1};
        } else if (dialog == 3) {
            btn[B_D1] = (Btn){dx + 12, by, 172, 18, "CHOOSE GAMES FOLDER", 1};
            btn[B_D3] = (Btn){dx + dw - 12 - 76, by, 76, 18, "LATER", 0};
        }
    }
}
static int btn_active(int i) {
    if (!btn[i].label) return 0;
    if (dialog) return i >= B_D1;
    if (i >= B_D1) return 0;
    if (i == B_START) return ngames > 0;
    if (i == B_SCRUP || i == B_SCRDN) return ngames > VIS_ROWS;
    if (i == B_FOLDER) return games_dir[0] != 0;
    return 1;
}
static int hit_btn(int x, int y) {
    for (int i = 0; i < B_COUNT; i++)
        if (btn_active(i) && x >= btn[i].x && y >= btn[i].y && x < btn[i].x + btn[i].w && y < btn[i].y + btn[i].h)
            return i;
    return B_NONE;
}
static int hit_row(int x, int y) {
    if (dialog) return -1;
    if (x < LIST_X + 2 || x >= LIST_X + LIST_W - 13 || y < ROW_Y0 || y >= ROW_Y0 + VIS_ROWS * ROW_H) return -1;
    int r = top_row + (y - ROW_Y0) / ROW_H;
    return r < ngames ? r : -1;
}
static void clamp_sel(void) {
    if (sel >= ngames) sel = ngames - 1;
    if (sel < 0) sel = 0;
    if (sel < top_row) top_row = sel;
    if (sel >= top_row + VIS_ROWS) top_row = sel - VIS_ROWS + 1;
    if (top_row > ngames - VIS_ROWS) top_row = ngames - VIS_ROWS;
    if (top_row < 0) top_row = 0;
}

static void draw_button(int i) {
    Btn *b = &btn[i];
    int active = btn_active(i);
    int down = active && pressed == i && hover == i;
    int hov  = active && hover == i && (pressed == B_NONE || pressed == i);
    Uint32 face, light, dark, tc;
    if (b->kind == 1) { face = down ? COL_GODOWN : hov ? COL_GOHOV : COL_GO; light = COL_GOLIGHT; dark = COL_GODARK; tc = 0xfff6ea; }
    else if (b->kind == 3) { face = down ? 0x0d1326 : hov ? 0x2a3866 : 0x18213f; light = 0x56648f; dark = 0x04060c; tc = 0xffffff; }
    else              { face = down ? COL_BTNDOWN : hov ? COL_BTNHOV : COL_BTN; light = COL_BTNLIGHT; dark = COL_BTNDARK; tc = COL_BTNTEXT; }
    if (!active) { face = 0x3a4060; light = 0x545a7c; dark = 0x1a1e30; tc = 0x6a7090; }
    bevel(b->x, b->y, b->w, b->h, face, light, dark, down);
    int off = down ? 1 : 0;
    if (b->kind == 2) { draw_glyph(b->x + 1 + off, b->y + 1 + off, next_char(&(const char *){b->label}), tc, 1); return; }
    if (i == B_START) {   /* grosser START-Knopf mit Pfeil */
        int tw = 2 * 8 * text_len(b->label) + 14;
        int x0 = b->x + (b->w - tw) / 2 + off, y0 = b->y + 3 + off;
        draw_glyph(x0 + 1, y0 + 4, 0x25b6, COL_DARK, 1); draw_glyph(x0, y0 + 3, 0x25b6, tc, 1);
        text_ex(x0 + 15, y0 + 1, b->label, COL_DARK, 2, 0);
        text_ex(x0 + 14, y0, b->label, tc, 2, 0);
        return;
    }
    text_center(b->x + b->w / 2 + off, b->y + (b->h - 8) / 2 + off, b->label, tc, 1, b->w - 4);
}

static void sentence(char *out, size_t n) {   /* Satzzeile wie in alten Adventures */
    if (msg[0] && SDL_GetTicks() < msg_until) { snprintf(out, n, "%s", msg); return; }
    if (dialog) { out[0] = 0; return; }
    switch (hover) {
        case B_START:    if (ngames) { snprintf(out, n, "Play %s", games[sel].name); return; } break;
        case B_FOLDER:   snprintf(out, n, "Open the games folder in Finder"); return;
        case B_RELOAD:   snprintf(out, n, "Scan the folder for new games"); return;
        case B_SETTINGS: snprintf(out, n, "Games folder and AGS engine"); return;
        case B_INFO:     snprintf(out, n, "About AGS Starter"); return;
        case B_QUIT:     snprintf(out, n, "Quit AGS Starter"); return;
    }
    if (hover_row >= 0) { snprintf(out, n, "Select %s", games[hover_row].name); return; }
    if (ngames) snprintf(out, n, "Play %s", games[sel].name);
    else if (!games_dir[0]) snprintf(out, n, "No games folder chosen yet");
    else snprintf(out, n, "No games found");
}

static void draw_dialog(void) {
    darken_all();
    int x = 52, y = 46, w = 280, h = 148;
    bevel(x, y, w, h, COL_PANEL, COL_BTNLIGHT, COL_BTNDARK, 0);
    fill(x + 1, y + 1, w - 2, 14, 0x374a7b);
    hline(x + 1, y + 15, w - 2, COL_BTNDARK);
    const char *title = dialog == 1 ? "SETTINGS" : dialog == 2 ? "ABOUT" : "WELCOME!";
    text_center(x + w / 2, y + 4, title, COL_AMBER, 1, w - 8);
    char line[512];
    int ty = y + 23;
    if (dialog == 1) {
        small_text(x + 12, ty, "Games folder:", COL_DIM, 0); ty += 10;
        small_text(x + 12, ty, games_dir[0] ? games_dir : "(none yet)", COL_TEXT, (w - 24) / SW);
        if (games_dir[0] && text_len(games_dir) > (w - 24) / SW) {   /* lange Pfade: Ende zeigen */
            fill(x + 12, ty, w - 24, 8, COL_PANEL);
            const char *e = games_dir; int n = text_len(e);
            while (n > (w - 24) / SW - 2) { next_char(&e); n--; }
            snprintf(line, sizeof line, "..%s", e);
            small_text(x + 12, ty, line, COL_TEXT, 0);
        }
        ty += 13;
        snprintf(line, sizeof line, "AGS engine version: %s", engine_ver);
        small_text(x + 12, ty, line, COL_DIM, 0);
    } else if (dialog == 2) {
        text_center(x + w / 2, ty - 2, "AGS STARTER for Mac", COL_AMBER, 1, w - 8); ty += 10;
        small_center(x + w / 2, ty, "Version " APP_VERSION " \xc2\xb7 made by MENRIX (Don_Key_Moustache)", COL_TEXT, w - 8); ty += 13;
        small_center(x + w / 2, ty, "AGS = Adventure Game Studio", COL_TEXT, w - 8); ty += 9;
        snprintf(line, sizeof line, "Built-in AGS engine: version %s", engine_ver);
        small_center(x + w / 2, ty, line, COL_SKY, w - 8); ty += 13;
        small_text(x + 14, ty, "\xc2\xb7 Click a game, then PLAY", COL_DIM, 0); ty += 9;
        small_text(x + 14, ty, "\xc2\xb7 Double-click or Enter plays right away", COL_DIM, 0); ty += 9;
        small_text(x + 14, ty, "\xc2\xb7 Buttons act when you release the mouse", COL_DIM, 0); ty += 9;
        small_text(x + 14, ty, "\xc2\xb7 Drag the window bigger to zoom", COL_DIM, 0); ty += 9;
        small_text(x + 14, ty, "\xc2\xb7 Esc quits", COL_DIM, 0);
    } else if (dialog == 3) {
        small_text(x + 12, ty, "Choose the folder that holds your AGS games.", COL_TEXT, 0); ty += 14;
        small_text(x + 12, ty, "Tip: put each game in its own subfolder,", COL_DIM, 0); ty += 9;
        small_text(x + 12, ty, "e.g. Games/Aftergold/Aftergold.ags", COL_DIM, 0);
    }
    for (int i = B_D1; i < B_COUNT; i++) if (btn[i].label) draw_button(i);
}

static void render(void) {
    /* Hintergrund */
    for (int i = 0; i < LW * LH; i++) fb[i] = COL_BG;
    /* Szene */
    if (have_header) {
        fill(0, 0, LW, SCENE_H, KEY);   /* Header-Bild wird spaeter darunter gezeichnet */
    } else {                            /* Ersatz: gezeichnete Szene */
        int fr = (SDL_GetTicks() / 380) % SCENE_FRAMES;
        for (int y = 0; y < SCENE_H; y++)
            for (int x = 0; x < SCENE_W; x++)
                fb[(SCENE_Y + y) * LW + x] = scene_pal[scene_px[fr][y * SCENE_W + x]];
    }
    /* Titel zweizeilig: grosses "AGS", darunter "STARTER" */
    { int x1 = (LW - 3 * 24) / 2, x2 = (LW - 7 * 16) / 2;
      text_ex(x1 + 2, 5, "AGS", COL_DARK, 3, 0);     text_ex(x1 + 1, 4, "AGS", 0x18274e, 3, 0);
      text_ex(x1, 3, "AGS", 0xffd59a, 3, 0);
      text_ex(x2 + 2, 32, "STARTER", COL_DARK, 2, 0); text_ex(x2 + 1, 31, "STARTER", 0x18274e, 2, 0);
      text_ex(x2, 30, "STARTER", 0xf2b27a, 2, 0);
      small_center(LW / 2 + 1, 49, "for Mac", COL_DARK, 100);   /* klein darunter: "for Mac" */
      small_center(LW / 2, 48, "for Mac", 0xefe6cf, 100); }
    hline(0, SCENE_H, LW, COL_DARK); hline(0, SCENE_H + 1, LW, COL_LIGHT);
    /* Satzzeile */
    char s[512]; sentence(s, sizeof s);
    fill(0, SENT_Y - 2, LW, 13, COL_DARK);
    small_center(LW / 2, SENT_Y, s, COL_SKY, LW - 8);
    hline(0, SENT_Y + 11, LW, COL_LIGHT);
    /* Spieleliste */
    groupbox(LIST_X, LIST_Y, LIST_W, LIST_H, "GAMES", COL_LIST);
    clamp_sel();
    if (ngames == 0) {
        text_center(LIST_X + LIST_W / 2, LIST_Y + 34, games_dir[0] ? "No games found" : "No games folder", COL_DIM, 1, LIST_W - 8);
        text_center(LIST_X + LIST_W / 2, LIST_Y + 50, "-> SETTINGS", COL_AMBER, 1, LIST_W - 8);
    }
    for (int r = 0; r < VIS_ROWS; r++) {
        int idx = top_row + r;
        if (idx >= ngames) break;
        int ry = ROW_Y0 + r * ROW_H, rw = LIST_W - 4 - (ngames > VIS_ROWS ? 12 : 0);
        Uint32 tc = COL_TEXT;
        if (idx == sel)            { fill(LIST_X + 2, ry, rw, ROW_H, COL_ROWSEL); tc = COL_ROWSELTX; }
        else if (idx == hover_row) { fill(LIST_X + 2, ry, rw, ROW_H, COL_ROWHOVER); }
        if (idx == sel) draw_glyph(LIST_X + 5, ry + 2, 0x25b6, COL_ROWSELTX, 1);
        text_ex(LIST_X + 16, ry + 2, games[idx].name, tc, 1, (rw - 16) / 8);
    }
    if (ngames > VIS_ROWS) {   /* Bildlaufleiste */
        int sx = LIST_X + LIST_W - 11, sy = LIST_Y + 12, sh = LIST_H - 24;
        fill(sx, sy, 9, sh, COL_TRACK);
        int th = sh * VIS_ROWS / ngames; if (th < 8) th = 8;
        int ty = sy + (sh - th) * top_row / (ngames - VIS_ROWS);
        bevel(sx + 1, ty, 7, th, COL_BTN, COL_BTNLIGHT, COL_BTNDARK, 0);
        draw_button(B_SCRUP); draw_button(B_SCRDN);
    }
    /* Knoepfe */
    for (int i = B_START; i <= B_QUIT; i++) draw_button(i);
    /* Statusleiste */
    fill(0, STAT_Y - 2, LW, LH - STAT_Y + 2, COL_STATUS);
    hline(0, STAT_Y - 3, LW, COL_LIGHT);
    char st[256];
    snprintf(st, sizeof st, "AGS = Adventure Game Studio \xc2\xb7 Engine %s", engine_ver);
    small_center(LW / 2, STAT_Y + 3, st, COL_AMBER, LW - 8);
    if (dialog) draw_dialog();
}

/* ------------------------------------------------------------------ Aktionen */

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex, *htex;
static Uint32 outbuf[LW * LH];

/* Header-Animation: Bildkoordinaten -> Fensterkoordinaten */
#define HX(x) ((float)(x) * LW / HDR_IMG_W)
#define HY(y) ((float)(y) * SCENE_H / HDR_IMG_H)
static void glow(float x, float y, float size, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_FRect q = {x - size / 2, y - size / 2, size, size};
    SDL_SetRenderDrawColor(ren, r, g, b, a);
    SDL_RenderFillRectF(ren, &q);
}
static void draw_header_anim(void) {
    double t = SDL_GetTicks() / 1000.0;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    /* Bildschirme: leichtes Flackern + wandernder Bildlauf-Streifen (alter Roehrenmonitor) */
    const float *sc[2] = {scr_win, scr_mac};
    for (int k = 0; k < 2; k++) {
        float x = HX(sc[k][0]), y = HY(sc[k][1]), w = HX(sc[k][2]), h = HY(sc[k][3]);
        SDL_FRect all = {x, y, w, h};
        SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)(8 + 6 * sin(t * 11 + k * 2)));
        SDL_RenderFillRectF(ren, &all);
        double u = fmod(t / 2.6 + k * 0.5, 1.0);
        SDL_FRect band = {x, y + (float)(u * h * 0.88), w, h * 0.12f};
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 34);
        SDL_RenderFillRectF(ren, &band);
    }
    /* Sterne funkeln */
    for (int i = 0; i < STARS_N; i++) {
        double a = 0.5 + 0.5 * sin(t * 1.9 + i * 1.7);
        if (a < 0.6) continue;
        float x = HX(hdr_stars[i][0]), y = HY(hdr_stars[i][1]);
        Uint8 al = (Uint8)(255 * (a - 0.6) / 0.4);
        SDL_FRect hz = {x - 1.6f, y - 0.25f, 3.2f, 0.5f}, vt = {x - 0.25f, y - 1.6f, 0.5f, 3.2f};
        SDL_SetRenderDrawColor(ren, 255, 255, 240, al);
        SDL_RenderFillRectF(ren, &hz); SDL_RenderFillRectF(ren, &vt);
    }
    /* Daten-Impulse wandern durchs Kabel: vom Windows-PC zum Mac */
    for (int k = 0; k < 3; k++) {
        double u = fmod(t / 3.6 + k / 3.0, 1.0);
        int i = (int)(u * (CABLE_N - 1));
        for (int j = 10; j >= 1; j--) {   /* Schweif */
            int ti = i - j * 3; if (ti < 0) continue;
            glow(HX(cable_path[ti][0]), HY(cable_path[ti][1]), 1.4f - j * 0.06f, 120, 255, 200, (Uint8)(190 - j * 17));
        }
        float x = HX(cable_path[i][0]), y = HY(cable_path[i][1]);
        glow(x, y, 5.0f, 90, 255, 190, 40);
        glow(x, y, 3.0f, 150, 255, 210, 120);
        glow(x, y, 1.7f, 240, 255, 248, 255);
    }
}

static void present(void) {
    render();
    for (int i = 0; i < LW * LH; i++) outbuf[i] = fb[i] == KEY ? 0 : (fb[i] | 0xff000000u);
    SDL_UpdateTexture(tex, NULL, outbuf, LW * 4);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (have_header) {
        SDL_Rect hr = {0, 0, LW, SCENE_H};
        SDL_RenderCopy(ren, htex, NULL, &hr);
        draw_header_anim();
        if (dialog) {   /* bei Dialogen auch den Header abdunkeln */
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
            SDL_RenderFillRect(ren, &hr);
        }
    }
    SDL_RenderCopy(ren, tex, NULL, NULL);
}
static void show(void) { present(); SDL_RenderPresent(ren); }

static void choose_games_dir(void) {
    char out[2048];
    if (osa("POSIX path of (choose folder with prompt \"Where are your AGS games?\")", out, sizeof out)) {
        size_t n = strlen(out);
        while (n > 1 && out[n-1] == '/') out[--n] = 0;
        snprintf(games_dir, sizeof games_dir, "%s", out);
        save_settings(); scan_games(); sel = 0; top_row = 0;
        char m[300]; snprintf(m, sizeof m, "%d game(s) found", ngames); set_msg(m);
    }
}
static int copy_file(const char *src, const char *dst) {
    FILE *a = fopen(src, "rb"); if (!a) return 0;
    char tmp[2100]; snprintf(tmp, sizeof tmp, "%s.neu", dst);
    FILE *b = fopen(tmp, "wb"); if (!b) { fclose(a); return 0; }
    char buf[65536]; size_t n; int ok = 1;
    while ((n = fread(buf, 1, sizeof buf, a)) > 0) if (fwrite(buf, 1, n, b) != n) { ok = 0; break; }
    fclose(a); if (fclose(b)) ok = 0;
    if (ok) { chmod(tmp, 0755); ok = rename(tmp, dst) == 0; } else unlink(tmp);
    return ok;
}
static void update_engine(void) {
    char out[2048];
    if (!osa("POSIX path of (choose file with prompt \"Choose the new AGS engine (the file ags, no extension)\")", out, sizeof out)) return;
    if (!is_mac_program(out)) { set_msg("Not a Mac engine (choose the file \"ags\", no extension)"); return; }
    if (!copy_file(out, engine)) { set_msg("Could not copy the engine into the app"); return; }
    char cmd[2300]; snprintf(cmd, sizeof cmd, "xattr -c \"%s\" 2>/dev/null", engine); system(cmd);
    read_engine_version();
    char m[100]; snprintf(m, sizeof m, "AGS engine updated: version %s", engine_ver); set_msg(m);
}
static void open_folder(void) {
    if (!games_dir[0]) return;
    pid_t p = fork();
    if (p == 0) { execlp("open", "open", games_dir, (char *)NULL); _exit(127); }
    if (p > 0) waitpid(p, NULL, 0);
    set_msg("Folder opened - added games? Then RESCAN");
}

static void play(void) {
    if (ngames == 0) return;
    if (!is_file(engine)) { set_msg("AGS engine missing in the app! -> SETTINGS"); return; }
    Game *g = &games[sel];
    char m[300]; snprintf(m, sizeof m, "Loading %s ...", g->name); set_msg(m);
    hover = B_NONE; pressed = B_NONE;
    show();
    time_t t = time(NULL); char ts[64]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
    logf_("\n===== %s  Start: %s =====\n", ts, g->name);
    logf_("File: %s%s\n", g->path, "");
    char gdir[2048]; snprintf(gdir, sizeof gdir, "%s", g->path);
    char *slash = strrchr(gdir, '/'); if (slash) *slash = 0;
    SDL_Delay(150);
    pid_t pid = fork();
    if (pid == 0) {
        int fd = open(log_file, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
        if (chdir(gdir) != 0) _exit(126);
        execl(engine, "ags", g->path, (char *)NULL);
        _exit(127);
    }
    if (pid < 0) { set_msg("Could not start the game"); return; }
    SDL_HideWindow(win);
    int status = 0;
    for (;;) {   /* warten, bis das Spiel beendet ist; dabei das Fenster "lebendig" halten */
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || (r < 0 && errno != EINTR)) break;
        SDL_PumpEvents();
        SDL_Delay(100);
    }
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_ShowWindow(win); SDL_RaiseWindow(win);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    char c[32]; snprintf(c, sizeof c, "%d", code);
    logf_("===== End (code %s) =====%s\n", c, "");
    if (code == 0) set_msg("Thanks for playing!");
    else { snprintf(m, sizeof m, "Error (code %d) - see log in ~/Library/Logs", code); set_msg(m); }
}

static void do_button(int i) {
    switch (i) {
        case B_START:    play(); break;
        case B_FOLDER:   open_folder(); break;
        case B_RELOAD:   scan_games(); clamp_sel(); { char m[100]; snprintf(m, sizeof m, "Rescanned: %d game(s)", ngames); set_msg(m); } break;
        case B_SETTINGS: dialog = 1; break;
        case B_INFO:     dialog = 2; break;
        case B_QUIT:     running = 0; break;
        case B_SCRUP:    if (top_row > 0) top_row--; if (sel >= top_row + VIS_ROWS) sel = top_row + VIS_ROWS - 1; break;
        case B_SCRDN:    if (top_row < ngames - VIS_ROWS) top_row++; if (sel < top_row) sel = top_row; break;
        case B_D1:       if (dialog == 1 || dialog == 3) { dialog = 0; choose_games_dir(); } break;
        case B_D2:       if (dialog == 1) { dialog = 0; update_engine(); } break;
        case B_D3:       dialog = 0; break;
    }
    layout();
}

/* ------------------------------------------------------------------ Testmodus (nur zum Testen auf Linux) */

static FILE *test_script = NULL;
static int test_wait = 0;
static void push_mouse(int type, int x, int y, int clicks) {
    SDL_Event e; memset(&e, 0, sizeof e);
    if (type == SDL_MOUSEMOTION) { e.type = type; e.motion.x = x; e.motion.y = y; }
    else { e.type = type; e.button.button = SDL_BUTTON_LEFT; e.button.x = x; e.button.y = y; e.button.clicks = clicks;
           e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED; }
    SDL_PushEvent(&e);
}
static void test_step(void) {
    if (!test_script) return;
    if (test_wait > 0) { test_wait--; return; }
    char line[512];
    if (!fgets(line, sizeof line, test_script)) { running = 0; return; }
    char cmd[32] = ""; int a = 0, b = 0; char arg[400] = "";
    sscanf(line, "%31s", cmd);
    if (!strcmp(cmd, "wait")) { sscanf(line, "%*s %d", &test_wait); }
    else if (!strcmp(cmd, "move")) { sscanf(line, "%*s %d %d", &a, &b); push_mouse(SDL_MOUSEMOTION, a, b, 0); }
    else if (!strcmp(cmd, "down")) { sscanf(line, "%*s %d %d", &a, &b); push_mouse(SDL_MOUSEBUTTONDOWN, a, b, 1); }
    else if (!strcmp(cmd, "up"))   { sscanf(line, "%*s %d %d", &a, &b); push_mouse(SDL_MOUSEBUTTONUP, a, b, 1); }
    else if (!strcmp(cmd, "dbl"))  { sscanf(line, "%*s %d %d", &a, &b); push_mouse(SDL_MOUSEBUTTONDOWN, a, b, 2); push_mouse(SDL_MOUSEBUTTONUP, a, b, 2); }
    else if (!strcmp(cmd, "key"))  { sscanf(line, "%*s %399s", arg);
        SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDL_GetKeyFromName(arg); SDL_PushEvent(&e); }
    else if (!strcmp(cmd, "wheel")) { sscanf(line, "%*s %d", &a);
        SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_MOUSEWHEEL; e.wheel.y = a; SDL_PushEvent(&e); }
    else if (!strcmp(cmd, "shot"))  { sscanf(line, "%*s %399s", arg);
        present();
        int ow, oh; SDL_GetRendererOutputSize(ren, &ow, &oh);
        Uint32 *pix = malloc((size_t)ow * oh * 4);
        SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, pix, ow * 4);
        FILE *f = fopen(arg, "wb");
        if (f) { fprintf(f, "P6\n%d %d\n255\n", ow, oh);
                 for (int i = 0; i < ow * oh; i++) { unsigned char c[3] = {pix[i] >> 16, pix[i] >> 8, pix[i]}; fwrite(c, 1, 3, f); }
                 fclose(f); }
        free(pix); }
    else if (!strcmp(cmd, "print")) { printf("STATE sel=%d top=%d ngames=%d dialog=%d hover=%d pressed=%d msg=%s\n", sel, top_row, ngames, dialog, hover, pressed, msg); fflush(stdout); }
}

/* ------------------------------------------------------------------ Hauptprogramm */

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    signal(SIGPIPE, SIG_IGN);
    const char *h = getenv("HOME"); snprintf(home, sizeof home, "%s", h ? h : "/tmp");
    snprintf(conf_dir, sizeof conf_dir, "%s/Library/Application Support/AGS Starter", home);
    snprintf(conf_file, sizeof conf_file, "%s/settings", conf_dir);
    char logdir[1200]; snprintf(logdir, sizeof logdir, "%s/Library/Logs", home); mkdirs(logdir);
    snprintf(log_file, sizeof log_file, "%s/AGS Starter.log", logdir);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");                /* scharfe Pixel */
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }

    char *base = SDL_GetBasePath();                                 /* in der App: .../Contents/Resources/ */
    snprintf(engine, sizeof engine, "%sags", base ? base : "./");
    if (base) SDL_free(base);
    read_engine_version();
    load_settings();
    scan_games();

    win = SDL_CreateWindow("AGS Starter", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           LW * ZOOM, LH * ZOOM, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { fprintf(stderr, "Fenster: %s\n", SDL_GetError()); return 1; }
    SDL_SetWindowMinimumSize(win, LW, LH);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_RenderSetLogicalSize(ren, LW, LH);    /* Inhalt wird mit dem Fenster gezoomt */
    SDL_RenderSetIntegerScale(ren, SDL_TRUE); /* nur ganze Zoomstufen = gleichmaessige Pixel */
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, LW, LH);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    {   /* Header-Bild laden (liegt in der App unter Resources/header.bmp) */
        char hp[2100]; char *b2 = SDL_GetBasePath();
        snprintf(hp, sizeof hp, "%sheader.bmp", b2 ? b2 : "./");
        if (b2) SDL_free(b2);
        SDL_Surface *hs = SDL_LoadBMP(hp);
        if (hs) {
            htex = SDL_CreateTextureFromSurface(ren, hs);
            SDL_FreeSurface(hs);
            if (htex) { SDL_SetTextureScaleMode(htex, SDL_ScaleModeLinear); have_header = 1; }
        }
    }

    const char *ts = getenv("AGS_STARTER_TEST");
    if (ts) test_script = fopen(ts, "r");

    if (!games_dir[0] || !is_dir(games_dir)) dialog = 3;   /* erster Start: Willkommen */
    layout();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_MOUSEMOTION:
                hover = hit_btn(e.motion.x, e.motion.y);
                hover_row = (pressed == B_NONE) ? hit_row(e.motion.x, e.motion.y) : -1;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button != SDL_BUTTON_LEFT) break;
                hover = hit_btn(e.button.x, e.button.y);
                if (hover != B_NONE) { pressed = hover; break; }
                { int r = hit_row(e.button.x, e.button.y);
                  if (r >= 0) { sel = r; pressed_row = r; msg[0] = 0; } }
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button != SDL_BUTTON_LEFT) break;
                { int h2 = hit_btn(e.button.x, e.button.y);
                  int p = pressed; pressed = B_NONE; hover = h2;
                  if (p != B_NONE && p == h2) do_button(p);          /* nur wenn ueber dem Knopf losgelassen */
                  int r = hit_row(e.button.x, e.button.y);
                  if (p == B_NONE && r >= 0 && r == pressed_row && e.button.clicks >= 2 && r == sel) play(); /* Doppelklick */
                  pressed_row = -1; }
                break;
            case SDL_MOUSEWHEEL:
                if (dialog) break;
                if (e.wheel.y > 0 && top_row > 0) top_row--;
                if (e.wheel.y < 0 && top_row < ngames - VIS_ROWS) top_row++;
                if (sel < top_row) sel = top_row;
                if (sel >= top_row + VIS_ROWS) sel = top_row + VIS_ROWS - 1;
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode k = e.key.keysym.sym;
                if (dialog) {
                    if (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_KP_ENTER) { dialog = 0; layout(); }
                    break;
                }
                msg[0] = 0;
                if (k == SDLK_UP) sel--;
                else if (k == SDLK_DOWN) sel++;
                else if (k == SDLK_PAGEUP) sel -= VIS_ROWS;
                else if (k == SDLK_PAGEDOWN) sel += VIS_ROWS;
                else if (k == SDLK_HOME) sel = 0;
                else if (k == SDLK_END) sel = ngames - 1;
                else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) play();
                else if (k == SDLK_ESCAPE) running = 0;
                else if (k == SDLK_F5) do_button(B_RELOAD);
                clamp_sel();
                break; }
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_LEAVE) { hover = B_NONE; hover_row = -1; }
                break;
            }
            if (e.type == SDL_MOUSEBUTTONUP || e.type == SDL_KEYDOWN) layout();
        }
        test_step();
        show();
        if (!test_script) SDL_Delay(16);
    }
    SDL_Quit();
    return 0;
}
