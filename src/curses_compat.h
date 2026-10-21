/* curses_compat.h — include the right curses flavour for the platform. */
#ifndef TODO_CURSES_COMPAT_H
#define TODO_CURSES_COMPAT_H

/* Pick the curses implementation:
     - Windows normally uses PDCurses (MSYS2 / mingw-w64)
     - every other platform uses ncurses
   Define TODO_USE_NCURSES to force ncurses (e.g. when both are installed). */

#if defined(TODO_USE_NCURSES)

#  define TODO_NCURSES 1
#  if defined(__has_include)
#    if __has_include(<curses.h>)
#      include <curses.h>
#    elif __has_include(<ncurses/curses.h>)
#      include <ncurses/curses.h>
#    elif __has_include(<ncursesw/curses.h>)
#      include <ncursesw/curses.h>
#    else
#      error "curses.h not found — install libncurses-dev (Debian) or ncurses-devel (Fedora)"
#    endif
#  else
#    include <curses.h>
#  endif

#elif defined(_WIN32)

   /* PDCurses (MSYS2 / mingw-w64): <pdcurses.h> pulls in pdcurses/curses.h */
#  define PDCURSES 1
#  include <pdcurses.h>

#else

#  define TODO_NCURSES 1
#  if defined(__has_include)
#    if __has_include(<curses.h>)
#      include <curses.h>
#    elif __has_include(<ncurses/curses.h>)
       /* Arch, some BSDs and MSYS2 ship the header in a subdirectory */
#      include <ncurses/curses.h>
#    elif __has_include(<ncursesw/curses.h>)
#      include <ncursesw/curses.h>
#    else
#      error "curses.h not found — install libncurses-dev (Debian) or ncurses-devel (Fedora)"
#    endif
#  else
#    include <curses.h>
#  endif

#endif

#endif /* TODO_CURSES_COMPAT_H */
