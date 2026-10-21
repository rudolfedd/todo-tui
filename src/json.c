/* json.c — recursive-descent JSON parser, no external dependencies. */
#include "json.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_DEPTH 200

typedef struct {
    const char *p;
    char       *err;
    size_t      errsz;
    int         failed;
    int         depth;
} P;

static void fail(P *s, const char *msg)
{
    if (s->failed) return;
    s->failed = 1;
    if (s->err && s->errsz) snprintf(s->err, s->errsz, "%s", msg);
}

static void skip_ws(P *s)
{
    while (*s->p == ' ' || *s->p == '\t' || *s->p == '\n' || *s->p == '\r') s->p++;
}

static JsonValue *parse_value(P *s);

static JsonValue *new_value(JsonType t)
{
    JsonValue *v = xcalloc(1, sizeof *v);
    v->type = t;
    return v;
}

static char *parse_string_raw(P *s)
{
    if (*s->p != '"') { fail(s, "expected string"); return NULL; }
    s->p++;
    Buf b;
    buf_init(&b);
    while (*s->p && *s->p != '"') {
        unsigned char c = (unsigned char)*s->p;
        if (c == '\\') {
            s->p++;
            char e = *s->p;
            switch (e) {
            case '"':  buf_putc(&b, '"');  s->p++; break;
            case '\\': buf_putc(&b, '\\'); s->p++; break;
            case '/':  buf_putc(&b, '/');  s->p++; break;
            case 'b':  buf_putc(&b, '\b'); s->p++; break;
            case 'f':  buf_putc(&b, '\f'); s->p++; break;
            case 'n':  buf_putc(&b, '\n'); s->p++; break;
            case 'r':  buf_putc(&b, '\r'); s->p++; break;
            case 't':  buf_putc(&b, '\t'); s->p++; break;
            case 'u': {
                s->p++;
                unsigned cp = 0;
                for (int i = 0; i < 4; i++) {
                    char h = *s->p;
                    if (!isxdigit((unsigned char)h)) { fail(s, "bad \\u escape"); buf_free(&b); return NULL; }
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                    else cp |= (unsigned)(h - 'A' + 10);
                    s->p++;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF && s->p[0] == '\\' && s->p[1] == 'u') {
                    /* surrogate pair */
                    const char *save = s->p;
                    s->p += 2;
                    unsigned lo = 0;
                    int ok = 1;
                    for (int i = 0; i < 4; i++) {
                        char h = *s->p;
                        if (!isxdigit((unsigned char)h)) { ok = 0; break; }
                        lo <<= 4;
                        if (h >= '0' && h <= '9') lo |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') lo |= (unsigned)(h - 'a' + 10);
                        else lo |= (unsigned)(h - 'A' + 10);
                        s->p++;
                    }
                    if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else {
                        s->p = save;
                    }
                }
                /* UTF-8 encode */
                if (cp < 0x80) {
                    buf_putc(&b, (char)cp);
                } else if (cp < 0x800) {
                    buf_putc(&b, (char)(0xC0 | (cp >> 6)));
                    buf_putc(&b, (char)(0x80 | (cp & 0x3F)));
                } else if (cp < 0x10000) {
                    buf_putc(&b, (char)(0xE0 | (cp >> 12)));
                    buf_putc(&b, (char)(0x80 | ((cp >> 6) & 0x3F)));
                    buf_putc(&b, (char)(0x80 | (cp & 0x3F)));
                } else {
                    buf_putc(&b, (char)(0xF0 | (cp >> 18)));
                    buf_putc(&b, (char)(0x80 | ((cp >> 12) & 0x3F)));
                    buf_putc(&b, (char)(0x80 | ((cp >> 6) & 0x3F)));
                    buf_putc(&b, (char)(0x80 | (cp & 0x3F)));
                }
                break;
            }
            default:
                fail(s, "invalid escape sequence");
                buf_free(&b);
                return NULL;
            }
        } else {
            buf_putc(&b, (char)c);
            s->p++;
        }
    }
    if (*s->p != '"') { fail(s, "unterminated string"); buf_free(&b); return NULL; }
    s->p++;
    return buf_release(&b);
}

static JsonValue *parse_object(P *s)
{
    JsonValue *v = new_value(JSON_OBJECT);
    s->p++; /* '{' */
    skip_ws(s);
    if (*s->p == '}') { s->p++; return v; }

    size_t cap = 8;
    v->u.object.keys = xmalloc(cap * sizeof *v->u.object.keys);
    v->u.object.values = xmalloc(cap * sizeof *v->u.object.values);

    for (;;) {
        skip_ws(s);
        char *key = parse_string_raw(s);
        if (!key) { json_free(v); return NULL; }
        skip_ws(s);
        if (*s->p != ':') { free(key); fail(s, "expected ':'"); json_free(v); return NULL; }
        s->p++;
        JsonValue *val = parse_value(s);
        if (!val) { free(key); json_free(v); return NULL; }

        if (v->u.object.count == cap) {
            cap *= 2;
            v->u.object.keys = xrealloc(v->u.object.keys, cap * sizeof *v->u.object.keys);
            v->u.object.values = xrealloc(v->u.object.values, cap * sizeof *v->u.object.values);
        }
        v->u.object.keys[v->u.object.count] = key;
        v->u.object.values[v->u.object.count] = val;
        v->u.object.count++;

        skip_ws(s);
        if (*s->p == ',') { s->p++; continue; }
        if (*s->p == '}') { s->p++; return v; }
        fail(s, "expected ',' or '}'");
        json_free(v);
        return NULL;
    }
}

static JsonValue *parse_array(P *s)
{
    JsonValue *v = new_value(JSON_ARRAY);
    s->p++; /* '[' */
    skip_ws(s);
    if (*s->p == ']') { s->p++; return v; }

    size_t cap = 8;
    v->u.array.items = xmalloc(cap * sizeof *v->u.array.items);

    for (;;) {
        JsonValue *item = parse_value(s);
        if (!item) { json_free(v); return NULL; }
        if (v->u.array.count == cap) {
            cap *= 2;
            v->u.array.items = xrealloc(v->u.array.items, cap * sizeof *v->u.array.items);
        }
        v->u.array.items[v->u.array.count++] = item;
        skip_ws(s);
        if (*s->p == ',') { s->p++; continue; }
        if (*s->p == ']') { s->p++; return v; }
        fail(s, "expected ',' or ']'");
        json_free(v);
        return NULL;
    }
}

static JsonValue *parse_value(P *s)
{
    if (s->depth++ > JSON_MAX_DEPTH) { fail(s, "nesting too deep"); s->depth--; return NULL; }
    skip_ws(s);

    JsonValue *res = NULL;
    char c = *s->p;

    if (c == '{') {
        res = parse_object(s);
    } else if (c == '[') {
        res = parse_array(s);
    } else if (c == '"') {
        char *str = parse_string_raw(s);
        if (str) {
            res = new_value(JSON_STRING);
            res->u.string = str;
        }
    } else if (!strncmp(s->p, "true", 4)) {
        s->p += 4;
        res = new_value(JSON_BOOL);
        res->u.boolean = 1;
    } else if (!strncmp(s->p, "false", 5)) {
        s->p += 5;
        res = new_value(JSON_BOOL);
        res->u.boolean = 0;
    } else if (!strncmp(s->p, "null", 4)) {
        s->p += 4;
        res = new_value(JSON_NULL);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        char *end = NULL;
        double d = strtod(s->p, &end);
        if (end == s->p) {
            fail(s, "invalid number");
        } else {
            s->p = end;
            res = new_value(JSON_NUMBER);
            res->u.number = d;
        }
    } else {
        fail(s, "unexpected character");
    }

    s->depth--;
    return res;
}

JsonValue *json_parse(const char *text, char *err, size_t errsz)
{
    if (!text) {
        if (err && errsz) snprintf(err, errsz, "no input");
        return NULL;
    }
    P s = { text, err, errsz, 0, 0 };
    JsonValue *v = parse_value(&s);
    if (!v) return NULL;
    skip_ws(&s);
    if (*s.p) {
        fail(&s, "trailing garbage after JSON value");
        json_free(v);
        return NULL;
    }
    return v;
}

JsonValue *json_get(const JsonValue *obj, const char *key)
{
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < obj->u.object.count; i++) {
        if (strcmp(obj->u.object.keys[i], key) == 0) return obj->u.object.values[i];
    }
    return NULL;
}

const char *json_str(const JsonValue *v, const char *def)
{
    if (!v || v->type != JSON_STRING) return def;
    return v->u.string;
}

double json_num(const JsonValue *v, double def)
{
    if (!v || v->type != JSON_NUMBER) return def;
    return v->u.number;
}

int json_bool(const JsonValue *v, int def)
{
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->u.boolean;
    if (v->type == JSON_NUMBER) return v->u.number != 0.0;
    return def;
}

void json_free(JsonValue *v)
{
    if (!v) return;
    switch (v->type) {
    case JSON_STRING:
        free(v->u.string);
        break;
    case JSON_ARRAY:
        for (size_t i = 0; i < v->u.array.count; i++) json_free(v->u.array.items[i]);
        free(v->u.array.items);
        break;
    case JSON_OBJECT:
        for (size_t i = 0; i < v->u.object.count; i++) {
            free(v->u.object.keys[i]);
            json_free(v->u.object.values[i]);
        }
        free(v->u.object.keys);
        free(v->u.object.values);
        break;
    default:
        break;
    }
    free(v);
}

void json_write_string(const char *s, Buf *out)
{
    buf_putc(out, '"');
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            switch (*p) {
            case '"':  buf_puts(out, "\\\""); break;
            case '\\': buf_puts(out, "\\\\"); break;
            case '\b': buf_puts(out, "\\b"); break;
            case '\f': buf_puts(out, "\\f"); break;
            case '\n': buf_puts(out, "\\n"); break;
            case '\r': buf_puts(out, "\\r"); break;
            case '\t': buf_puts(out, "\\t"); break;
            default:
                if (*p < 0x20) buf_printf(out, "\\u%04x", *p);
                else buf_putc(out, (char)*p);
            }
        }
    }
    buf_putc(out, '"');
}
