/************************************************************************ 
 * This file is part of PDCurses. PDCurses is public domain software;	*
 * you may use it for any purpose. This software is provided AS IS with	*
 * NO WARRANTY whatsoever.						*
 *									*
 * If you use PDCurses in an application, an acknowledgement would be	*
 * appreciated, but is not mandatory. If you make corrections or	*
 * enhancements to PDCurses, please forward them to the current		*
 * maintainer for the benefit of other users.				*
 *									*
 * No distribution of modified PDCurses code may be made under the name	*
 * "PDCurses", except by the current maintainer. (Although PDCurses is	*
 * public domain, the name is a trademark.)				*
 *									*
 * See the file maintain.er for details of the current maintainer.	*
 ************************************************************************/

#define	CURSES_LIBRARY 1
#include <curses.h>

RCSID("$Id: inch.c,v 1.22 2006/09/24 21:22:33 wmcbrine Exp $");

/*man-start**************************************************************

  Name:                                                          inch

  Synopsis:
	chtype inch(void);
	chtype winch(WINDOW *win);
	chtype mvinch(int y, int x);
	chtype mvwinch(WINDOW *win, int y, int x);

  X/Open Description:

	NOTE: All these functions are implemented as macros.

  PDCurses Description:
	Depending upon the state of the raw character output, 7- or
	8-bit characters will be output.

  X/Open Return Value:
	All functions return OK on success and ERR on error.

  Portability				     X/Open    BSD    SYS V
					     Dec '88
	inch					Y	Y	Y
	winch					Y	Y	Y
	mvinch					Y	Y	Y
	mvwinch					Y	Y	Y

**man-end****************************************************************/

chtype winch(WINDOW *win)
{
	PDC_LOG(("winch() - called\n"));

	if (win == (WINDOW *)NULL)
		return (chtype)ERR;

	return win->_y[win->_cury][win->_curx];
}

chtype inch(void)
{
	PDC_LOG(("inch() - called\n"));

	return winch(stdscr);
}

chtype mvinch(int y, int x)
{
	PDC_LOG(("mvinch() - called\n"));

	if (move(y, x) == ERR)
		return (chtype)ERR;

	return stdscr->_y[stdscr->_cury][stdscr->_curx];
}

chtype mvwinch(WINDOW *win, int y, int x)
{
	PDC_LOG(("mvwinch() - called\n"));

	if (wmove(win, y, x) == ERR)
		return (chtype)ERR;

	return win->_y[win->_cury][win->_curx];
}

#ifdef PDC_WIDE
int win_wch(WINDOW *win, cchar_t *wcval)
{
	PDC_LOG(("win_wch() - called\n"));

	if (!win || !wcval)
		return ERR;

	*wcval = win->_y[win->_cury][win->_curx];

	return OK;
}

int in_wch(cchar_t *wcval)
{
	PDC_LOG(("in_wch() - called\n"));

	return win_wch(stdscr, wcval);
}

int mvin_wch(int y, int x, cchar_t *wcval)
{
	PDC_LOG(("mvin_wch() - called\n"));

	if (!wcval || (move(y, x) == ERR))
		return ERR;

	*wcval = stdscr->_y[stdscr->_cury][stdscr->_curx];

	return OK;
}

int mvwin_wch(WINDOW *win, int y, int x, cchar_t *wcval)
{
	PDC_LOG(("mvwin_wch() - called\n"));

	if (!wcval || (wmove(win, y, x) == ERR))
		return ERR;

	*wcval = win->_y[win->_cury][win->_curx];

	return OK;
}
#endif
