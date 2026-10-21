/* app.c — application state and business logic. */
#include "app.h"
#include "store.h"
#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATUS_MS 2600
#define MAX_UNDO  120

const char *view_name(ViewMode v)
{
    switch (v) {
    case VIEW_ALL:     return "all";
    case VIEW_ACTIVE:  return "active";
    case VIEW_DONE:    return "done";
    case VIEW_TODAY:   return "today";
    case VIEW_OVERDUE: return "overdue";
    case VIEW_TAG:     return "tag";
    default:           return "?";
    }
}

const char *sort_name(SortMode s)
{
    switch (s) {
    case SORT_CREATED:  return "newest";
    case SORT_PRIORITY: return "priority";
    case SORT_DUE:      return "due";
    case SORT_ALPHA:    return "alphabet";
    default:            return "?";
    }
}

/* --------------------------------------------------------------- util --- */

static void snap_push(SnapStack *s, const TaskList *src)
{
    if (s->count == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 16;
        s->snaps = xrealloc(s->snaps, cap * sizeof *s->snaps);
        s->cap = cap;
    }
    TaskList *copy = xmalloc(sizeof *copy);
    tasklist_init(copy);
    tasklist_copy(src, copy);
    s->snaps[s->count++] = *copy;
    free(copy);
}

static TaskList *snap_pop(SnapStack *s)
{
    if (!s->count) return NULL;
    TaskList *l = &s->snaps[--s->count];
    return l;
}

static void snap_clear(SnapStack *s)
{
    for (size_t i = 0; i < s->count; i++) tasklist_free(&s->snaps[i]);
    s->count = 0;
}

static void snapshot(App *a)
{
    snap_push(&a->undo, &a->tasks);
    if (a->undo.count > MAX_UNDO) {
        /* drop the oldest snapshot */
        tasklist_free(&a->undo.snaps[0]);
        memmove(&a->undo.snaps[0], &a->undo.snaps[1], (a->undo.count - 1) * sizeof *a->undo.snaps);
        a->undo.count--;
    }
    snap_clear(&a->redo);
}

/* ------------------------------------------------------------ filters --- */

static int task_matches(const Task *t, const App *a)
{
    switch (a->view_mode) {
    case VIEW_ACTIVE:
        if (t->done) return 0;
        break;
    case VIEW_DONE:
        if (!t->done) return 0;
        break;
    case VIEW_TODAY:
        if (!date_is_today(t->due)) return 0;
        break;
    case VIEW_OVERDUE:
        if (t->done || !date_is_past(t->due)) return 0;
        break;
    case VIEW_TAG:
        if (!a->tag_filter || !task_has_tag(t, a->tag_filter)) return 0;
        break;
    default:
        break;
    }

    const char *q = ed_text(&a->search);
    if (q && *q) {
        if (!ci_strstr(t->title, q) &&
            !ci_strstr(t->notes, q) &&
            !ci_strstr(t->tags, q) &&
            !ci_strstr(t->due, q))
            return 0;
    }
    return 1;
}

static int task_cmp(const Task *x, const Task *y, SortMode mode)
{
    switch (mode) {
    case SORT_PRIORITY: {
        if (x->prio != y->prio) return (int)y->prio - (int)x->prio; /* high first */
        break;
    }
    case SORT_DUE: {
        int xe = (x->due && *x->due) ? 0 : 1;
        int ye = (y->due && *y->due) ? 0 : 1;
        if (xe != ye) return xe - ye;                     /* dated first */
        if (!xe) {
            int c = strcmp(x->due, y->due);
            if (c) return c;
        }
        if (x->prio != y->prio) return (int)y->prio - (int)x->prio;
        break;
    }
    case SORT_ALPHA: {
        int c = ci_cmp(x->title, y->title);
        if (c) return c;
        break;
    }
    case SORT_CREATED:
    default: {
        if (x->created != y->created) return (x->created < y->created) ? 1 : -1; /* newest first */
        break;
    }
    }
    if (x->created != y->created) return (x->created < y->created) ? 1 : -1;
    return x->id - y->id;
}

/* Stable merge sort over indices. */
static void msort(size_t *a, size_t *tmp, size_t n, const TaskList *l, SortMode mode)
{
    if (n < 2) return;
    size_t mid = n / 2;
    msort(a, tmp, mid, l, mode);
    msort(a + mid, tmp, n - mid, l, mode);
    size_t i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        const Task *x = &l->items[a[i]];
        const Task *y = &l->items[a[j]];
        if (task_cmp(x, y, mode) <= 0) tmp[k++] = a[i++];
        else tmp[k++] = a[j++];
    }
    while (i < mid) tmp[k++] = a[i++];
    while (j < n)  tmp[k++] = a[j++];
    memcpy(a, tmp, n * sizeof *a);
}

void app_rebuild_view(App *a)
{
    size_t n = 0;
    for (size_t i = 0; i < a->tasks.count; i++) {
        if (task_matches(&a->tasks.items[i], a)) n++;
    }

    if (n > a->view_cap) {
        a->view_cap = n + 16;
        a->view = xrealloc(a->view, a->view_cap * sizeof *a->view);
    }

    size_t k = 0;
    for (size_t i = 0; i < a->tasks.count; i++) {
        if (task_matches(&a->tasks.items[i], a)) a->view[k++] = i;
    }
    a->view_len = k;

    if (k > 0) {
        size_t *tmp = xmalloc(k * sizeof *tmp);
        msort(a->view, tmp, k, &a->tasks, a->sort_mode);
        free(tmp);
    }

    if (a->cursor >= a->view_len) a->cursor = a->view_len ? a->view_len - 1 : 0;
    app_scroll_to_cursor(a);
}

void app_scroll_to_cursor(App *a)
{
    int height = a->bottom - a->top;
    if (height <= 0) return;
    if (a->cursor < a->scroll) a->scroll = a->cursor;
    if (a->cursor >= a->scroll + (size_t)height) a->scroll = a->cursor - (size_t)height + 1;
    if (a->view_len && a->scroll + (size_t)height > a->view_len) {
        size_t over = a->scroll + (size_t)height - a->view_len;
        a->scroll = over > a->scroll ? 0 : a->scroll - over;
    }
}

/* -------------------------------------------------------------- setup --- */

void app_init(App *a, const char *config_path)
{
    memset(a, 0, sizeof *a);
    tasklist_init(&a->tasks);
    ed_init(&a->search);
    a->view_mode = VIEW_ACTIVE;
    a->sort_mode = SORT_CREATED;
    a->mode = MODE_LIST;
    a->config_path = xstrdup(config_path ? config_path : "");

    for (int i = 0; i < F_COUNT; i++) ed_init(&a->form.ed[i]);
    a->form.editing_id = -1;
    a->form.prio = PRIO_MED;

    a->status_until = 0;
    a->search_saved = xstrdup("");
}

void app_free(App *a)
{
    tasklist_free(&a->tasks);
    free(a->view);
    ed_free(&a->search);
    free(a->search_saved);
    free(a->tag_filter);
    free(a->status);
    free(a->config_path);
    for (int i = 0; i < F_COUNT; i++) ed_free(&a->form.ed[i]);
    free(a->form.notes_backup);
    if (a->picker.items) {
        for (size_t i = 0; i < a->picker.count; i++) free(a->picker.items[i]);
        free(a->picker.items);
    }
    snap_clear(&a->undo);
    snap_clear(&a->redo);
    free(a->undo.snaps);
    free(a->redo.snaps);
}

/* ------------------------------------------------------------ status --- */

void app_set_status(App *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) return;

    free(a->status);
    a->status = xmalloc((size_t)need + 1);
    va_start(ap, fmt);
    vsnprintf(a->status, (size_t)need + 1, fmt, ap);
    va_end(ap);
    a->status_until = now_epoch() + STATUS_MS / 1000;
}

void app_tick_status(App *a)
{
    if (a->status && a->status_until && now_epoch() >= a->status_until) {
        free(a->status);
        a->status = NULL;
        a->status_until = 0;
    }
}

int app_save(App *a)
{
    if (!a->config_path || !*a->config_path) return -1;
    char err[256];
    if (store_save(a->config_path, &a->tasks, err, sizeof err) != 0) {
        app_set_status(a, "Save failed: %s", err);
        return -1;
    }
    a->dirty = 0;
    return 0;
}

/* ----------------------------------------------------------- queries --- */

Task *app_current(const App *a)
{
    if (a->cursor >= a->view_len) return NULL;
    return &a->tasks.items[a->view[a->cursor]];
}

/* --------------------------------------------------------- mutations --- */

void app_focus_index(App *a, size_t task_index)
{
    for (size_t i = 0; i < a->view_len; i++) {
        if (a->view[i] == task_index) {
            a->cursor = i;
            app_scroll_to_cursor(a);
            return;
        }
    }
    if (a->cursor >= a->view_len) a->cursor = a->view_len ? a->view_len - 1 : 0;
}

void app_toggle_done(App *a)
{
    Task *t = app_current(a);
    if (!t) { app_set_status(a, "Nothing to toggle"); return; }
    size_t idx = a->view[a->cursor];
    snapshot(a);
    t->done = !t->done;
    t->completed = t->done ? now_epoch() : 0;
    a->dirty = 1;
    app_rebuild_view(a);
    app_focus_index(a, idx);
    if (app_save(a) == 0)
        app_set_status(a, t->done ? "Marked as done" : "Marked as active");
}

void app_cycle_priority(App *a)
{
    Task *t = app_current(a);
    if (!t) return;
    size_t idx = a->view[a->cursor];
    snapshot(a);
    t->prio = (Priority)((t->prio + 1) % 3);
    a->dirty = 1;
    app_rebuild_view(a);
    app_focus_index(a, idx);
    if (app_save(a) == 0)
        app_set_status(a, "Priority: %s", prio_name(t->prio));
}

void app_delete_current(App *a)
{
    Task *t = app_current(a);
    if (!t) { app_set_status(a, "Nothing to delete"); return; }
    snapshot(a);
    size_t idx = a->view[a->cursor];
    tasklist_remove(&a->tasks, idx);
    a->dirty = 1;
    app_rebuild_view(a);
    app_save(a);
    app_set_status(a, "Task deleted");
}

void app_clear_done(App *a)
{
    int removed = 0;
    for (size_t i = a->tasks.count; i-- > 0;) {
        if (a->tasks.items[i].done) removed++;
    }
    if (!removed) { app_set_status(a, "No completed tasks"); return; }

    snapshot(a); /* snapshot BEFORE the removal so undo can restore */
    for (size_t i = a->tasks.count; i-- > 0;) {
        if (a->tasks.items[i].done) tasklist_remove(&a->tasks, i);
    }
    a->dirty = 1;
    app_rebuild_view(a);
    if (app_save(a) == 0)
        app_set_status(a, "Removed %d completed task(s)", removed);
}

void app_start_form(App *a, int editing_id)
{
    Form *f = &a->form;
    f->editing_id = editing_id;
    f->field = F_TITLE;

    if (editing_id >= 0) {
        Task *t = tasklist_find_by_id(&a->tasks, editing_id);
        if (!t) { a->mode = MODE_LIST; return; }
        ed_set(&f->ed[F_TITLE], t->title);
        ed_set(&f->ed[F_NOTES], t->notes);
        f->prio = (int)t->prio;
        ed_set(&f->ed[F_TAGS], t->tags);
        ed_set(&f->ed[F_DUE], t->due);
    } else {
        ed_clear(&f->ed[F_TITLE]);
        ed_clear(&f->ed[F_NOTES]);
        f->prio = PRIO_MED;
        ed_clear(&f->ed[F_TAGS]);
        ed_clear(&f->ed[F_DUE]);
    }
    a->mode = MODE_FORM;
}

void app_form_commit(App *a)
{
    Form *f = &a->form;
    char *title = xstrdup(ed_text(&f->ed[F_TITLE]));
    str_strip(title);
    if (!*title) {
        app_set_status(a, "Title cannot be empty");
        free(title);
        return;
    }

    char *notes = xstrdup(ed_text(&f->ed[F_NOTES]));
    char *tags  = xstrdup(ed_text(&f->ed[F_TAGS]));
    char *due   = xstrdup(ed_text(&f->ed[F_DUE]));
    str_strip(notes);
    str_strip(tags);
    str_strip(due);

    snapshot(a);

    if (f->editing_id >= 0) {
        Task *t = tasklist_find_by_id(&a->tasks, f->editing_id);
        if (t) {
            free(t->title); t->title = xstrdup(title);
            free(t->notes); t->notes = xstrdup(notes);
            free(t->tags);  t->tags  = xstrdup(tags);
            free(t->due);   t->due   = xstrdup(due);
            t->prio = (Priority)f->prio;
        }
        app_set_status(a, "Task updated");
    } else {
        Task t;
        memset(&t, 0, sizeof t);
        t.id        = a->tasks.next_id++;
        t.title     = title;
        t.notes     = notes;
        t.tags      = tags;
        t.due       = due;
        t.prio      = (Priority)f->prio;
        t.done      = 0;
        t.created   = now_epoch();
        t.completed = 0;
        tasklist_append(&a->tasks, &t);
        app_set_status(a, "Task added");
    }

    free(title);
    free(notes);
    free(tags);
    free(due);

    a->dirty = 1;
    a->mode = MODE_LIST;
    app_rebuild_view(a);
    if (app_save(a) == 0)
        app_set_status(a, f->editing_id >= 0 ? "Task updated" : "Task added");
}

void app_form_cancel(App *a)
{
    a->mode = MODE_LIST;
    app_set_status(a, "Cancelled");
}

void app_undo(App *a)
{
    TaskList *snap = snap_pop(&a->undo);
    if (!snap) { app_set_status(a, "Nothing to undo"); return; }
    snap_push(&a->redo, &a->tasks);
    tasklist_copy(snap, &a->tasks);
    tasklist_free(snap);
    a->dirty = 1;
    app_rebuild_view(a);
    app_save(a);
    app_set_status(a, "Undo");
}

void app_redo(App *a)
{
    TaskList *snap = snap_pop(&a->redo);
    if (!snap) { app_set_status(a, "Nothing to redo"); return; }
    snap_push(&a->undo, &a->tasks);
    tasklist_copy(snap, &a->tasks);
    tasklist_free(snap);
    a->dirty = 1;
    app_rebuild_view(a);
    app_save(a);
    app_set_status(a, "Redo");
}

/* ------------------------------------------------------------ picker --- */

void app_open_picker(App *a, PickKind kind)
{
    Picker *p = &a->picker;
    if (p->items) {
        for (size_t i = 0; i < p->count; i++) free(p->items[i]);
        free(p->items);
        p->items = NULL;
        p->count = 0;
    }
    p->kind = kind;
    p->sel = 0;
    p->scroll = 0;

    switch (kind) {
    case PICK_FILTER: {
        size_t cap = VIEW_COUNT;
        p->items = xmalloc(cap * sizeof *p->items);
        for (int i = 0; i < VIEW_COUNT; i++) p->items[p->count++] = xstrdup(view_name((ViewMode)i));
        p->sel = (int)a->view_mode;
        break;
    }
    case PICK_SORT: {
        size_t cap = SORT_COUNT;
        p->items = xmalloc(cap * sizeof *p->items);
        for (int i = 0; i < SORT_COUNT; i++) p->items[p->count++] = xstrdup(sort_name((SortMode)i));
        p->sel = (int)a->sort_mode;
        break;
    }
    case PICK_TAG: {
        size_t cap = 16;
        p->items = xmalloc(cap * sizeof *p->items);
        for (size_t i = 0; i < a->tasks.count; i++) {
            char **parts = NULL;
            size_t n = split_list(a->tasks.items[i].tags, &parts);
            for (size_t k = 0; k < n; k++) {
                int dup = 0;
                for (size_t j = 0; j < p->count; j++) {
                    if (ci_eq(p->items[j], parts[k])) { dup = 1; break; }
                }
                if (!dup) {
                    if (p->count == cap) {
                        cap *= 2;
                        p->items = xrealloc(p->items, cap * sizeof *p->items);
                    }
                    p->items[p->count++] = xstrdup(parts[k]);
                }
            }
            for (size_t k = 0; k < n; k++) free(parts[k]);
            free(parts);
        }
        /* sort tags alphabetically */
        for (size_t i = 0; i < p->count; i++) {
            for (size_t j = i + 1; j < p->count; j++) {
                if (ci_cmp(p->items[j], p->items[i]) < 0) {
                    char *tmp = p->items[i];
                    p->items[i] = p->items[j];
                    p->items[j] = tmp;
                }
            }
        }
        if (a->tag_filter) {
            for (size_t i = 0; i < p->count; i++) {
                if (ci_eq(p->items[i], a->tag_filter)) { p->sel = (int)i; break; }
            }
        }
        break;
    }
    }
    a->mode = MODE_PICKER;
}

void app_picker_apply(App *a)
{
    Picker *p = &a->picker;
    if (p->sel < 0 || (size_t)p->sel >= p->count) { a->mode = MODE_LIST; return; }
    const char *choice = p->items[p->sel];

    switch (p->kind) {
    case PICK_FILTER: {
        ViewMode m = VIEW_ALL;
        for (int i = 0; i < VIEW_COUNT; i++) {
            if (strcmp(view_name((ViewMode)i), choice) == 0) { m = (ViewMode)i; break; }
        }
        if (m == VIEW_TAG && !a->tag_filter) m = VIEW_ALL;
        a->view_mode = m;
        a->cursor = 0;
        a->scroll = 0;
        app_rebuild_view(a);
        app_set_status(a, "Filter: %s", choice);
        break;
    }
    case PICK_SORT: {
        SortMode m = SORT_CREATED;
        for (int i = 0; i < SORT_COUNT; i++) {
            if (strcmp(sort_name((SortMode)i), choice) == 0) { m = (SortMode)i; break; }
        }
        a->sort_mode = m;
        app_rebuild_view(a);
        app_set_status(a, "Sort: %s", choice);
        break;
    }
    case PICK_TAG:
        free(a->tag_filter);
        a->tag_filter = xstrdup(choice);
        a->view_mode = VIEW_TAG;
        a->cursor = 0;
        a->scroll = 0;
        app_rebuild_view(a);
        app_set_status(a, "Tag: %s", choice);
        break;
    }
    a->mode = MODE_LIST;
}

void app_picker_cancel(App *a)
{
    a->mode = MODE_LIST;
    app_set_status(a, "Cancelled");
}
