/* editor.h — a small single-line text editor with a moving cursor. */
#ifndef TODO_EDITOR_H
#define TODO_EDITOR_H

#include <stddef.h>

typedef struct {
    char  *buf;    /* NUL-terminated UTF-8 text */
    size_t len;    /* bytes in use */
    size_t cap;    /* allocated bytes */
    size_t pos;    /* cursor offset in bytes */
} Editor;

void ed_init(Editor *e);
void ed_free(Editor *e);
void ed_set(Editor *e, const char *text);
void ed_clear(Editor *e);
const char *ed_text(const Editor *e);

void ed_insert(Editor *e, const char *utf8);
void ed_backspace(Editor *e);
void ed_delete(Editor *e);
void ed_left(Editor *e);
void ed_right(Editor *e);
void ed_home(Editor *e);
void ed_end(Editor *e);
void ed_delete_word(Editor *e);
void ed_kill_to_start(Editor *e);
void ed_kill_to_end(Editor *e);

/* Cursor column (in codepoints) and the visible text of the line. */
size_t ed_col(const Editor *e);

#endif /* TODO_EDITOR_H */
