/* PDCurses */

#include "pdcsdl.h"

#include <stdlib.h>
#include <string.h>

#ifdef PDC_WIDE
   #define USE_UNICODE_ACS_CHARS 1
#else
   #define USE_UNICODE_ACS_CHARS 0
#endif

#include "../common/acs_defs.h"
#include "../common/pdccolor.h"

#define MAXRECT 200     /* maximum number of rects to queue up before
                           an update is forced; the number was chosen
                           arbitrarily */

static SDL_Rect uprect[MAXRECT];       /* table of rects to update */
static chtype oldch = (chtype)(-1);    /* current attribute */
static int rectcount = 0;              /* index into uprect */
static int foregr = -2, backgr = -2;   /* current foreground, background */
static bool blinked_off = FALSE;

/* do the real updates on a delay */

void PDC_update_rects(void)
{
    if (rectcount)
    {
        /* if the maximum number of rects has been reached, we're
           probably better off doing a full screen update */

        if (rectcount == MAXRECT)
            SDL_Flip(pdc_screen);
        else
            SDL_UpdateRects(pdc_screen, rectcount, uprect);

        rectcount = 0;
    }
}

static SDL_Color *get_pdc_color( const int color_idx)
{
    static SDL_Color c;
    const PACKED_RGB rgb = PDC_get_palette_entry( color_idx > 0 ? color_idx : 0);

    c.r = (Uint8)Get_RValue( rgb);
    c.g = (Uint8)Get_GValue( rgb);
    c.b = (Uint8)Get_BValue( rgb);
    return( &c);
}

static Uint32 get_pdc_mapped( const int color_idx)
{
   SDL_Color *c = get_pdc_color( color_idx);

   return( SDL_MapRGB( pdc_screen->format, c->r, c->g, c->b));
}

/* set the font colors to match the chtype's attribute */

static void _set_attr(chtype ch)
{
    attr_t sysattrs = SP->termattrs;

#ifdef PDC_WIDE
    TTF_SetFontStyle(pdc_ttffont,
        ( ((ch & A_BOLD) && (sysattrs & A_BOLD)) ?
            TTF_STYLE_BOLD : 0) |
        ( ((ch & A_ITALIC) && (sysattrs & A_ITALIC)) ?
            TTF_STYLE_ITALIC : 0) );
#endif

    ch &= (A_COLOR|A_BOLD|A_BLINK|A_REVERSE);

    if (oldch != ch)
    {
        int newfg, newbg;

        if (SP->mono)
            return;

        extended_pair_content(PAIR_NUMBER(ch), &newfg, &newbg);

        if ((ch & A_BOLD) && !(sysattrs & A_BOLD))
            newfg |= 8;
        if ((ch & A_BLINK) && !(sysattrs & A_BLINK))
            newbg |= 8;

        if (ch & A_REVERSE)
        {
            int tmp = newfg;
            newfg = newbg;
            newbg = tmp;
        }

        if (newfg != foregr)
        {
#ifndef PDC_WIDE
            SDL_SetPalette(pdc_font, SDL_LOGPAL,
                           get_pdc_color( newfg), pdc_flastc, 1);
#endif
            foregr = newfg;
        }

        if (newbg != backgr)
        {
#ifndef PDC_WIDE
            if (newbg == -1)
                SDL_SetColorKey(pdc_font, SDL_SRCCOLORKEY, 0);
            else
            {
                if (backgr == -1)
                    SDL_SetColorKey(pdc_font, 0, 0);

                SDL_SetPalette(pdc_font, SDL_LOGPAL,
                               get_pdc_color( newbg), 0, 1);
            }
#endif
            backgr = newbg;
        }

        oldch = ch;
    }
}

#ifdef PDC_WIDE
/* Draw some of the ACS_* "graphics" */

#define BIT_UP       1
#define BIT_DN       2
#define BIT_RT       4
#define BIT_LT       8
#define HORIZ   (BIT_LT | BIT_RT)
#define VERTIC  (BIT_UP | BIT_DN)
         /* Macros used for deciphering ACS_S1, ACS_S3, ACS_S7, ACS_S9     */
#define Sn_CHARS      0x10
#define SCAN_LINE( n)      (Sn_CHARS | ((n - 1) << 8))

static bool _grprint(chtype ch, const SDL_Rect dest)
{
    int i = 0;
    bool rval;
    const int remap_tbl[] = {
            ACS_ULCORNER, BIT_DN | BIT_RT,   ACS_LLCORNER, BIT_UP | BIT_RT,
            ACS_URCORNER, BIT_DN | BIT_LT,   ACS_LRCORNER, BIT_UP | BIT_LT,
            ACS_LTEE, VERTIC | BIT_RT,       ACS_RTEE, VERTIC | BIT_LT,
            ACS_TTEE, HORIZ | BIT_DN,        ACS_BTEE, HORIZ | BIT_UP,
            ACS_HLINE, HORIZ,                ACS_VLINE, VERTIC,
            ACS_PLUS, HORIZ | VERTIC,        ACS_BLOCK, 0,
            ACS_S1, HORIZ | SCAN_LINE( 1),     ACS_S3, HORIZ | SCAN_LINE( 3),
            ACS_S7, HORIZ | SCAN_LINE( 7),     ACS_S9, HORIZ | SCAN_LINE( 9),    0 };

    while( remap_tbl[i] && remap_tbl[i] != (int)ch)
        i += 2;
    if( remap_tbl[i] == (int)ch)
    {
        const int hmid = (pdc_fheight - pdc_fthick) >> 1;
        const int wmid = (pdc_fwidth - pdc_fthick) >> 1;
        const int mask = remap_tbl[i + 1];
        SDL_Rect temp = dest;
        const Uint32 col = get_pdc_mapped( foregr);

        if( ch == ACS_BLOCK)
            SDL_FillRect(pdc_screen, &temp, col);
        if( mask & HORIZ)
        {
            temp.h = pdc_fthick;
            if( mask & Sn_CHARS)    /* extract scan line for ACS_Sn characters */
                temp.y += (mask >> 8) * hmid >> 2;
            else
                temp.y += hmid;
            switch( mask & HORIZ)
            {
                case BIT_RT:
                    temp.x += wmid;
                    temp.w -= wmid;
                    break;
                case BIT_LT:
                    temp.w = wmid + pdc_fthick;
                    break;
                case HORIZ:
                    break;
            }
            SDL_FillRect(pdc_screen, &temp, col);
        }
        temp = dest;
        if( mask & VERTIC)
        {
            temp.x += wmid;
            temp.w = pdc_fthick;
            switch( mask & VERTIC)
            {
                case BIT_DN:
                    temp.y += hmid;
                    temp.h -= hmid;
                    break;
                case BIT_UP:
                    temp.h = hmid + pdc_fthick;
                    break;
                case VERTIC:
                    break;
            }
            SDL_FillRect(pdc_screen, &temp, col);
        }
        rval = TRUE;
    }
    else
        rval = FALSE;   /* didn't draw it -- fall back to acs_map */
    return( rval);
}
#endif

/* draw a cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    SDL_Rect src, dest;
    chtype ch;
    int oldrow, oldcol;
#ifdef PDC_WIDE
    Uint16 chstr[2] = {0, 0};
#endif

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d from row %d col %d\n",
             row, col, SP->cursrow, SP->curscol));

    oldrow = SP->cursrow;
    oldcol = SP->curscol;

    /* clear the old cursor */

    PDC_transform_line(oldrow, oldcol, 1, curscr->_y[oldrow] + oldcol);

    if (!SP->visibility)
        return;

    /* draw a new cursor by overprinting the existing character in
       reverse, either the full cell (when visibility == 2) or the
       lowest quarter of it (when visibility == 1) */

    ch = curscr->_y[row][col] ^ A_REVERSE;

    _set_attr(ch);

    src.h = (SP->visibility == 1) ? pdc_fheight >> 2 : pdc_fheight;
    src.w = pdc_fwidth;

    dest.y = (row + 1) * pdc_fheight - src.h + pdc_yoffset;
    dest.x = col * pdc_fwidth + pdc_xoffset;
    dest.h = src.h;
    dest.w = src.w;

#ifdef PDC_WIDE
    SDL_FillRect(pdc_screen, &dest, get_pdc_mapped( backgr));

    if (!(SP->visibility == 2 && _is_altcharset( ch) &&
        _grprint(ch & (0x7f | A_ALTCHARSET), dest)))
    {
        if( _is_altcharset( ch))
            ch = acs_map[ch & 0x7f];

        chstr[0] = (Uint16)( ch & A_CHARTEXT);

        switch (pdc_sdl_render_mode)
        {
        case PDC_SDL_RENDER_SOLID:
            pdc_font = TTF_RenderUNICODE_Solid(pdc_ttffont, chstr,
                                               *(get_pdc_color( foregr)));
            break;
        case PDC_SDL_RENDER_SHADED:
            pdc_font = TTF_RenderUNICODE_Shaded(pdc_ttffont, chstr,
                                               *(get_pdc_color( foregr)),
                                               *(get_pdc_color( backgr)));
            break;
        default:
            pdc_font = TTF_RenderUNICODE_Blended(pdc_ttffont, chstr,
                                               *(get_pdc_color( foregr)));
        }

        if (pdc_font)
        {
            int center = pdc_fwidth > pdc_font->w ?
                        (pdc_fwidth - pdc_font->w) >> 1 : 0;
            src.x = 0;
            src.y = pdc_fheight - src.h;
            dest.x += center;
            SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
            dest.x -= center;
            SDL_FreeSurface(pdc_font);
            pdc_font = NULL;
        }
    }
#else
    if( _is_altcharset( ch))
        ch = acs_map[ch & 0x7f];

    src.x = (ch & 0xff) % 32 * pdc_fwidth;
    src.y = (ch & 0xff) / 32 * pdc_fheight + (pdc_fheight - src.h);

    SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
#endif

    if (oldrow != row || oldcol != col)
    {
        if (rectcount == MAXRECT)
            PDC_update_rects();

        uprect[rectcount++] = dest;
    }
}

static bool _merge_rects( SDL_Rect *a, const SDL_Rect *b)
{
    if( a->x == b->x && a->w == b->w)
    {
        const int ay = a->y + a->h;
        const int by = b->y + b->h;

        if( ay >= b->y && by >= a->y)
        {
            const int y1 = min( a->y, b->y);
            const int y2 = max( ay, by);

            a->y = y1;
            a->h = y2 - y1;
            return TRUE;
        }
    }
    if( a->y == b->y && a->h == b->h)
    {
        const int ax = a->x + a->w;
        const int bx = b->x + b->w;

        if( ax >= b->x && bx >= a->x)
        {
            const int x1 = min( a->x, b->x);
            const int x2 = max( ax, bx);

            a->x = x1;
            a->w = x2 - x1;
            return TRUE;
        }
    }
    return FALSE;
}

void _new_packet(attr_t attr, int lineno, int x, int len, const chtype *srcp)
{
    SDL_Rect src, dest;
    int j;
#ifdef PDC_WIDE
    Uint16 chstr[2] = {0, 0};
#endif
    attr_t sysattrs = SP->termattrs;
    int hcol = SP->line_color;
    bool blink = blinked_off && (attr & A_BLINK) && (sysattrs & A_BLINK);

    if (rectcount == MAXRECT)
        PDC_update_rects();

#ifdef PDC_WIDE
    src.x = 0;
    src.y = 0;
#endif
    src.h = pdc_fheight;
    src.w = pdc_fwidth;

    dest.y = pdc_fheight * lineno + pdc_yoffset;
    dest.x = pdc_fwidth * x + pdc_xoffset;
    dest.h = pdc_fheight;
    dest.w = pdc_fwidth * len;
    uprect[rectcount++] = dest;

    /* we try merging the new rectangle with the previous one.  It's
       possible the result can be merged with the one before that,
       and so on.  */

    while( rectcount >= 2 && _merge_rects( uprect + rectcount - 2,
                                           uprect + rectcount - 1))
       rectcount--;

    _set_attr(attr);

    if (backgr == -1)
        SDL_LowerBlit(pdc_tileback, &dest, pdc_screen, &dest);
#ifdef PDC_WIDE
    else
        SDL_FillRect(pdc_screen, &dest, get_pdc_mapped( backgr));
#endif

    if (hcol == -1)
        hcol = (short)foregr;

    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];

        if (blink)
            ch = ' ';

        dest.w = pdc_fwidth;

        if( _is_altcharset( ch))
        {
#ifdef PDC_WIDE
            if (_grprint(ch & (0x7f | A_ALTCHARSET), dest))
            {
                dest.x += pdc_fwidth;
                continue;
            }
#endif
            ch = acs_map[ch & 0x7f];
        }

#ifdef PDC_WIDE
        ch &= A_CHARTEXT;

        if (ch != ' ')
        {
            if (chstr[0] != ch)
            {
                chstr[0] = (Uint16)ch;

                if (pdc_font)
                    SDL_FreeSurface(pdc_font);

                switch (pdc_sdl_render_mode)
                {
                case PDC_SDL_RENDER_SOLID:
                    pdc_font = TTF_RenderUNICODE_Solid(pdc_ttffont, chstr,
                                                       *(get_pdc_color(foregr)));
                    break;
                case PDC_SDL_RENDER_SHADED:
                    pdc_font = TTF_RenderUNICODE_Shaded(pdc_ttffont, chstr,
                                                       *(get_pdc_color(foregr)),
                                                       *(get_pdc_color(backgr)));
                    break;
                default:
                    pdc_font = TTF_RenderUNICODE_Blended(pdc_ttffont, chstr,
                                                       *(get_pdc_color(foregr)));
                }
            }

            if (pdc_font)
            {
                int center = pdc_fwidth > pdc_font->w ?
                    (pdc_fwidth - pdc_font->w) >> 1 : 0;
                dest.x += center;
                SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
                dest.x -= center;
            }
        }
#else
        src.x = (ch & 0xff) % 32 * pdc_fwidth;
        src.y = (ch & 0xff) / 32 * pdc_fheight;

        SDL_LowerBlit(pdc_font, &src, pdc_screen, &dest);
#endif

        if (!blink && (attr & (A_LEFT | A_RIGHT)))
        {
            dest.w = pdc_fthick;

            if (attr & A_LEFT)
                SDL_FillRect(pdc_screen, &dest, get_pdc_mapped( hcol));

            if (attr & A_RIGHT)
            {
                dest.x += pdc_fwidth - pdc_fthick;
                SDL_FillRect(pdc_screen, &dest, get_pdc_mapped( hcol));
                dest.x -= pdc_fwidth - pdc_fthick;
            }
        }

        dest.x += pdc_fwidth;
    }

#ifdef PDC_WIDE
    if (pdc_font)
    {
        SDL_FreeSurface(pdc_font);
        pdc_font = NULL;
    }
#endif

    if (!blink && (attr & (A_UNDERLINE | A_OVERLINE | A_STRIKEOUT)))
    {
        dest.x = pdc_fwidth * x + pdc_xoffset;
        dest.h = pdc_fthick;
        dest.w = pdc_fwidth * len;
        if( attr & A_OVERLINE)
           SDL_FillRect(pdc_screen, &dest, get_pdc_mapped( hcol));
        if( attr & A_UNDERLINE)
        {
           dest.y += pdc_fheight - pdc_fthick;
           SDL_FillRect(pdc_screen, &dest, get_pdc_mapped( hcol));
           dest.y -= pdc_fheight - pdc_fthick;
        }
        if( attr & A_STRIKEOUT)
        {
           dest.y += (pdc_fheight - pdc_fthick) / 2;
           SDL_FillRect(pdc_screen, &dest, get_pdc_mapped(hcol));
        }
    }
}

/* update the given physical line to look like the corresponding line in
   curscr */

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    attr_t old_attr, attr;
    int i, j;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

    old_attr = *srcp & (A_ATTRIBUTES ^ A_ALTCHARSET);

    for (i = 1, j = 1; j < len; i++, j++)
    {
        attr = srcp[i] & (A_ATTRIBUTES ^ A_ALTCHARSET);

        if (attr != old_attr)
        {
            _new_packet(old_attr, lineno, x, i, srcp);
            old_attr = attr;
            srcp += i;
            x += i;
            i = 0;
        }
    }

    _new_packet(old_attr, lineno, x, i, srcp);
}

static Uint32 _blink_timer(Uint32 interval, void *param)
{
    SDL_Event event;

    INTENTIONALLY_UNUSED_PARAMETER( param);
    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);
    return(interval);
}

void PDC_blink_text(void)
{
    static SDL_TimerID blinker_id = 0;
    int i, j, k;

    oldch = (chtype)(-1);

    if (!(SP->termattrs & A_BLINK))
    {
        SDL_RemoveTimer(blinker_id);
        blinker_id = 0;
    }
    else if (!blinker_id)
    {
        blinker_id = SDL_AddTimer(500, _blink_timer, NULL);
        blinked_off = TRUE;
    }

    blinked_off = !blinked_off;

    for (i = 0; i < SP->lines; i++)
    {
        const chtype *srcp = curscr->_y[i];

        for (j = 0; j < SP->cols; j++)
            if (srcp[j] & A_BLINK)
            {
                k = j;
                while (k < SP->cols && (srcp[k] & A_BLINK))
                    k++;
                PDC_transform_line(i, j, k - j, srcp + j);
                j = k;
            }
    }

    oldch = (chtype)(-1);
}

void PDC_doupdate(void)
{
    PDC_napms(1);
}
