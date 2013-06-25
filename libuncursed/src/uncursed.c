/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Copyright (c) 2013 Alex Smith. */
/* The 'uncursed' rendering library may be distributed under either of the
 * following licenses:
 *  - the NetHack general public license
 *  - the GNU General Public license v2 or later
 * If you obtained uncursed as part of NetHack 4, you can find these licenses in
 * the files libnethack/dat/license and libnethack/dat/gpl respectively.
 */
/* This library aims for source compatibility with the 'ncurses' library,
   containing many the same function calls, variables, etc. (some calls are left
   unimplemented, either due to being terminal-specific, due to being rarely
   used ncurses extensions, or because they would have added considerable
   complexity for little gain).  It does not aim to produce the same output;
   ncurses aims to adapt output appropriately for the terminal the user is
   using, whereas uncursed has multiple output backends, with the terminal
   backend aiming for a lowest common denominator output, rather than an output
   customized to any specific terminal.  As such, some of the methods in ncurses
   have trivial or no-op implementations.  uncursed also provides a few methods
   of its own.

   Note that there should be no platform-specific code at all in this file.
   That goes in the other files, e.g. tty.c.
*/

#define _ISOC99_SOURCE
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> /* for vsnprintf, this file does no I/O */

#include "uncursed.h"
#include "uncursed_hooks.h"

#define min(a,b) ((a) < (b) ? (a) : (b))

/* manual page 3ncurses color */
/* Color pairs are kind-of pointless for rendering purposes on modern terminals,
   but are used in the source. They do have a kind-of use for "palette change"
   like activities where the source sets color pairs, and then recolors the
   screen via changing the color pairs. As such, we record color pairs in the
   window content, and change them to colors at the last possible moment. */
int COLORS = 16; /* externally visible */
int COLOR_PAIRS = 32767; /* externally visible; must fit into 15 bits */
static uncursed_color (*pair_content_list)[2] = 0; /* dynamically allocated */
static uncursed_color pair_content_alloc_count = -1;

int start_color(void) {return OK;} /* no-op */

#define DEFAULT_FOREGROUND COLOR_WHITE
#define DEFAULT_BACKGROUND COLOR_BLACK
int init_pair(uncursed_color pairnum,
              uncursed_color fgcolor, uncursed_color bgcolor) {
    if (pairnum <= 0) return ERR;
    if (pairnum > pair_content_alloc_count) {
        pair_content_list = realloc(pair_content_list,
                                    (pairnum+1) * sizeof(*pair_content_list));
        if (!pair_content_list) {
            pair_content_alloc_count = -1;
            return ERR;
        }
        int default_f, default_b;
        if (pair_content_alloc_count < 0) {
            default_f = DEFAULT_FOREGROUND;
            default_b = DEFAULT_BACKGROUND;
        } else {
            default_f = pair_content_list[0][0];
            default_b = pair_content_list[0][1];
        }
        while (pair_content_alloc_count < pairnum) {
            pair_content_list[pair_content_alloc_count][0] = default_f;
            pair_content_list[pair_content_alloc_count][1] = default_b;
            pair_content_alloc_count++;
        }
    }
    pair_content_list[pairnum][0] = fgcolor;
    pair_content_list[pairnum][1] = bgcolor;
    return OK;
}
uncursed_bool has_colors(void) {return 1;}

/* We could actually implement this vaguely portably, although it would involve
   refreshing the screen (both to update the colors, and because the relevant
   output would be garbage on some terminals).  The problem comes when you try
   to maintain the color palette (e.g. for people who start watching halfway
   through, or for buggy terminals). */
int init_color(uncursed_color colornum, short r, short g, short b) {
    (void)colornum; (void)r; (void)g; (void)b;
    return OK;
}
uncursed_bool can_change_color(void) {return 0;}
int color_content(uncursed_color c, short *r, short *g, short *b) {
    /* We don't know, but here's a reasonable guess... */
    if (c < 0 || c > 15) return ERR;
    else if (c == 7) {*r = 750; *g = 750; *b = 750;}
    else if (c == 8) {*r = 500; *g = 500; *b = 500;}
    if (c != 7 && c != 8) {
        *r = (c & 1) ? (c >= 8 ? 1000 : 500) : 0;
        *g = (c & 2) ? (c >= 8 ? 1000 : 500) : 0;
        *b = (c & 4) ? (c >= 8 ? 1000 : 500) : 0;
    }
    return OK;
}
int pair_content(uncursed_color pairnum,
                 uncursed_color *fgcolor, uncursed_color *bgcolor) {
    if (pairnum < 0) {
        return ERR;
    } else if (pair_content_alloc_count < 0) {
        *fgcolor = DEFAULT_FOREGROUND;
        *bgcolor = DEFAULT_BACKGROUND;
    } else if (pairnum > pair_content_alloc_count) {
        *fgcolor = pair_content_list[0][0];
        *bgcolor = pair_content_list[0][1];
    } else {
        *fgcolor = pair_content_list[pairnum][0];
        *bgcolor = pair_content_list[pairnum][1];
    }
    return OK;
}

/* manual page 3ncurses attr */
UNCURSED_ANDWINDOWDEF(int, attrset, attr_t attr, attr) {
    win->current_attr = attr;
    return OK;
}
UNCURSED_ANDWINDOWDEF(int, attron,  attr_t attr, attr) {
    win->current_attr |= attr;
    return OK;
}
UNCURSED_ANDWINDOWDEF(int, attroff, attr_t attr, attr) {
    win->current_attr &= ~attr;
    return OK;
}

UNCURSED_ANDWINDOWDEF(int, color_set, uncursed_color pairnum, pairnum) {
    win->current_attr &= ~COLOR_PAIR(PAIR_NUMBER(win->current_attr));
    win->current_attr |= COLOR_PAIR(pairnum);
    return OK;
}

UNCURSED_ANDWINDOWVDEF(int, standout) {
    wattron(win, A_STANDOUT);
    return OK;
}
UNCURSED_ANDWINDOWVDEF(int, standend) {
    wattrset(win, A_NORMAL);
    return OK;
}

UNCURSED_ANDWINDOWDEF(int, attr_get, attr_t* attr; uncursed_color* pairnum;
                      void *unused, attr, pairnum, unused) {
    (void) unused;
    *attr = win->current_attr;
    *pairnum = PAIR_NUMBER(win->current_attr);
    return OK;
}
UNCURSED_ANDWINDOWDEF(int, attr_off, attr_t attr; void *unused, attr, unused) {
    (void) unused;
    return wattroff(win, attr);
}
UNCURSED_ANDWINDOWDEF(int, attr_on, attr_t attr; void *unused, attr, unused) {
    (void) unused;
    return wattron(win, attr);
}
UNCURSED_ANDWINDOWDEF(int, attr_set, attr_t attr; void *unused, attr, unused) {
    (void) unused;
    return wattrset(win, attr);
}
UNCURSED_ANDMVWINDOWDEF(int, chgat, int len; attr_t attr;
                        uncursed_color pairnum; const void *unused,
                        len, attr, pairnum, unused) {
    (void) unused;
    int x = win->x;
    while (len) {
        win->chararray[win->y*win->stride+x].attr = attr | COLOR_PAIR(pairnum);
        len--; x++;
        if (x > win->maxx) break;
    }
    return OK;
}

/* manual page 3ncurses add_wch */
int TABSIZE = 8; /* externally visible */
UNCURSED_ANDMVWINDOWDEF(int, add_wch, const cchar_t *ch, ch) {
    if (ch->chars[0] == 8) { // backspace
        if (win->x > 0) win->x--;
    } else if (ch->chars[0] == 9) { // tab
        win->x += TABSIZE - (win->x % TABSIZE);
        if (win->x > win->maxx) win->x = win->maxx;
    } else if (ch->chars[0] == 10) {
        wclrtoeol(win);
        win->y++;
        if (win->y > win->maxy) {
            scroll(win);
            win->y--;
        }
        win->x = 0;
    } else if (ch->chars[0] < 32 || /* nonprintable characters */
               (ch->chars[0] >= 127 && ch->chars[0] < 160)) {
        if (waddch(win, ch->attr | '^') == ERR) return ERR;
        return waddch(win, ch->attr | (ch->chars[0] + 64));
    } else {
        /* TODO: Detect whether ch contains only combining and zero-width
           characters, and combine them into the current character rather
           than replacing the current character with them, as well as not
           moving the cursor. (This is curses behaviour, but is a little
           perverse with respect to cursor motion; it'd make more sense
           to combine into the previous character.) */
        memcpy(win->chararray + win->y*win->stride+win->x, ch, sizeof *ch);
        win->chararray[win->y*win->stride+win->x].attr |= win->current_attr;
        win->x++;
        if (win->x > win->maxx) {win->x = 0; win->y++;}
        /* Nothing in the documentation implies that we need to scroll in this
           situation... */
        if (win->y > win->maxy) {win->y--;}
    }
    return OK;
}
UNCURSED_ANDWINDOWDEF(int, echo_wchar, const cchar_t *ch, ch) {
    if (wadd_wch(win, ch) == ERR) return ERR;
    return wrefresh(win);
}

static const cchar_t WACS[] = {
    {0, {0x25ae, 0}}, {0, {0x2592, 0}}, {0, {0x2534, 0}}, {0, {0x00b7, 0}},
    {0, {0x2592, 0}}, {0, {0x2193, 0}}, {0, {0x00b0, 0}}, {0, {0x25c6, 0}},
    {0, {0x2265, 0}}, {0, {0x2500, 0}}, {0, {0x2603, 0}}, {0, {0x2190, 0}},
    {0, {0x2264, 0}}, {0, {0x2514, 0}}, {0, {0x2518, 0}}, {0, {0x2524, 0}},
    {0, {0x2260, 0}}, {0, {0x03c0, 0}}, {0, {0x00b1, 0}}, {0, {0x253c, 0}},
    {0, {0x2192, 0}}, {0, {0x261c, 0}}, {0, {0x23ba, 0}}, {0, {0x23bb, 0}},
    {0, {0x23bc, 0}}, {0, {0x23bd, 0}}, {0, {0x00a3, 0}}, {0, {0x252c, 0}},
    {0, {0x2192, 0}}, {0, {0x250c, 0}}, {0, {0x2510, 0}}, {0, {0x2502, 0}}};
const cchar_t* WACS_BLOCK    = WACS+0;
const cchar_t* WACS_BOARD    = WACS+1;
const cchar_t* WACS_BTEE     = WACS+2;
const cchar_t* WACS_BULLET   = WACS+3;
const cchar_t* WACS_CKBOARD  = WACS+4;
const cchar_t* WACS_DARROW   = WACS+5;
const cchar_t* WACS_DEGREE   = WACS+6;
const cchar_t* WACS_DIAMOND  = WACS+7;
const cchar_t* WACS_GEQUAL   = WACS+8;
const cchar_t* WACS_HLINE    = WACS+9;
const cchar_t* WACS_LANTERN  = WACS+10;
const cchar_t* WACS_LARROW   = WACS+11;
const cchar_t* WACS_LEQUAL   = WACS+12;
const cchar_t* WACS_LLCORNER = WACS+13;
const cchar_t* WACS_LRCORNER = WACS+14;
const cchar_t* WACS_LTEE     = WACS+15;
const cchar_t* WACS_NEQUAL   = WACS+16;
const cchar_t* WACS_PI       = WACS+17;
const cchar_t* WACS_PLMINUS  = WACS+18;
const cchar_t* WACS_PLUS     = WACS+19;
const cchar_t* WACS_RARROW   = WACS+20;
const cchar_t* WACS_RTEE     = WACS+21;
const cchar_t* WACS_S1       = WACS+22;
const cchar_t* WACS_S3       = WACS+23;
const cchar_t* WACS_S7       = WACS+24;
const cchar_t* WACS_S9       = WACS+25;
const cchar_t* WACS_STERLING = WACS+26;
const cchar_t* WACS_TTEE     = WACS+27;
const cchar_t* WACS_UARROW   = WACS+28;
const cchar_t* WACS_ULCORNER = WACS+29;
const cchar_t* WACS_URCORNER = WACS+30;
const cchar_t* WACS_VLINE    = WACS+31;

static const cchar_t WACS_T[] = {
    {0, {0x250f, 0}}, {0, {0x2517, 0}}, {0, {0x2513, 0}}, {0, {0x251b, 0}},
    {0, {0x252b, 0}}, {0, {0x2523, 0}}, {0, {0x253b, 0}}, {0, {0x2533, 0}},
    {0, {0x2501, 0}}, {0, {0x2503, 0}}, {0, {0x254b, 0}}};
const cchar_t* WACS_T_ULCORNER = WACS_T+0;
const cchar_t* WACS_T_LLCORNER = WACS_T+1;
const cchar_t* WACS_T_URCORNER = WACS_T+2;
const cchar_t* WACS_T_LRCORNER = WACS_T+3;
const cchar_t* WACS_T_LTEE     = WACS_T+4;
const cchar_t* WACS_T_RTEE     = WACS_T+5;
const cchar_t* WACS_T_BTEE     = WACS_T+6;
const cchar_t* WACS_T_TTEE     = WACS_T+7;
const cchar_t* WACS_T_HLINE    = WACS_T+8;
const cchar_t* WACS_T_VLINE    = WACS_T+9;
const cchar_t* WACS_T_PLUS     = WACS_T+10;

static const cchar_t WACS_D[] = {
    {0, {0x2554, 0}}, {0, {0x255a, 0}}, {0, {0x2557, 0}}, {0, {0x255d, 0}},
    {0, {0x2563, 0}}, {0, {0x2560, 0}}, {0, {0x2569, 0}}, {0, {0x2566, 0}},
    {0, {0x2550, 0}}, {0, {0x2551, 0}}, {0, {0x256c, 0}}};
const cchar_t* WACS_D_ULCORNER = WACS_D+0;
const cchar_t* WACS_D_LLCORNER = WACS_D+1;
const cchar_t* WACS_D_URCORNER = WACS_D+2;
const cchar_t* WACS_D_LRCORNER = WACS_D+3;
const cchar_t* WACS_D_LTEE     = WACS_D+4;
const cchar_t* WACS_D_RTEE     = WACS_D+5;
const cchar_t* WACS_D_BTEE     = WACS_D+6;
const cchar_t* WACS_D_TTEE     = WACS_D+7;
const cchar_t* WACS_D_HLINE    = WACS_D+8;
const cchar_t* WACS_D_VLINE    = WACS_D+9;
const cchar_t* WACS_D_PLUS     = WACS_D+10;

/* manual page 3ncurses add_wchstr */
UNCURSED_ANDMVWINDOWDEF(int, add_wchstr, const cchar_t *charray, charray) {
    int n = 0;
    while (charray[n].chars[0] != 0 && n != CCHARW_MAX) n++;
    return wadd_wchnstr(win, charray, n);
}
UNCURSED_ANDMVWINDOWDEF(int, add_wchnstr, const cchar_t *charray; int n,
                        charray, n) {
    if (n > win->maxx - win->x + 1) n = win->maxx - win->x + 1;
    memcpy(win->chararray + win->y*win->stride+win->x, charray,
           n * sizeof *charray);
    return OK;
}

/* manual page 3ncurses addch */
static wchar_t cp437[] = { /* codepage 437 character table */
/* First 128 chars are the same as ASCII */
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
/* Next 128 chars are IBM extended */
0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
};

UNCURSED_ANDMVWINDOWDEF(int, addch, const chtype ch, ch) {
    cchar_t cchar = {win->current_attr | (ch & ~(A_CHARTEXT)),
                     {cp437[ch & A_CHARTEXT], 0} };
    return wadd_wch(win, &cchar);
}
UNCURSED_ANDWINDOWDEF(int, echochar, const chtype ch, ch) {
    cchar_t cchar = {win->current_attr | (ch & ~(A_CHARTEXT)),
                     {cp437[ch & A_CHARTEXT], 0} };
    return wecho_wchar(win, &cchar);
}

/* manual page 3ncurses addchstr */
UNCURSED_ANDMVWINDOWDEF(int, addchstr, const chtype *charray, charray) {
    int n = 0;
    while (charray[n]) n++;
    return waddchnstr(win, charray, n);
}
UNCURSED_ANDMVWINDOWDEF(int, addchnstr, const chtype *charray; int n,
                        charray, n) {
    if (n > win->maxx - win->x + 1) n = win->maxx - win->x + 1;
    cchar_t *p = win->chararray + win->y*win->stride + win->x;
    int i;
    for (i = 0; i < n; i++) {
        p->attr = charray[i] & ~(A_CHARTEXT);
        p->chars[0] = cp437[charray[i] & A_CHARTEXT];
        p->chars[1] = 0;
        p++;
    }
    return OK;
}

/* manual page 3ncurses addstr */
UNCURSED_ANDMVWINDOWDEF(int, addstr, const char *s, s) {
    while (*s) if (waddch(win, *s++) == ERR) return ERR;
    return OK;
}
UNCURSED_ANDMVWINDOWDEF(int, addnstr, const char *s; int n, s, n) {
    while (*s && n-- != 0) {
        if (waddch(win, *s++) == ERR) return ERR;
        /* Negative n means write until the end of the line. */
        if (n < 0 && win->x == 0) return OK;
    }
    return OK;
}

/* manual page 3ncurses addwstr */
UNCURSED_ANDMVWINDOWDEF(int, addwstr, const wchar_t *s, s) {
    while (*s) {
        cchar_t c = {0, {*s++, 0}};
        if (wadd_wch(win, &c) == ERR) return ERR;
    }
    return OK;
}
UNCURSED_ANDMVWINDOWDEF(int, addwnstr, const wchar_t *s; int n, s, n) {
    while (*s && n-- != 0) {
        cchar_t c = {0, {*s++, 0}};
        if (wadd_wch(win, &c) == ERR) return ERR;
        /* Negative n means write until the end of the line. */
        if (n < 0 && win->x == 0) return OK;
    }
    return OK;
}

/* manual page 3ncurses default_colors */
int use_default_colors(void) {
    assume_default_colors(-1, -1);
    return OK;
}
/* No, I don't know why these are ints either. */
int assume_default_colors(int fgcolor, int bgcolor) {
    init_pair(0, fgcolor, bgcolor);
    return OK;
}

/* manual page 3ncurses beep */
static WINDOW *nout_win = 0; /* Window drawn onto by wnoutrefresh */
int beep(void) {
    uncursed_hook_beep();
    return OK;
}
int flash(void) {
    /* Invert colors on the entire screen. */
    int i;
    for (i = 0; i < pair_content_alloc_count; i++) {
        uncursed_color t = pair_content_list[i][0];
        pair_content_list[i][0] = pair_content_list[i][1];
        pair_content_list[i][1] = t;
    }
    /* Redraw the entire screen. */
    touchwin(nout_win);
    doupdate();
    uncursed_hook_delay(500);
    /* Now put it back to the way it was. */
    for (i = 0; i < pair_content_alloc_count; i++) {
        uncursed_color t = pair_content_list[i][0];
        pair_content_list[i][0] = pair_content_list[i][1];
        pair_content_list[i][1] = t;
    }
    touchwin(nout_win);
    doupdate();
    return OK;
}

/* manual page 3ncurses border */
UNCURSED_ANDWINDOWDEF(int, border, chtype ls; chtype rs; chtype ts; chtype bs;
                      chtype tl; chtype tr; chtype bl; chtype br,
                      ls, rs, ts, bs, tl, tr, bl, br) {
    if (ls == 0) ls = ACS_VLINE;
    if (rs == 0) rs = ACS_VLINE;
    if (ts == 0) ts = ACS_HLINE;
    if (bs == 0) bs = ACS_HLINE;
    if (tl == 0) tl = ACS_ULCORNER;
    if (tr == 0) tr = ACS_URCORNER;
    if (bl == 0) bl = ACS_LLCORNER;
    if (br == 0) br = ACS_LRCORNER;
    int sx = win->x;
    int sy = win->y;
    int i;
    for (i = 1; i < win->maxx; i++) {
        mvwaddch(win, 0, i, ts);
        mvwaddch(win, win->maxy, i, bs);
    }
    for (i = 1; i < win->maxx; i++) {
        mvwaddch(win, i, 0, ls);
        mvwaddch(win, i, win->maxx, rs);
    }
    mvwaddch(win, 0, 0, tl);
    mvwaddch(win, 0, win->maxx, tr);
    mvwaddch(win, win->maxy, 0, bl);
    mvwaddch(win, win->maxy, win->maxx, br);
    win->x = sx;
    win->y = sy;
    return OK;
}
int box(WINDOW *win, chtype verch, chtype horch) {
    return wborder(win, verch, verch, horch, horch, 0, 0, 0, 0);
}
UNCURSED_ANDMVWINDOWDEF(int, hline, chtype ch; int n, ch, n) {
    /* We'd go into an infinite loop if someone tried to draw a line of
       cursor motion commands... */
    if (ch == 8 || ch == 9 || ch == 10) return ERR;
    int sx = win->x;
    int sy = win->y;
    while (n > 0) {
        if (win->x == win->maxx) n = 1;
        waddch(win, ch);
        n--;
    }
    win->x = sx;
    win->y = sy;
    return OK;    
}
UNCURSED_ANDMVWINDOWDEF(int, vline, chtype ch; int n, ch, n) {
    if (ch == 8 || ch == 9 || ch == 10) return ERR;
    int sx = win->x;
    int sy = win->y;
    while (n > 0) {
        if (win->x == win->maxx) n = 1;
        waddch(win, ch);
        wmove(win, win->x-1, win->y+1);
        n--;
    }
    win->x = sx;
    win->y = sy;
    return OK;
}

/* manual page 3ncurses border_set */
UNCURSED_ANDWINDOWDEF(int, border_set, const cchar_t * ls; const cchar_t * rs;
                      const cchar_t * ts; const cchar_t * bs; const cchar_t * tl;
                      const cchar_t * tr; const cchar_t * bl; const cchar_t * br,
                      ls, rs, ts, bs, tl, tr, bl, br) {
    if (ls == 0) ls = WACS_VLINE;
    if (rs == 0) rs = WACS_VLINE;
    if (ts == 0) ts = WACS_HLINE;
    if (bs == 0) bs = WACS_HLINE;
    if (tl == 0) tl = WACS_ULCORNER;
    if (tr == 0) tr = WACS_URCORNER;
    if (bl == 0) bl = WACS_LLCORNER;
    if (br == 0) br = WACS_LRCORNER;
    int sx = win->x;
    int sy = win->y;
    int i;
    for (i = 1; i < win->maxx; i++) {
        mvwadd_wch(win, 0, i, ts);
        mvwadd_wch(win, win->maxy, i, bs);
    }
    for (i = 1; i < win->maxx; i++) {
        mvwadd_wch(win, i, 0, ls);
        mvwadd_wch(win, i, win->maxx, rs);
    }
    mvwadd_wch(win, 0, 0, tl);
    mvwadd_wch(win, 0, win->maxx, tr);
    mvwadd_wch(win, win->maxy, 0, bl);
    mvwadd_wch(win, win->maxy, win->maxx, br);
    win->x = sx;
    win->y = sy;
    return OK;
}
int box_set(WINDOW *win, const cchar_t * verch, const cchar_t * horch) {
    return wborder_set(win, verch, verch, horch, horch, 0, 0, 0, 0);
}
UNCURSED_ANDMVWINDOWDEF(int, hline_set, const cchar_t * ch; int n, ch, n) {
    /* We'd go into an infinite loop if someone tried to draw a line of
       cursor motion commands... */
    if (ch->chars[0] == 8 || ch->chars[0] == 9 || ch->chars[0] == 10)
        return ERR;
    int sx = win->x;
    int sy = win->y;
    while (n > 0) {
        if (win->x == win->maxx) n = 1;
        wadd_wch(win, ch);
        n--;
    }
    win->x = sx;
    win->y = sy;
    return OK;    
}
UNCURSED_ANDMVWINDOWDEF(int, vline_set, const cchar_t * ch; int n, ch, n) {
    if (ch->chars[0] == 8 || ch->chars[0] == 9 || ch->chars[0] == 10)
        return ERR;
    int sx = win->x;
    int sy = win->y;
    while (n > 0) {
        if (win->x == win->maxx) n = 1;
        wadd_wch(win, ch);
        wmove(win, win->x-1, win->y+1);
        n--;
    }
    win->x = sx;
    win->y = sy;
    return OK;
}

/* manual page 3ncurses inopts */
int cbreak(void) { return noraw(); }
int nocbreak(void) { timeout(-1); return OK; }
int noecho(void) { return OK; }
int halfdelay(int d) { timeout(d * 100); return OK; }
int intrflush(WINDOW *win, uncursed_bool b) { (void)win; (void)b; return OK; }
int keypad   (WINDOW *win, uncursed_bool b) { (void)win; (void)b; return OK; }
int meta     (WINDOW *win, uncursed_bool b) { (void)win; (void)b; return OK; }
int nodelay  (WINDOW *win, uncursed_bool b) {
    wtimeout(win, b ? 0 : -1); return OK;
}
int raw(void)   { uncursed_hook_rawsignals(1); return OK; }
int noraw(void) { uncursed_hook_rawsignals(0); return OK; }
int qiflush(void) { return OK; }
int noqiflush(void) { return OK; }
int notimeout (WINDOW *win, uncursed_bool b) { (void)win; (void)b; return OK; }
void timeout(int t) { wtimeout(stdscr, t); }
void wtimeout(WINDOW *win, int t) { win->timeout = t; }
int typeahead(int fd) { (void)fd; return OK; }

/* manual page 3ncurses overlay */
int overlay(const WINDOW *from, const WINDOW *to) {
    return copywin(from, to, 0, 0, 0, 0, min(from->maxy, to->maxy),
                   min(from->maxx, to->maxx), 1);
}
int overwrite(const WINDOW *from, const WINDOW *to) {
    return copywin(from, to, 0, 0, 0, 0, min(from->maxy, to->maxy),
                   min(from->maxx, to->maxx), 0);
}
int copywin(const WINDOW *from, const WINDOW *to,
            int from_minx, int from_miny, int to_minx, int to_miny,
            int to_maxx, int to_maxy, int skip_blanks) {
    int i, j;
    for (i = to_minx; i <= to_maxx; i++) {
        for (j = to_miny; j <= to_maxy; j++) {
            cchar_t *f = from->chararray + i + j * from->stride;
            cchar_t *t = to->chararray + i - to_minx + from_minx +
                (j - to_miny + from_miny) * to->stride;
            if (skip_blanks && f->chars[0] == 32) continue;
            *t = *f;
        }
    }
    return OK;
}


/* manual page 3ncurses clear */
UNCURSED_ANDWINDOWVDEF(int, erase) {
    wmove(win, 0, 0);
    return wclrtobot(win);
}
UNCURSED_ANDWINDOWVDEF(int, clear) {
    werase(win);
    return clearok(win, 1);
}
UNCURSED_ANDWINDOWVDEF(int, clrtobot) {
    wclrtoeol(win);
    int j, i;
    for (j = win->y+1; j < win->maxy; j++) {
        for (i = 0; i < win->maxx; i++) {
            win->chararray[i + j*win->stride].attr = win->current_attr;
            win->chararray[i + j*win->stride].chars[0] = 32;
            win->chararray[i + j*win->stride].chars[1] = 0;
        }
    }
    return OK;
}
UNCURSED_ANDWINDOWVDEF(int, clrtoeol) {
    int maxpos = win->maxx + win->y * win->stride;
    int curpos = win->x + win->y * win->stride;
    while (curpos < maxpos) {
        win->chararray[curpos].attr = win->current_attr;
        win->chararray[curpos].chars[0] = 32;
        win->chararray[curpos].chars[1] = 0;
        curpos++;
    }
    return OK;
}

/* manual page 3ncurses outopts */
int clearok(WINDOW *win, uncursed_bool clear_on_refresh) {
    win->clear_on_refresh = clear_on_refresh;
    return OK;
}
int nonl(void) { return OK; }
int leaveok(WINDOW *win, uncursed_bool dont_restore_cursor) {
    (void) win;
    (void) dont_restore_cursor;
    return OK;
}

/* manual page 3ncurses kernel */
int curs_set(int vis) { uncursed_hook_setcursorsize(vis); return OK; }

/* manual page 3ncurses util */
char *unctrl(char d) {
    int c = d;
    if (d < 0) d += 256;
    static char s[5] = {'M', '-'};
    char *r = s+2;
    if (c > 127) { c -= 128; r = s; }
    if (c == 127) { c = '?'; r = s; }
    if (c < 32) { s[2] = '^'; s[3] = c+64; s[4] = 0; }
    else {s[2] = c; s[3] = 0;}
    return r;
}
wchar_t *wunctrl(wchar_t c) {
    static wchar_t s[5] = {(wchar_t)'M', (wchar_t)'-'};
    wchar_t *r = s+2;
    if (c > 127 && c < 160) { c -= 128; r = s; }
    if (c == 127) { c = '?'; r = s; }
    if (c < 32) { s[2] = '^'; s[3] = c+64; s[4] = 0; }
    else {s[2] = c; s[3] = 0;}
    return r;
}
char *keyname(int c) {
    if (c < 256) return unctrl(c);
    /* We have three types of special keys:
       - Cursor motion / numeric keypad: ESC [ letter or ESC O letter
         (Modified: ESC [ 1 ; modifier letter or ESC O 1 ; modifier letter)
       - General function keys: ESC [ number ~
         (Modified: ESC [ number ; modifier ~)
       - F1-F5 can send other codes, such as ESC [ [ letter
       The letters can be both uppercase and lowercase. (Lowercase letters
       are used for the numeric keypad by some terminals.)

       We use the integer as a bitfield:
       256     always true (to make the code >= 256)
       512     true for cursor motion/numpad
       1024    true for Linux console F1-F5
       2048 up the modifier seen minus 1 (0 for no modifier)
       1       the number or letter seen

       Based on the codes normally sent, a modifier of shift sets the 2048s bit,
       of alt sets the 4096s bit, of control sets the 8192s it. Some codes won't
       be sent by certain terminals, and some will overlap. One problem we have
       is that there are some keys that are the same but have different codes on
       different terminals (e.g. home/end), and some keys that are conflated by
       some terminals but not others (e.g. numpad home versus main keyboard
       home). In order to give stable KEY_x definitions, we translate ^[OH and
       ^[OF (home/end on xterm) to ^[[1~ and ^[[4~ (home/end on Linux console),
       ^[[15~ (F5 on xterm) to ^[[[E (F5 on Linux console), and ^[[E (num5 on
       xterm) and ^[[G (num6 on Linux console outside application mode) to ^[Ou
       (num5 on Linux console inside application mode). There's also one clash
       between ESC [ P (Pause/Break), and ESC O P (PF1); we resolve this by
       translating ESC [ P as ESC [ [ P.

       keyname's job is to undo all this, and return a sensible name for the
       key that's pressed. Unlike curses keyname, it will construct a name for
       any keypress.
    */
    static char keybuf[80] = "KEY_";
    if (c & KEY_CTRL)  strcat(keybuf, "CTRL_");
    if (c & KEY_ALT)   strcat(keybuf, "ALT_");
    if (c & KEY_SHIFT) strcat(keybuf, "SHIFT_");
    c &= ~(KEY_CTRL | KEY_ALT | KEY_SHIFT);
    switch(c) {
#define KEYCHECK(x) case KEY_##x: strcat(keybuf, #x); break
        KEYCHECK(HOME); KEYCHECK(IC); KEYCHECK(DC); KEYCHECK(END);
        KEYCHECK(PPAGE); KEYCHECK(NPAGE);
        KEYCHECK(UP); KEYCHECK(DOWN); KEYCHECK(RIGHT); KEYCHECK(LEFT);
        KEYCHECK(BREAK); KEYCHECK(BTAB);
        KEYCHECK(F1); KEYCHECK(F2); KEYCHECK(F3); KEYCHECK(F4); KEYCHECK(F5);
        KEYCHECK(F6); KEYCHECK(F7); KEYCHECK(F8); KEYCHECK(F9); KEYCHECK(F10);
        KEYCHECK(F11); KEYCHECK(F12); KEYCHECK(F13); KEYCHECK(F14);
        KEYCHECK(F15); KEYCHECK(F16); KEYCHECK(F17); KEYCHECK(F18);
        KEYCHECK(F19); KEYCHECK(F20);
        KEYCHECK(PF1); KEYCHECK(PF2); KEYCHECK(PF3); KEYCHECK(PF4);
        KEYCHECK(A1); KEYCHECK(A2); KEYCHECK(A3); KEYCHECK(A4);
        KEYCHECK(B1); KEYCHECK(B2); KEYCHECK(B3);
        KEYCHECK(C1); KEYCHECK(C2); KEYCHECK(C3);
        KEYCHECK(D1); KEYCHECK(D3);
        KEYCHECK(BACKSPACE); KEYCHECK(ESCAPE);
        KEYCHECK(MOUSE); KEYCHECK(RESIZE); KEYCHECK(PRINT);
    default: return NULL;
    }
    return keybuf;
}
char *key_name(wchar_t c) {
    /* For some reason, this returns a narrow string not a wide string, and as
       such, we can't return wide characters at all, so we just return NULL.
       (TODO: add a wide string version for uncursed). Wide character key codes
       are like narrow character key codes, but 0x10ff00 higher to allow for
       the hugely greater number of codepoints. */
    if (c >= 0x110000) return keyname(c - 0x10ff00);
    if (c < 256) return unctrl(c);
    return 0;
}
int delay_output(int ms) { uncursed_hook_delay(ms); return OK; }

/* manual page 3ncurses delch */
UNCURSED_ANDMVWINDOWVDEF(int, delch) {
    memmove(win->chararray + win->x + win->y*win->stride,
            win->chararray + win->x + win->y*win->stride+1,
            (win->maxx - win->x) * sizeof *(win->chararray));
    win->chararray[win->maxx + win->y*win->stride].attr =
        win->current_attr;
    win->chararray[win->maxx + win->y*win->stride].chars[0] = 32;
    win->chararray[win->maxx + win->y*win->stride].chars[1] = 0;
    return OK;
}

/* manual page 3ncurses deleteln */
UNCURSED_ANDWINDOWVDEF(int, deleteln) {
    return winsdelln(win, -1);
}
UNCURSED_ANDWINDOWVDEF(int, insertln) {
    return winsdelln(win, 1);
}
UNCURSED_ANDWINDOWDEF(int, insdelln, int n, n) {
    uncursed_bool inserting = 1;
    if (n < 0) inserting = 0;
    if (n == 0) return OK; /* avoid memcpy(x,x), it's undefined behaviour */
    int j, i;
    for (j = (inserting ? win->maxy : win->y);
         j >= win->y && j <= win->maxy;
         j += (inserting ? -1 : 1)) {
        if (j + n >= win->y && j+n <= win->maxy)
            memcpy(win->chararray + j*win->stride,
                   win->chararray + (j+n)*win->stride,
                   win->maxx * sizeof *(win->chararray));
        else for (i = 0; i < win->maxx; i++) {
                win->chararray[i + j*win->stride].attr = win->current_attr;
                win->chararray[i + j*win->stride].chars[0] = 32;
                win->chararray[i + j*win->stride].chars[1] = 0;
            }
    }
    return OK;
}

/* manual page 3ncurses initscr */
WINDOW *stdscr = 0;
static WINDOW *save_stdscr = 0;
static WINDOW *disp_win = 0; /* WINDOW drawn onto by doupdate */
int LINES, COLUMNS;

WINDOW *initscr(void) {
    if (save_stdscr || stdscr) return 0;
    uncursed_hook_init(&LINES, &COLUMNS);
    nout_win = newwin(0, 0, 0, 0);
    if (!nout_win) return 0;
    disp_win = newwin(0, 0, 0, 0);
    if (!disp_win) {free(nout_win); return 0;}
    stdscr = newwin(0, 0, 0, 0);
    if (!stdscr) {free(nout_win); free(disp_win); return 0;}
    return stdscr;
}
int endwin(void) {
    save_stdscr = stdscr;
    stdscr = 0;
    uncursed_hook_exit();
    return touchwin(stdscr);
}
uncursed_bool isendwin(void) {
    return !stdscr;
}

/* manual page 3ncurses window */
WINDOW *newwin(int h, int w, int t, int r) {
    WINDOW *win = malloc(sizeof (WINDOW));
    if (!win) return 0;
    win->chararray = malloc(w*h * sizeof *(win->chararray));
    if (!win->chararray) return 0;
    win->current_attr = 0;
    win->y = win->x = 0;
    win->maxx = w-1;
    win->maxy = h-1;
    win->stride = w; /* no reason to use any other packing scheme */
    win->scry = t;
    win->scrx = r;
    win->parent = 0;
    win->childcount = 0;
    win->timeout = -1; /* input in this WINDOW is initially blocking */
    win->clear_on_refresh = 0;
    werase(win);
    return win;
}
WINDOW *subwin(WINDOW *parent, int h, int w, int t, int r) {
    WINDOW *win = malloc(sizeof (WINDOW));
    if (!win) return 0;
    parent->childcount++;
    win->parent = parent;
    win->chararray = parent->chararray;
    win->current_attr = 0;
    win->y = win->x = 0;
    win->maxx = w-1;
    win->maxy = h-1;
    win->stride = parent->stride;
    win->scry = t;
    win->scrx = r;
    win->timeout = -1;
    win->clear_on_refresh = 0;
    return win;
}
WINDOW *derwin(WINDOW *parent, int h, int w, int t, int r) {
    WINDOW *rv = subwin(parent, h, w, t+parent->scry, r+parent->scrx);
    if (rv) mvderwin(rv, t, r);
    return rv;
}

int delwin(WINDOW *win) {
    if (!win) return ERR;
    if (win->childcount) return ERR;
    if (win->parent) win->parent->childcount--;
    else free(win->chararray);
    free(win);
    return OK;
}

int mvwin(WINDOW *win, int y, int x) {
    if (win->maxy + y >= LINES || y < 0) return ERR;
    if (win->maxx + x >= COLUMNS || x < 0) return ERR;
    win->scry = y;
    win->scrx = x;
    return OK;
}
int mvderwin(WINDOW *win, int y, int x) {
    win->chararray = win->parent->chararray + x + y * (win->parent->stride);
    return OK;
}

/* Synch routines are mostly no-ops because touchwin is also a no-op */
void wsyncup(WINDOW *win) { (void)win; }
void wsyncdown(WINDOW *win) { (void)win; }
int syncok(WINDOW *win, uncursed_bool sync) { (void)win; (void)sync; return OK; }
/* but this one isn't */
void wcursyncup(WINDOW *win) {
    if (!win->parent) return;
    win->parent->x = win->x +
        (win->chararray - win->parent->chararray) % (win->parent->stride);
    win->parent->y = win->y +
        (win->chararray - win->parent->chararray) / (win->parent->stride);
    wcursyncup(win->parent);
}

/* manual page 3ncurses refresh */
UNCURSED_ANDWINDOWVDEF(int, refresh) {
    wnoutrefresh(win);
    return doupdate();
}
int redrawwin(WINDOW *win) {
    touchwin(win);
    return wrefresh(win);
}
int wredrawln(WINDOW *win, int first, int num) {
    touchline(win, first, num);
    return wrefresh(win);
}
int wnoutrefresh(WINDOW *win) {
    if (win->clear_on_refresh) nout_win->clear_on_refresh = 1;
    win->clear_on_refresh = 0;
    copywin(win, nout_win, 0, 0, win->scry, win->scrx,
            win->maxy, win->maxx, 0);
    return wmove(nout_win, win->scry+win->y, win->scrx+win->x);
}
int doupdate(void) {
    int i, j;
    if (nout_win->clear_on_refresh) {
        werase(disp_win);
        uncursed_hook_fullredraw();
    }
    nout_win->clear_on_refresh = 0;
    cchar_t *p = nout_win->chararray;
    cchar_t *q = disp_win->chararray;
    for (i = 0; i <= nout_win->maxx; i++) {
        for (j = 0; j <= nout_win->maxy; j++) {
            if (p->attr != q->attr)
                uncursed_hook_update(j, i);
            int k;
            for (k = 0; k < CCHARW_MAX; k++) {
                if (p->chars[k] != q->chars[k])
                    uncursed_hook_update(j, i);
                if (p->chars[k] == 0) break;
            }
            p++; q++;
        }
    }
    uncursed_hook_positioncursor(nout_win->y, nout_win->x);
    return OK;
}
void uncursed_rhook_updated(int y, int x) {
    disp_win->chararray[x + y * disp_win->stride] =
        nout_win->chararray[x + y * nout_win->stride];
}

int uncursed_rhook_color_at(int y, int x) {
    attr_t a = disp_win->chararray[x + y * disp_win->stride].attr;
    int p = PAIR_NUMBER(a);
    uncursed_color f, b;
    pair_content(p, &f, &b);
    /* Many attributes are simulated with color. */
    if (a & A_REVERSE) { int t = f; f = b; b = t; }
    /* For portability, we have bright implies bold, bold implies bright.
       The implementation libraries know this, so we just send the
       brightness. */
    if (a & A_BOLD)    { f |= 8; }
    if (a & A_INVIS)   { f = b; }
    if (f == -1) f = 16;
    if (b == -1) b = 16;

    return f | (b << 5) | (!!(a & A_UNDERLINE) << 10);
}
char uncursed_rhook_cp437_at(int y, int x) {
    wchar_t wc = disp_win->chararray[x + y * disp_win->stride].chars[0];
    int i;
    for (i = 0; i < 256; i++)
        if (cp437[i] == wc) return i;
    return 0xa8; /* an upside-down question mark */
}
char *uncursed_rhook_utf8_at(int y, int x) {
    wchar_t *c = disp_win->chararray[x + y * disp_win->stride].chars;
    /* The maximum number of UTF-8 bytes for one codepoint is 4. */
    static char utf8[CCHARW_MAX * 4 + 1];
    char *r = utf8;
    int itercount = 0;
    while (*c != 0) {
        if (*c < 0x80) *r++ = *c;
        else if (*c < 0x800) {
            *r++ = 0xc0 | ((*c >> 6) & 0x3f);
            *r++ = 0x80 | ((*c >> 0) & 0x3f);
        } else if (*c < 0x10000) {
            *r++ = 0xe0 | ((*c >> 12) & 0x3f);
            *r++ = 0x80 | ((*c >> 6) & 0x3f);
            *r++ = 0x80 | ((*c >> 0) & 0x3f);
        } else if (*c < 0x110000) {
            *r++ = 0xe0 | ((*c >> 18) & 0x3f);
            *r++ = 0x80 | ((*c >> 12) & 0x3f);
            *r++ = 0x80 | ((*c >> 6) & 0x3f);
            *r++ = 0x80 | ((*c >> 0) & 0x3f);
        } else { // out of Unicode range
            /* This is the encoding for REPLACEMENT CHARACTER. */
            *r++ = 0xef; *r++ = 0xbf; *r++ = 0xbd;
        }
        itercount++;
        if (itercount == CCHARW_MAX) break;
    }
    *r = 0;
    return utf8;
}

/* manual page 3ncurses get_wch */
static wchar_t pushback_w = 0x110000;
int unget_wch(wchar_t c) {
    if (pushback_w < 0x110000) return ERR;
    pushback_w = c;
    return OK;
}
UNCURSED_ANDMVWINDOWDEF(int, get_wch, wint_t *rv, rv) {
    if (pushback_w < 0x110000) {
        *rv = pushback_w;
        pushback_w = 0x110000;
        return OK;
    }
    *rv = uncursed_hook_getkeyorcodepoint(win->timeout);
    /* When we have multiple possible key codes for certain keys, pick one
       and merge them together. */
    if (*rv >= 0x110000) {
        *rv -= 0x10ff00;
        int mods = *rv & (KEY_SHIFT | KEY_ALT | KEY_CTRL);
        *rv &= ~mods;
        if (*rv == (KEY_KEYPAD | 'H')) *rv = KEY_HOME;
        if (*rv == (KEY_KEYPAD | 'F')) *rv = KEY_END;
        if (*rv == (KEY_FUNCTION | 15)) *rv = KEY_F5;
        if (*rv == (KEY_KEYPAD | 'E')) *rv = KEY_B2;
        if (*rv == (KEY_KEYPAD | 'G')) *rv = KEY_B2;
        *rv |= mods;
        *rv += 0x10ff00;
    }
    return OK;
}

/* manual page 3ncurses getcchar */
int getcchar(const cchar_t *c, wchar_t *s, attr_t *attr,
             short *pairnum, void *unused) {
    (void) unused;
    int ccount = 0;
    while (c->chars[ccount] != 0 && ccount != CCHARW_MAX) ccount++;
    if (!s) return ccount;
    s[ccount] = 0;
    while(ccount--) s[ccount] = c->chars[ccount];
    *attr = c->attr;
    *pairnum = PAIR_NUMBER(c->attr);
    return OK;
}
int setcchar(cchar_t *c, const wchar_t *s, attr_t attr,
             short pairnum, void *unused) {
    (void) unused;
    int ccount = 0;
    while (s[ccount] != 0 && ccount != CCHARW_MAX)
        c->chars[ccount] = s[ccount];
    c->attr = attr & ~(COLOR_PAIR(PAIR_NUMBER(attr)));
    c->attr |= COLOR_PAIR(pairnum);
    return OK;
}

/* manual page 3ncurses getch */
UNCURSED_ANDMVWINDOWVDEF(int, getch) {
    wint_t w;
    wrefresh(win);
    if (wget_wch(win, &w) == ERR) return ERR;
    if (w >= 0x110000) return w - 0x10ff00; /* keypress */
    if (w < 128) return w;
    int i;
    for (i = 128; i < 256; i++) {
        if ((wint_t)cp437[i] == w) return i;
    }
    return 0xa8; /* an upside-down question mark */
}

/* manual page 3ncurses move */
UNCURSED_ANDWINDOWDEF(int, move, int y; int x, y, x) {
    win->y = y;
    win->x = x;
    return OK;
}

/* manual page 3ncurses touch */
int touchwin(WINDOW *win) {(void)win; return OK;}
int untouchwin(WINDOW *win) {(void)win; return OK;}
int touchline(WINDOW *win, int first, int count) {
    (void)win; (void)first; (void)count;
    return OK;
}
int wtouchln(WINDOW *win, int first, int count, int touched) {
    (void)win; (void)first; (void)count; (void)touched;
    return OK;
}

/* manual page 3ncurses printw */
int printw(const char *fmt, ...) {
    int rv; va_list vl; va_start(vl, fmt);
    rv = vw_printw(stdscr, fmt, vl);
    va_end(vl); return rv;
}
int wprintw(WINDOW *win, const char *fmt, ...) {
    int rv; va_list vl; va_start(vl, fmt);
    rv = vw_printw(win, fmt, vl);
    va_end(vl); return rv;
}
int mvprintw(int y, int x, const char *fmt, ...) {
    int rv; va_list vl; va_start(vl, fmt);
    move(y, x);
    rv = vw_printw(stdscr, fmt, vl);
    va_end(vl); return rv;
}
int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...) {
    int rv; va_list vl; va_start(vl, fmt);
    wmove(win, y, x);
    rv = vw_printw(win, fmt, vl);
    va_end(vl); return rv;
}
int vw_printw(WINDOW *win, const char *fmt, va_list vl) {
    va_list vl2; va_copy(vl2, vl);
    char *bf = malloc(5);
    if (!bf) return ERR;
    /* Count the length of the string */
    int ccount = vsnprintf(bf, 5, fmt, vl);
    bf = realloc(bf, ccount+1);
    if (!bf) return ERR;
    vsnprintf(bf, 5, fmt, vl2);
    char *r = bf;
    while (*r) waddch(win, *r++);
    free(bf);
    return OK;
}

/* manual page 3ncurses scroll */
int scroll(WINDOW *win) { return wscrl(win, 1); }
UNCURSED_ANDWINDOWDEF(int, scrl, int n, n) {
    int y = win->y;
    win->y = 0;
    winsdelln(win, n);
    win->y = y;
    return OK;
}

/* manual page 3ncurses wresize */
int wresize(WINDOW *win, int newh, int neww) {
    if (win->childcount) return ERR; /* should we try to implement this? */
    if (win->parent)     return ERR; /* or this? this one's easier */
    WINDOW *temp = newwin(newh, neww, win->scry, win->scrx);
    if (!temp) return ERR;
    overwrite(win, temp);
    free(win->chararray);
    *win = *temp;
    free(temp);
    return OK;
}
