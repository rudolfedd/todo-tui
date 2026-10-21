/* test_json.c — tiny self-contained test suite for the JSON parser/writer. */
#include "json.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void test_scalars(void)
{
    char err[128];
    JsonValue *v = json_parse("null", err, sizeof err);
    CHECK(v && v->type == JSON_NULL);
    json_free(v);

    v = json_parse("true", err, sizeof err);
    CHECK(v && v->type == JSON_BOOL && v->u.boolean == 1);
    json_free(v);

    v = json_parse("false", err, sizeof err);
    CHECK(v && v->type == JSON_BOOL && v->u.boolean == 0);
    json_free(v);

    v = json_parse("-12.5e2", err, sizeof err);
    CHECK(v && v->type == JSON_NUMBER);
    CHECK(v && v->u.number == -1250.0);
    json_free(v);

    v = json_parse("\"hi\"", err, sizeof err);
    CHECK(v && v->type == JSON_STRING && !strcmp(v->u.string, "hi"));
    json_free(v);
}

static void test_escapes(void)
{
    char err[128];
    JsonValue *v = json_parse("\"a\\nb\\tc\\\"d\\\\e\"", err, sizeof err);
    CHECK(v && v->type == JSON_STRING);
    CHECK(v && !strcmp(v->u.string, "a\nb\tc\"d\\e"));
    json_free(v);

    /* \u escapes, including a surrogate pair (U+1F600) */
    v = json_parse("\"\\u0410\\u00e9\\ud83d\\ude00\"", err, sizeof err);
    CHECK(v && v->type == JSON_STRING);
    CHECK(v && !strcmp(v->u.string, "\xd0\x90\xc3\xa9\xf0\x9f\x98\x80"));
    json_free(v);
}

static void test_structures(void)
{
    char err[128];
    const char *src =
        "{\"version\": 1, \"tasks\": ["
        "{\"id\": 1, \"title\": \"first\", \"done\": false, \"tags\": [\"a\",\"b\"]},"
        "{\"id\": 2, \"title\": \"second\", \"done\": true, \"priority\": \"high\"}"
        "], \"name\": null}";

    JsonValue *root = json_parse(src, err, sizeof err);
    CHECK(root != NULL);
    if (!root) { printf("  parse error: %s\n", err); return; }

    CHECK(json_get(root, "version") && json_num(json_get(root, "version"), -1) == 1);

    JsonValue *tasks = json_get(root, "tasks");
    CHECK(tasks && tasks->type == JSON_ARRAY && tasks->u.array.count == 2);

    JsonValue *t0 = tasks->u.array.items[0];
    CHECK(t0 && json_get(t0, "id") && json_num(json_get(t0, "id"), 0) == 1);
    CHECK(t0 && !strcmp(json_str(json_get(t0, "title"), ""), "first"));
    CHECK(t0 && json_bool(json_get(t0, "done"), 1) == 0);

    JsonValue *tags = json_get(t0, "tags");
    CHECK(tags && tags->type == JSON_ARRAY && tags->u.array.count == 2);
    CHECK(tags && !strcmp(tags->u.array.items[1]->u.string, "b"));

    JsonValue *t1 = tasks->u.array.items[1];
    CHECK(t1 && json_bool(json_get(t1, "done"), 0) == 1);
    CHECK(t1 && !strcmp(json_str(json_get(t1, "priority"), ""), "high"));

    CHECK(json_get(root, "name") && json_get(root, "name")->type == JSON_NULL);
    CHECK(json_get(root, "missing") == NULL);

    json_free(root);
}

static void test_errors(void)
{
    char err[128];
    CHECK(json_parse("{", err, sizeof err) == NULL);
    CHECK(json_parse("[1,2", err, sizeof err) == NULL);
    CHECK(json_parse("{\"a\":}", err, sizeof err) == NULL);
    CHECK(json_parse("\"unterminated", err, sizeof err) == NULL);
    CHECK(json_parse("1 2", err, sizeof err) == NULL);
    CHECK(json_parse("tru", err, sizeof err) == NULL);
}

static void test_writer(void)
{
    Buf b;
    buf_init(&b);
    json_write_string("a\"b\\c\nd", &b);
    CHECK(!strcmp(b.data, "\"a\\\"b\\\\c\\nd\""));
    buf_free(&b);

    buf_init(&b);
    json_write_string("\x01\x1f", &b);
    CHECK(!strcmp(b.data, "\"\\u0001\\u001f\""));
    buf_free(&b);
}

static void test_utf8(void)
{
    CHECK(utf8_len("abc") == 3);
    CHECK(utf8_len("\xd0\x90\xc3\xa9") == 2);
    char *t = utf8_truncate("\xd0\x90\xc3\xa9z", 1);
    CHECK(t && !strcmp(t, "\xd0\x90"));
    free(t);
}

static void test_split(void)
{
    char **parts = NULL;
    size_t n = split_list("a, b ;c,,d", &parts);
    CHECK(n == 4);
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);

    n = split_list("   ", &parts);
    CHECK(n == 0);
    free(parts);
}

int main(void)
{
    test_scalars();
    test_escapes();
    test_structures();
    test_errors();
    test_writer();
    test_utf8();
    test_split();

    if (failures) {
        printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
