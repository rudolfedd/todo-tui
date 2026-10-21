/* util.h — tiny helpers: allocation, dynamic strings, UTF-8 utilities. */
#ifndef TODO_UTIL_H
#define TODO_UTIL_H

#include <stddef.h>

/* Allocation helpers — abort on OOM (a todo app cannot do without memory). */
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
void  xfree(void *p);

/* Dynamic byte buffer. */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} Buf;

void buf_init(Buf *b);
void buf_free(Buf *b);
void buf_reserve(Buf *b, size_t extra);
void buf_append(Buf *b, const char *data, size_t n);
void buf_puts(Buf *b, const char *s);
void buf_putc(Buf *b, char c);
void buf_printf(Buf *b, const char *fmt, ...);
char *buf_release(Buf *b); /* detach the buffer, caller owns it */

/* Case-insensitive helpers (locale independent, ASCII + Latin-1 fold). */
int  ci_eq(const char *a, const char *b);
int  ci_cmp(const char *a, const char *b);
const char *ci_strstr(const char *hay, const char *needle);
char *ci_strdup(const char *s);

/* UTF-8 helpers. */
size_t utf8_len(const char *s);            /* number of codepoints */
size_t utf8_char_len(unsigned char c);    /* bytes of the codepoint starting at c */
char  *utf8_truncate(const char *s, size_t max_chars); /* NULL-terminated copy, cut on a char boundary */

/* Misc. */
long long now_epoch(void);
char *today_string(void);      /* "YYYY-MM-DD" in local time, caller frees */
char *format_date(long long t);/* "YYYY-MM-DD" in local time, caller frees */
int   date_is_past(const char *iso);   /* due date earlier than today */
int   date_is_today(const char *iso);
void  str_strip(char *s);             /* in-place trim of spaces */
void  str_lower(char *s);             /* in-place ASCII tolower */

/* Split a comma/space separated list into trimmed, non-empty parts. */
size_t split_list(const char *s, char ***out); /* caller frees array and items */

#endif /* TODO_UTIL_H */
