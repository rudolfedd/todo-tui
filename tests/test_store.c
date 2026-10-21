/* test_store.c — round trip tests for the JSON backed task storage. */
#include "json.h"
#include "model.h"
#include "store.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static const char *TMP_PATH = "test_store_tmp.json";

static void make_task(TaskList *l, const char *title, const char *notes,
                       const char *tags, const char *due, Priority p, int done)
{
    Task t;
    memset(&t, 0, sizeof t);
    t.id        = l->next_id++;
    t.title     = (char *)title;
    t.notes     = (char *)notes;
    t.tags      = (char *)tags;
    t.due       = (char *)due;
    t.prio      = p;
    t.done      = done;
    t.created   = 1700000000;
    t.completed = done ? 1700000500 : 0;
    tasklist_append(l, &t);
}

static void test_roundtrip(void)
{
    TaskList a;
    tasklist_init(&a);
    make_task(&a, "Buy milk", "2.5%", "home, errands", "2026-10-24", PRIO_HIGH, 0);
    make_task(&a, "Call mum", "", "family", "", PRIO_LOW, 1);
    make_task(&a, "Quote \"this\"", "line1\nline2", "work", "2026-10-01", PRIO_MED, 0);

    char err[256];
    CHECK(store_save(TMP_PATH, &a, err, sizeof err) == 0);

    TaskList b;
    tasklist_init(&b);
    CHECK(store_load(TMP_PATH, &b, err, sizeof err) == 0);

    CHECK(b.count == a.count);
    if (b.count == 3) {
        CHECK(!strcmp(b.items[0].title, "Buy milk"));
        CHECK(!strcmp(b.items[0].notes, "2.5%"));
        CHECK(!strcmp(b.items[0].tags, "home, errands"));
        CHECK(!strcmp(b.items[0].due, "2026-10-24"));
        CHECK(b.items[0].prio == PRIO_HIGH);
        CHECK(b.items[0].done == 0);
        CHECK(b.items[0].completed == 0);

        CHECK(b.items[1].done == 1);
        CHECK(b.items[1].completed == 1700000500);
        CHECK(b.items[1].prio == PRIO_LOW);

        CHECK(!strcmp(b.items[2].title, "Quote \"this\""));
        CHECK(!strcmp(b.items[2].notes, "line1\nline2"));
        CHECK(b.items[2].prio == PRIO_MED);
    }
    CHECK(b.next_id == a.next_id);

    tasklist_free(&a);
    tasklist_free(&b);
    remove(TMP_PATH);
}

static void test_missing_file(void)
{
    TaskList l;
    tasklist_init(&l);
    char err[256];
    CHECK(store_load("definitely_not_here_12345.json", &l, err, sizeof err) == 0);
    CHECK(l.count == 0);
    tasklist_free(&l);
}

static void test_broken_file(void)
{
    FILE *f = fopen("test_store_broken.json", "wb");
    CHECK(f != NULL);
    if (f) {
        fputs("{ this is not json ", f);
        fclose(f);
    }
    TaskList l;
    tasklist_init(&l);
    char err[256];
    CHECK(store_load("test_store_broken.json", &l, err, sizeof err) != 0);
    CHECK(err[0] != '\0');
    tasklist_free(&l);
    remove("test_store_broken.json");
}

static void test_legacy_shape(void)
{
    /* a file written by hand: tags as a string, priority as a number */
    FILE *f = fopen("test_store_legacy.json", "wb");
    CHECK(f != NULL);
    if (f) {
        fputs("{\"version\":1,\"next_id\":5,\"tasks\":["
              "{\"id\":3,\"title\":\"legacy\",\"done\":1,\"priority\":2,"
              "\"tags\":\"one,two\",\"due\":null}]}", f);
        fclose(f);
    }
    TaskList l;
    tasklist_init(&l);
    char err[256];
    CHECK(store_load("test_store_legacy.json", &l, err, sizeof err) == 0);
    CHECK(l.count == 1);
    if (l.count == 1) {
        CHECK(!strcmp(l.items[0].title, "legacy"));
        CHECK(l.items[0].done == 1);
        CHECK(l.items[0].prio == PRIO_HIGH);
        CHECK(!strcmp(l.items[0].tags, "one, two"));
        CHECK(!strcmp(l.items[0].due, ""));
    }
    tasklist_free(&l);
    remove("test_store_legacy.json");
}

int main(void)
{
    test_roundtrip();
    test_missing_file();
    test_broken_file();
    test_legacy_shape();

    if (failures) {
        printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
