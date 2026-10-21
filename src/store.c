/* store.c — persistence in a portable, human readable JSON format. */
#include "store.h"
#include "json.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#define STORE_VERSION 1

/* ------------------------------------------------------------- paths --- */

static char *join_path(const char *dir, const char *leaf)
{
    size_t dl = strlen(dir);
    while (dl > 1 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\')) dl--;
    size_t n = dl + 1 + strlen(leaf) + 1;
    char *r = xmalloc(n);
    snprintf(r, n, "%.*s/%s", (int)dl, dir, leaf);
    return r;
}

char *store_default_path(void)
{
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (!appdata || !*appdata) appdata = getenv("USERPROFILE");
    if (!appdata || !*appdata) appdata = ".";
    char *dir = join_path(appdata, "todo-tui");
    char *file = join_path(dir, "todos.json");
    free(dir);
    return file;
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        char *dir = join_path(xdg, "todo-tui");
        char *file = join_path(dir, "todos.json");
        free(dir);
        return file;
    }
    const char *home = getenv("HOME");
    if (!home || !*home) home = ".";
    char *cfg = join_path(home, ".config");
    char *dir = join_path(cfg, "todo-tui");
    char *file = join_path(dir, "todos.json");
    free(cfg);
    free(dir);
    return file;
#endif
}

/* ------------------------------------------------------------- mkdirs --- */

static int dir_exists(const char *path)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static int make_one_dir(const char *path)
{
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL)) return 0;
    return (GetLastError() == ERROR_ALREADY_EXISTS) ? 0 : -1;
#else
    if (mkdir(path, 0700) == 0) return 0;
    return (errno == EEXIST) ? 0 : -1;
#endif
}

/* Create every missing parent directory of the file path `file_path`. */
static int make_parent_dirs(const char *file_path, char *err, size_t errsz)
{
    char *tmp = xstrdup(file_path);
    size_t n = strlen(tmp);
    for (size_t i = 0; i < n; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char save = tmp[i];
            tmp[i] = '\0';
            if (tmp[0] && !dir_exists(tmp)) {
                if (make_one_dir(tmp) != 0 && !dir_exists(tmp)) {
                    if (err && errsz) snprintf(err, errsz, "cannot create directory '%s'", tmp);
                    free(tmp);
                    return -1;
                }
            }
            tmp[i] = save;
        }
    }
    free(tmp);
    return 0;
}

int store_ensure_dir(const char *path, char *err, size_t errsz)
{
    return make_parent_dirs(path, err, errsz);
}

/* --------------------------------------------------------------- load --- */

/* Canonical form of a tag list: "a, b, c". */
static char *normalise_tags(const char *raw)
{
    char **parts = NULL;
    size_t n = split_list(raw, &parts);
    Buf b;
    buf_init(&b);
    for (size_t i = 0; i < n; i++) {
        if (i) buf_puts(&b, ", ");
        buf_puts(&b, parts[i]);
        free(parts[i]);
    }
    free(parts);
    return buf_release(&b);
}

static void task_from_json(const JsonValue *o, Task *t, TaskList *l)
{
    memset(t, 0, sizeof *t);
    t->id        = (int)json_num(json_get(o, "id"), 0);
    t->title     = xstrdup(json_str(json_get(o, "title"), ""));
    t->notes     = xstrdup(json_str(json_get(o, "notes"), ""));
    t->done      = json_bool(json_get(o, "done"), 0);
    t->created   = (long long)json_num(json_get(o, "created"), 0);
    t->completed = (long long)json_num(json_get(o, "completed"), 0);

    const JsonValue *pr = json_get(o, "priority");
    if (pr && pr->type == JSON_NUMBER) {
        int n = (int)pr->u.number;
        t->prio = (Priority)(n < 0 ? 0 : (n > 2 ? 2 : n));
    } else {
        t->prio = prio_from_name(json_str(pr, "low"));
    }

    const JsonValue *tags = json_get(o, "tags");
    if (tags && tags->type == JSON_ARRAY) {
        Buf b;
        buf_init(&b);
        for (size_t i = 0; i < tags->u.array.count; i++) {
            const JsonValue *item = tags->u.array.items[i];
            if (item->type != JSON_STRING) continue;
            if (b.len) buf_puts(&b, ", ");
            buf_puts(&b, item->u.string);
        }
        t->tags = buf_release(&b);
    } else {
        t->tags = normalise_tags(json_str(tags, ""));
    }

    const JsonValue *due = json_get(o, "due");
    t->due = xstrdup((due && due->type == JSON_STRING) ? due->u.string : "");

    if (t->id <= 0) t->id = l->next_id;
}

int store_load(const char *path, TaskList *out, char *err, size_t errsz)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (errno == ENOENT) return 0; /* fresh start */
        if (err && errsz) snprintf(err, errsz, "cannot open '%s': %s", path, strerror(errno));
        return -1;
    }

    Buf b;
    buf_init(&b);
    char chunk[4096];
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) buf_append(&b, chunk, got);
    fclose(f);

    if (b.len == 0) { buf_free(&b); return 0; }

    char jerr[128];
    JsonValue *root = json_parse(b.data ? b.data : "", jerr, sizeof jerr);
    buf_free(&b);
    if (!root) {
        if (err && errsz) snprintf(err, errsz, "invalid JSON in '%s': %s", path, jerr);
        return -1;
    }

    const JsonValue *tasks = json_get(root, "tasks");
    if (!tasks || tasks->type != JSON_ARRAY) {
        json_free(root);
        if (err && errsz) snprintf(err, errsz, "'%s' has no 'tasks' array", path);
        return -1;
    }

    tasklist_clear(out);
    for (size_t i = 0; i < tasks->u.array.count; i++) {
        const JsonValue *o = tasks->u.array.items[i];
        if (!o || o->type != JSON_OBJECT) continue;
        Task t;
        task_from_json(o, &t, out);
        tasklist_append(out, &t);
        task_free_strings(&t);
    }

    int next = (int)json_num(json_get(root, "next_id"), 0);
    if (next > out->next_id) out->next_id = next;

    json_free(root);
    return 0;
}

/* --------------------------------------------------------------- save --- */

static int atomic_replace(const char *tmp, const char *dst)
{
#ifdef _WIN32
    return MoveFileExA(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(tmp, dst) == 0 ? 0 : -1;
#endif
}

int store_save(const char *path, const TaskList *list, char *err, size_t errsz)
{
    if (make_parent_dirs(path, err, errsz) != 0) return -1;

    Buf b;
    buf_init(&b);
    buf_puts(&b, "{\n  \"version\": ");
    buf_printf(&b, "%d", STORE_VERSION);
    buf_puts(&b, ",\n  \"next_id\": ");
    buf_printf(&b, "%d", list->next_id);
    buf_puts(&b, ",\n  \"tasks\": [");

    for (size_t i = 0; i < list->count; i++) {
        const Task *t = &list->items[i];
        buf_puts(&b, i ? ",\n    {" : "\n    {");
        buf_printf(&b, "\"id\": %d", t->id);

        buf_puts(&b, ", \"title\": ");
        json_write_string(t->title, &b);
        buf_puts(&b, ", \"notes\": ");
        json_write_string(t->notes, &b);

        buf_printf(&b, ", \"done\": %s", t->done ? "true" : "false");
        buf_puts(&b, ", \"priority\": ");
        json_write_string(prio_name(t->prio), &b);
        buf_printf(&b, ", \"created\": %lld", t->created);
        buf_printf(&b, ", \"completed\": %lld", t->completed);

        buf_puts(&b, ", \"tags\": [");
        char **parts = NULL;
        size_t n = split_list(t->tags, &parts);
        for (size_t k = 0; k < n; k++) {
            if (k) buf_putc(&b, ',');
            json_write_string(parts[k], &b);
        }
        for (size_t k = 0; k < n; k++) free(parts[k]);
        free(parts);
        buf_putc(&b, ']');

        buf_puts(&b, ", \"due\": ");
        json_write_string(t->due, &b);
        buf_putc(&b, '}');
    }

    buf_puts(&b, list->count ? "\n  ]\n}\n" : "]\n}\n");
    char *text = buf_release(&b);

    size_t n = strlen(path) + 8;
    char *tmp = xmalloc(n);
    snprintf(tmp, n, "%s.tmp", path);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        if (err && errsz) snprintf(err, errsz, "cannot write '%s': %s", tmp, strerror(errno));
        free(tmp);
        free(text);
        return -1;
    }
    size_t wrote = fwrite(text, 1, strlen(text), f);
    int ok = (wrote == strlen(text));
    if (fclose(f) != 0) ok = 0;
    free(text);

    if (!ok) {
        if (err && errsz) snprintf(err, errsz, "short write to '%s'", tmp);
        remove(tmp);
        free(tmp);
        return -1;
    }
    if (atomic_replace(tmp, path) != 0) {
        if (err && errsz) snprintf(err, errsz, "cannot replace '%s'", path);
        remove(tmp);
        free(tmp);
        return -1;
    }
    free(tmp);
    return 0;
}
