/* util.c */
#include "util.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void oom(void)
{
    fputs("todo-tui: out of memory\n", stderr);
    exit(2);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) oom();
    return p;
}

void *xcalloc(size_t n, size_t sz)
{
    void *p = calloc(n ? n : 1, sz ? sz : 1);
    if (!p) oom();
    return p;
}

void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) oom();
    return q;
}

char *xstrdup(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *r = xmalloc(n + 1);
    memcpy(r, s, n + 1);
    return r;
}

char *xstrndup(const char *s, size_t n)
{
    if (!s) s = "";
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *r = xmalloc(len + 1);
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}

void xfree(void *p) { free(p); }

/* ---------------------------------------------------------------- Buf --- */

void buf_init(Buf *b)
{
    b->data = NULL;
    b->len = b->cap = 0;
}

void buf_free(Buf *b)
{
    free(b->data);
    buf_init(b);
}

void buf_reserve(Buf *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap) return;
    size_t want = b->cap ? b->cap : 64;
    while (want < b->len + extra + 1) want *= 2;
    b->data = xrealloc(b->data, want);
    b->cap = want;
}

void buf_append(Buf *b, const char *data, size_t n)
{
    buf_reserve(b, n);
    memcpy(b->data + b->len, data, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void buf_puts(Buf *b, const char *s)
{
    if (s) buf_append(b, s, strlen(s));
}

void buf_putc(Buf *b, char c)
{
    buf_append(b, &c, 1);
}

void buf_printf(Buf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) { va_end(ap2); return; }
    buf_reserve(b, (size_t)need);
    vsnprintf(b->data + b->len, (size_t)need + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)need;
}

char *buf_release(Buf *b)
{
    char *d = b->data ? b->data : xstrdup("");
    buf_init(b);
    return d;
}

/* ------------------------------------------------------- case folding --- */

static int fold(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

int ci_eq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (fold((unsigned char)*a) != fold((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

int ci_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        int d = fold((unsigned char)*a) - fold((unsigned char)*b);
        if (d) return d;
        a++; b++;
    }
    return fold((unsigned char)*a) - fold((unsigned char)*b);
}

const char *ci_strstr(const char *hay, const char *needle)
{
    if (!hay || !needle) return NULL;
    if (!*needle) return hay;
    size_t nl = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i] && fold((unsigned char)p[i]) == fold((unsigned char)needle[i])) i++;
        if (i == nl) return p;
    }
    return NULL;
}

char *ci_strdup(const char *s)
{
    char *r = xstrdup(s);
    str_lower(r);
    return r;
}

/* ---------------------------------------------------------------- utf8 --- */

size_t utf8_char_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; /* invalid lead byte: treat as a single byte */
}

size_t utf8_len(const char *s)
{
    if (!s) return 0;
    size_t n = 0;
    for (const char *p = s; *p; p += utf8_char_len((unsigned char)*p)) n++;
    return n;
}

char *utf8_truncate(const char *s, size_t max_chars)
{
    if (!s) return xstrdup("");
    size_t n = 0;
    const char *p = s;
    while (*p && n < max_chars) {
        p += utf8_char_len((unsigned char)*p);
        n++;
    }
    return xstrndup(s, (size_t)(p - s));
}

/* ---------------------------------------------------------------- misc --- */

long long now_epoch(void)
{
    return (long long)time(NULL);
}

/* localtime() is not thread safe, but todo-tui is single threaded and the
   alternative (localtime_r / localtime_s) differs between POSIX and Win32. */
static int local_time(time_t t, struct tm *out)
{
    struct tm *p = localtime(&t);
    if (!p) return 0;
    *out = *p;
    return 1;
}

char *format_date(long long t)
{
    time_t tt = (time_t)t;
    struct tm tmv;
    if (!local_time(tt, &tmv)) return xstrdup("");
    char out[32];
    if (!strftime(out, sizeof out, "%Y-%m-%d", &tmv)) return xstrdup("");
    return xstrdup(out);
}

char *today_string(void)
{
    return format_date(now_epoch());
}

int date_is_today(const char *iso)
{
    if (!iso || !*iso) return 0;
    char *today = today_string();
    int r = strcmp(iso, today) == 0;
    free(today);
    return r;
}

int date_is_past(const char *iso)
{
    if (!iso || !*iso) return 0;
    char *today = today_string();
    int r = strcmp(iso, today) < 0;
    free(today);
    return r;
}

void str_strip(char *s)
{
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

void str_lower(char *s)
{
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

size_t split_list(const char *s, char ***out)
{
    size_t cap = 8, n = 0;
    char **arr = xmalloc(cap * sizeof *arr);
    if (!s) { *out = arr; return 0; }

    const char *p = s;
    while (*p) {
        while (*p == ',' || *p == ';' || *p == '\n' || *p == '\t') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ',' && *p != ';' && *p != '\n' && *p != '\t') p++;
        size_t len = (size_t)(p - start);

        /* trim the surrounding blanks of every item */
        size_t b = 0;
        while (b < len && isspace((unsigned char)start[b])) b++;
        size_t e = len;
        while (e > b && isspace((unsigned char)start[e - 1])) e--;
        if (e == b) continue; /* empty item */

        if (n == cap) {
            cap *= 2;
            arr = xrealloc(arr, cap * sizeof *arr);
        }
        arr[n++] = xstrndup(start + b, e - b);
    }
    *out = arr;
    return n;
}
