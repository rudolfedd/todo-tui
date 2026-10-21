/* app.h — application state and business logic (no curses dependencies). */
#ifndef TODO_APP_H
#define TODO_APP_H

#include "editor.h"
#include "model.h"

#include <stddef.h>

/* ------------------------------------------------------------- filters --- */

typedef enum {
    VIEW_ALL = 0,
    VIEW_ACTIVE,
    VIEW_DONE,
    VIEW_TODAY,
    VIEW_OVERDUE,
    VIEW_TAG,
    VIEW_COUNT
} ViewMode;

typedef enum {
    SORT_CREATED = 0,
    SORT_PRIORITY,
    SORT_DUE,
    SORT_ALPHA,
    SORT_COUNT
} SortMode;

const char *view_name(ViewMode v);
const char *sort_name(SortMode s);

/* -------------------------------------------------------------- modes --- */

typedef enum {
    MODE_LIST = 0,
    MODE_SEARCH,
    MODE_FORM,
    MODE_HELP,
    MODE_CONFIRM,
    MODE_PICKER
} Mode;

/* --------------------------------------------------------------- form --- */

enum {
    F_TITLE = 0,
    F_NOTES,
    F_PRIO,
    F_TAGS,
    F_DUE,
    F_COUNT
};

typedef struct {
    int     editing_id;   /* -1 => create a new task */
    int     field;
    Editor  ed[F_COUNT];
    int     prio;
    char   *notes_backup; /* unused placeholder for symmetry */
} Form;

/* ------------------------------------------------------------- picker --- */

typedef enum { PICK_FILTER = 0, PICK_TAG, PICK_SORT } PickKind;

typedef struct {
    PickKind kind;
    char   **items;
    size_t   count;
    int      sel;
    int      scroll;
} Picker;

/* ------------------------------------------------------ undo / redo --- */

typedef struct {
    TaskList *snaps;
    size_t    count;
    size_t    cap;
} SnapStack;

/* --------------------------------------------------------------- app --- */

typedef struct {
    TaskList  tasks;

    size_t   *view;        /* indices into tasks, filtered + sorted */
    size_t    view_len;
    size_t    view_cap;
    size_t    cursor;      /* position inside view */
    size_t    scroll;      /* first visible row */
    int       top, bottom; /* viewport geometry (rows) */

    ViewMode  view_mode;
    SortMode  sort_mode;

    Editor    search;      /* live search query */
    char     *search_saved;/* query before entering search mode */
    int       search_dirty;

    char     *tag_filter;  /* active tag filter (may be NULL) */

    Mode      mode;
    Form      form;
    Picker    picker;
    int       confirm_action;  /* CONFIRM_DELETE / CONFIRM_CLEAR_DONE */
    int       help_scroll;

    char     *status;
    long long status_until;

    SnapStack undo;
    SnapStack redo;

    char     *config_path;
    int       dirty;
} App;

/* Actions */
enum { CONFIRM_NONE = 0, CONFIRM_DELETE, CONFIRM_CLEAR_DONE };

void app_init(App *a, const char *config_path);
void app_free(App *a);

void app_rebuild_view(App *a);
void app_scroll_to_cursor(App *a);

Task *app_current(const App *a);
/* Move the cursor onto the given task index (if it is part of the view). */
void  app_focus_index(App *a, size_t task_index);

void app_set_status(App *a, const char *fmt, ...);
void app_tick_status(App *a);

int  app_save(App *a);

/* Mutations (each one pushes an undo snapshot). */
void app_toggle_done(App *a);
void app_cycle_priority(App *a);
void app_delete_current(App *a);
void app_clear_done(App *a);
void app_start_form(App *a, int editing_id);
void app_form_commit(App *a);
void app_form_cancel(App *a);
void app_undo(App *a);
void app_redo(App *a);

void app_open_picker(App *a, PickKind kind);
void app_picker_apply(App *a);
void app_picker_cancel(App *a);

void app_toggle_help_scroll(App *a);

#endif /* TODO_APP_H */
