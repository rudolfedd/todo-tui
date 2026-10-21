/* curses_compat.h — include the right curses flavour for the platform. */
#ifndef TODO_CURSES_COMPAT_H
#define TODO_CURSES_COMPAT_H

#if defined(_WIN32)
   /* PDCurses (MSYS2 / mingw-w64): <pdcurses.h> pulls in pdcurses/curses.h */
#  define PDCURSES 1
#  include <pdcurses.h>
#else
#  define TODO_NCURSES 1
#  include <curses.h>
#endif

#endif /* TODO_CURSES_COMPAT_H */
