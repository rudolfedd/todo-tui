/* model.c — task data model. */
#include "model.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

const char *prio_name(Priority p)
{
    switch (p) {
    case PRIO_HIGH: return "high";
    case PRIO_MED:  return "med";
    case PRIO_LOW:  return "low";
    default:        return "low";
    }
}

Priority prio_from_name(const char *name)
{
    if (!name) return PRIO_LOW;
    if (ci_eq(name, "high") || ci_eq(name, "urgent") || ci_eq(name, "1")) return PRIO_HIGH;
    if (ci_eq(name, "med") || ci_eq(name, "medium") || ci_eq(name, "normal")) return PRIO_MED;
    return PRIO_LOW;
}

void task_free_strings(Task *t)
{
    if (!t) return;
    free(t->title);
    free(t->notes);
    free(t->tags);
    free(t->due);
    t->title = t->notes = t->tags = t->due = NULL;
}

void tasklist_init(TaskList *l)
{
    l->items = NULL;
    l->count = l->cap = 0;
    l->next_id = 1;
}

void tasklist_clear(TaskList *l)
{
    for (size_t i = 0; i < l->count; i++) task_free_strings(&l->items[i]);
    l->count = 0;
    l->next_id = 1;
}

void tasklist_free(TaskList *l)
{
    if (!l) return;
    tasklist_clear(l);
    free(l->items);
    l->items = NULL;
    l->cap = 0;
}

size_t tasklist_append(TaskList *l, const Task *t)
{
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->items = xrealloc(l->items, l->cap * sizeof *l->items);
    }
    Task *dst = &l->items[l->count];
    dst->id        = t->id;
    dst->title     = xstrdup(t->title);
    dst->notes     = xstrdup(t->notes);
    dst->done      = t->done;
    dst->prio      = t->prio;
    dst->created   = t->created;
    dst->completed = t->completed;
    dst->tags      = xstrdup(t->tags);
    dst->due       = xstrdup(t->due);
    if (dst->id >= l->next_id) l->next_id = dst->id + 1;
    return l->count++;
}

void tasklist_remove(TaskList *l, size_t idx)
{
    if (idx >= l->count) return;
    task_free_strings(&l->items[idx]);
    if (idx + 1 < l->count) {
        memmove(&l->items[idx], &l->items[idx + 1], (l->count - idx - 1) * sizeof *l->items);
    }
    l->count--;
}

Task *tasklist_find_by_id(TaskList *l, int id)
{
    for (size_t i = 0; i < l->count; i++) {
        if (l->items[i].id == id) return &l->items[i];
    }
    return NULL;
}

void tasklist_copy(const TaskList *src, TaskList *dst)
{
    tasklist_clear(dst);
    for (size_t i = 0; i < src->count; i++) tasklist_append(dst, &src->items[i]);
    dst->next_id = src->next_id;
}

size_t task_tag_count(const Task *t)
{
    char **parts = NULL;
    size_t n = split_list(t ? t->tags : "", &parts);
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
    return n;
}

char *task_tag_at(const Task *t, size_t i)
{
    char **parts = NULL;
    size_t n = split_list(t ? t->tags : "", &parts);
    char *r = (i < n) ? xstrdup(parts[i]) : xstrdup("");
    for (size_t k = 0; k < n; k++) free(parts[k]);
    free(parts);
    return r;
}

int task_has_tag(const Task *t, const char *tag)
{
    if (!t || !tag || !*tag) return 0;
    char **parts = NULL;
    size_t n = split_list(t->tags, &parts);
    int found = 0;
    for (size_t i = 0; i < n; i++) {
        if (ci_eq(parts[i], tag)) { found = 1; break; }
    }
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
    return found;
}
