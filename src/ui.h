/* ui.h — curses front-end: rendering and key handling. */
#ifndef TODO_UI_H
#define TODO_UI_H

#include "app.h"

typedef struct {
    int  key;       /* ASCII byte or curses KEY_* code */
    char text[8];   /* UTF-8 text to insert (empty when key is a special key) */
} KeyEvent;

/* Initialise the terminal. Returns 0 on success. */
int  ui_init(void);

/* Restore the terminal. */
void ui_shutdown(void);

/* Draw one frame. */
void ui_draw(App *a);

/* Read one key event. Returns 1 when a key was read, 0 on timeout. */
int  ui_read_key(KeyEvent *ev);

/* Process one key event. Returns 0 when the user asked to quit. */
int  ui_handle_key(App *a, const KeyEvent *ev);

#endif /* TODO_UI_H */
