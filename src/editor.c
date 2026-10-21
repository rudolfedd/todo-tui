/* editor.c — single line editor with UTF-8 aware cursor movement. */
#include "editor.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

void ed_init(Editor *e)
{
    e->buf = xmalloc(16);
    e->buf[0] = '\0';
    e->len = 0;
    e->cap = 16;
    e->pos = 0;
}

void ed_free(Editor *e)
{
    free(e->buf);
    e->buf = NULL;
    e->len = e->cap = e->pos = 0;
}

void ed_set(Editor *e, const char *text)
{
    if (!text) text = "";
    size_t n = strlen(text);
    if (n + 1 > e->cap) {
        e->cap = n + 16;
        e->buf = xrealloc(e->buf, e->cap);
    }
    memcpy(e->buf, text, n + 1);
    e->len = n;
    e->pos = n;
}

void ed_clear(Editor *e)
{
    e->buf[0] = '\0';
    e->len = 0;
    e->pos = 0;
}

const char *ed_text(const Editor *e)
{
    return e->buf ? e->buf : "";
}

static void ed_reserve(Editor *e, size_t extra)
{
    if (e->len + extra + 1 > e->cap) {
        size_t want = e->cap ? e->cap : 16;
        while (want < e->len + extra + 1) want *= 2;
        e->buf = xrealloc(e->buf, want);
        e->cap = want;
    }
}

void ed_insert(Editor *e, const char *utf8)
{
    if (!utf8 || !*utf8) return;
    size_t n = strlen(utf8);
    ed_reserve(e, n);
    memmove(e->buf + e->pos + n, e->buf + e->pos, e->len - e->pos);
    memcpy(e->buf + e->pos, utf8, n);
    e->len += n;
    e->pos += n;
    e->buf[e->len] = '\0';
}

/* Byte offset of the previous codepoint boundary before `pos`. */
static size_t prev_char(const Editor *e, size_t pos)
{
    if (pos == 0) return 0;
    size_t p = pos - 1;
    while (p > 0 && ((unsigned char)e->buf[p] & 0xC0) == 0x80) p--;
    return p;
}

/* Byte offset of the next codepoint boundary after `pos`. */
static size_t next_char(const Editor *e, size_t pos)
{
    if (pos >= e->len) return e->len;
    size_t p = pos + 1;
    while (p < e->len && ((unsigned char)e->buf[p] & 0xC0) == 0x80) p++;
    return p;
}

void ed_backspace(Editor *e)
{
    if (e->pos == 0) return;
    size_t start = prev_char(e, e->pos);
    memmove(e->buf + start, e->buf + e->pos, e->len - e->pos + 1);
    e->len -= (e->pos - start);
    e->pos = start;
}

void ed_delete(Editor *e)
{
    if (e->pos >= e->len) return;
    size_t end = next_char(e, e->pos);
    memmove(e->buf + e->pos, e->buf + end, e->len - end + 1);
    e->len -= (end - e->pos);
}

void ed_left(Editor *e)
{
    e->pos = prev_char(e, e->pos);
}

void ed_right(Editor *e)
{
    e->pos = next_char(e, e->pos);
}

void ed_home(Editor *e) { e->pos = 0; }
void ed_end(Editor *e)  { e->pos = e->len; }

void ed_delete_word(Editor *e)
{
    size_t p = e->pos;
    while (p > 0 && e->buf[p - 1] == ' ') p = prev_char(e, p);
    while (p > 0 && e->buf[p - 1] != ' ') p = prev_char(e, p);
    if (p == e->pos) return;
    memmove(e->buf + p, e->buf + e->pos, e->len - e->pos + 1);
    e->len -= (e->pos - p);
    e->pos = p;
}

void ed_kill_to_start(Editor *e)
{
    if (e->pos == 0) return;
    memmove(e->buf, e->buf + e->pos, e->len - e->pos + 1);
    e->len -= e->pos;
    e->pos = 0;
}

void ed_kill_to_end(Editor *e)
{
    e->buf[e->pos] = '\0';
    e->len = e->pos;
}

size_t ed_col(const Editor *e)
{
    size_t n = 0;
    for (size_t p = 0; p < e->pos && p < e->len; p += utf8_char_len((unsigned char)e->buf[p])) n++;
    return n;
}
