/* ui.c — rendering and key handling. */
#include "ui.h"
#include "curses_compat.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------- colours --- */

enum {
    CP_DEFAULT = 1,
    CP_HEADER,
    CP_HINT,
    CP_SEL,
    CP_DONE,
    CP_HIGH,
    CP_MED,
    CP_LOW,
    CP_TAG,
    CP_DUE,
    CP_OVERDUE,
    CP_STATUS,
    CP_DIALOG,
    CP_DIM,
    CP_BAR,
    CP_COUNT
};

static int g_h, g_w;
static int g_inited;

static void init_colors(void)
{
    if (!has_colors()) return;
    start_color();
#ifdef NCURSES_VERSION
    use_default_colors();
#else
    use_default_colors();
#endif

    init_pair(CP_DEFAULT, -1, -1);
    init_pair(CP_HEADER,  COLOR_BLACK,  COLOR_CYAN);
    init_pair(CP_HINT,    COLOR_WHITE,  COLOR_BLUE);
    init_pair(CP_SEL,     COLOR_BLACK,  COLOR_WHITE);
    init_pair(CP_DONE,    COLOR_GREEN,  -1);
    init_pair(CP_HIGH,    COLOR_RED,    -1);
    init_pair(CP_MED,     COLOR_YELLOW, -1);
    init_pair(CP_LOW,     COLOR_CYAN,   -1);
    init_pair(CP_TAG,     COLOR_MAGENTA, -1);
    init_pair(CP_DUE,     COLOR_GREEN,  -1);
    init_pair(CP_OVERDUE, COLOR_RED,    -1);
    init_pair(CP_STATUS,  COLOR_YELLOW, -1);
    init_pair(CP_DIALOG,  COLOR_WHITE,  COLOR_BLUE);
    init_pair(CP_DIM,     COLOR_BLUE,   -1);
    init_pair(CP_BAR,     COLOR_GREEN,  -1);
}

/* --------------------------------------------------------- primitives --- */

/* Print `s` at (y,x), clipped to the screen. Handles UTF-8 gracefully. */
static void put(int y, int x, const char *s, int pair)
{
    if (!s || !*s) return;
    if (y < 0 || y >= g_h || x < 0 || x >= g_w) return;

    int maxw = g_w - x;
    size_t chars = utf8_len(s);
    char *tmp = NULL;
    if (chars > (size_t)maxw) {
        tmp = utf8_truncate(s, (size_t)maxw);
        s = tmp;
    }
    attron(COLOR_PAIR(pair));
    mvaddstr(y, x, s);
    attroff(COLOR_PAIR(pair));
    free(tmp);
}

static void fill(int y, int x, int w, int pair)
{
    if (y < 0 || y >= g_h) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > g_w) w = g_w - x;
    if (w <= 0) return;
    attron(COLOR_PAIR(pair));
    for (int i = 0; i < w; i++) mvaddch(y, x + i, ' ');
    attroff(COLOR_PAIR(pair));
}

static void hline_at(int y, int x, int w)
{
    if (y < 0 || y >= g_h || w <= 0) return;
    if (x + w > g_w) w = g_w - x;
    if (x < 0) { w += x; x = 0; }
    if (w <= 0) return;
    for (int i = 0; i < w; i++) mvaddch(y, x + i, ACS_HLINE);
}

/* ------------------------------------------------------------- layout --- */

static void draw_header(App *a, int W)
{
    size_t total = a->tasks.count;
    size_t done = 0;
    for (size_t i = 0; i < total; i++) if (a->tasks.items[i].done) done++;

    Buf b;
    buf_init(&b);
    buf_puts(&b, " todo-tui ");
    buf_printf(&b, " %zu/%zu done ", done, total);
    buf_printf(&b, " view:%s ", view_name(a->view_mode));
    buf_printf(&b, " sort:%s ", sort_name(a->sort_mode));
    if (a->tag_filter) buf_printf(&b, " tag:#%s ", a->tag_filter);
    const char *q = ed_text(&a->search);
    if (q && *q) buf_printf(&b, " find:%s ", q);

    fill(0, 0, W, CP_HEADER);
    put(0, 0, b.data ? b.data : "", CP_HEADER);
    buf_free(&b);
}

static void draw_list(App *a, int top, int bottom, int W)
{
    int height = bottom - top + 1;
    if (height <= 0) return;

    if (a->view_len == 0) {
        put(top + 1, 4, "Nothing to show.", CP_DIM);
        put(top + 2, 4, "Press 'a' to add a task, '/' to search, 'f' to change the filter.", CP_DIM);
        return;
    }

    for (int i = 0; i < height; i++) {
        size_t vi = a->scroll + (size_t)i;
        if (vi >= a->view_len) break;

        const Task *t = &a->tasks.items[a->view[vi]];
        int  y      = top + i;
        int  sel    = (vi == a->cursor);
        int  base   = sel ? CP_SEL : CP_DEFAULT;

        /* right hand block ------------------------------------------- */
        int prio_pair = (t->prio == PRIO_HIGH) ? CP_HIGH
                       : (t->prio == PRIO_MED) ? CP_MED : CP_LOW;

        char **tags = NULL;
        size_t ntags = split_list(t->tags, &tags);
        int tags_pair = CP_TAG;
        int tags_w = 0;
        for (size_t k = 0; k < ntags; k++) {
            tags_w += (int)utf8_len(tags[k]) + 1;
            if (k) tags_w += 1;
        }

        int due_pair = CP_DUE;
        const char *due_text = t->due;
        int overdue = (!t->done && date_is_past(t->due));
        if (overdue) due_pair = CP_OVERDUE;

        int prio_w = 4;                 /* HIGH / MED  / LOW  */
        int due_w  = due_text && *due_text ? (int)utf8_len(due_text) : 0;
        if (overdue) due_w += 8;        /* "overdue " prefix */

        int right_w = tags_w + 1 + prio_w + (due_w ? due_w + 1 : 0);

        /* left hand block -------------------------------------------- */
        int title_x = 6;
        int avail = W - title_x - right_w - 2;
        if (avail < 8) avail = 8;

        char *title = utf8_truncate(t->title, (size_t)avail);

        /* draw --------------------------------------------------------- */
        if (sel) fill(y, 0, W, CP_SEL);
        put(y, 0, sel ? ">" : " ", base);
        put(y, 2, t->done ? "[x]" : "[ ]", t->done ? (sel ? CP_SEL : CP_DONE) : base);
        put(y, title_x, title, t->done ? (sel ? CP_SEL : CP_DONE) : base);

        int rx = W - right_w;
        if (rx < title_x + (int)utf8_len(title) + 1) rx = title_x + (int)utf8_len(title) + 1;

        if (ntags) {
            Buf tb;
            buf_init(&tb);
            for (size_t k = 0; k < ntags; k++) {
                if (k) buf_putc(&tb, ' ');
                buf_printf(&tb, "#%s", tags[k]);
            }
            put(y, rx, tb.data, sel ? CP_SEL : tags_pair);
            buf_free(&tb);
            rx += tags_w;
        } else {
            rx += tags_w;
        }
        rx += 1;

        put(y, rx, prio_name(t->prio), sel ? CP_SEL : prio_pair);
        rx += prio_w;

        if (due_w) {
            rx += 1;
            if (overdue) {
                put(y, rx, "overdue ", sel ? CP_SEL : CP_OVERDUE);
                rx += 8;
            }
            put(y, rx, due_text, sel ? CP_SEL : due_pair);
        }

        free(title);
        for (size_t k = 0; k < ntags; k++) free(tags[k]);
        free(tags);
    }
}

static void draw_status(App *a, int y, int W)
{
    /* left: progress + counters, right: transient status message */
    size_t total = a->tasks.count, done = 0;
    for (size_t i = 0; i < total; i++) if (a->tasks.items[i].done) done++;

    int barw = 12;
    int filled = total ? (int)((double)done * barw / (double)total + 0.5) : 0;

    Buf b;
    buf_init(&b);
    buf_printf(&b, " %zu/%zu ", done, total);
    for (int i = 0; i < barw; i++) buf_putc(&b, i < filled ? '#' : '-');

    if (a->view_len != total) {
        buf_printf(&b, "   shown %zu", a->view_len);
    }
    if (a->cursor < a->view_len) {
        buf_printf(&b, "   #%zu/%zu", a->cursor + 1, a->view_len);
    }

    put(y, 0, b.data ? b.data : "", CP_DIM);
    buf_free(&b);

    const Task *t = app_current(a);
    int msg_len = a->status ? (int)utf8_len(a->status) : 0;

    if (!a->status && t && t->notes && *t->notes) {
        int x = 30;
        int limit = W - x - msg_len - 3;
        if (limit > 4) {
            char *notes = utf8_truncate(t->notes, (size_t)limit);
            put(y, x, notes, CP_DIM);
            free(notes);
        }
    }

    if (a->status) {
        int x = W - msg_len - 2;
        if (x < 1) x = 1;
        put(y, x, a->status, CP_STATUS);
    }
}

static void draw_search_line(App *a, int y, int W)
{
    const char *q = ed_text(&a->search);
    Buf b;
    buf_init(&b);
    buf_puts(&b, " find: ");
    buf_puts(&b, q);
    put(y, 0, b.data ? b.data : "", CP_STATUS);

    size_t col = 7 + ed_col(&a->search);
    if ((int)col < W - 1) {
        attron(COLOR_PAIR(CP_STATUS));
        mvaddch(y, (int)col, '_');
        attroff(COLOR_PAIR(CP_STATUS));
    }
    buf_free(&b);
}

static const char *hints_for(const App *a)
{
    switch (a->mode) {
    case MODE_LIST:
        return "j/k move  space done  a add  e edit  d del  p prio  / find  f filter  s sort  t tag  u undo  ? help  q quit";
    case MODE_SEARCH:
        return "Enter apply   Esc cancel   Ctrl-W delete word   Ctrl-U clear";
    case MODE_FORM:
        return "Tab next field   Enter save   Esc cancel   Left/Right change priority";
    case MODE_HELP:
        return "j/k scroll   q or ? close";
    case MODE_CONFIRM:
        return "y confirm   n or Esc cancel";
    case MODE_PICKER:
        return "j/k move   Enter select   Esc cancel";
    default:
        return "";
    }
}

static void draw_hints(const App *a, int y, int W)
{
    fill(y, 0, W, CP_HINT);
    put(y, 0, hints_for(a), CP_HINT);
}

/* ------------------------------------------------------------ dialogs --- */

static void draw_box(int y, int x, int h, int w, const char *title)
{
    for (int i = 0; i < w; i++) {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + h - 1, x + i, ACS_HLINE);
    }
    for (int i = 0; i < h; i++) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + w - 1, ACS_VLINE);
    }
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

    if (title && *title) {
        put(y, x + 2, title, CP_DIALOG);
    }
}

static void draw_form(App *a)
{
    int w = g_w - 8;  if (w > 68) w = 68;  if (w < 30) w = 30;
    int h = 2 + F_COUNT + 2;
    int x = (g_w - w) / 2;
    int y = (g_h - h) / 2;
    if (y < 0) y = 0;

    draw_box(y, x, h, w, a->form.editing_id >= 0 ? " Edit task " : " New task ");
    fill(y, x, w, CP_DIALOG);

    static const char *labels[F_COUNT] = { "Title   ", "Notes   ", "Priority", "Tags    ", "Due     " };

    for (int i = 0; i < F_COUNT; i++) {
        int ly = y + 1 + i;
        put(ly, x + 2, labels[i], CP_DIALOG);
        int vx = x + 12;
        int vw = w - 14;
        int active = (a->form.field == i);

        if (i == F_PRIO) {
            const char *p = prio_name((Priority)a->form.prio);
            put(ly, vx, p, active ? CP_STATUS : CP_DIALOG);
            if (active) {
                int len = (int)utf8_len(p);
                put(ly, vx + len + 1, "< >", CP_DIALOG);
            }
        } else {
            const char *txt = ed_text(&a->form.ed[i]);
            char *clip = utf8_truncate(txt, (size_t)(vw > 0 ? vw : 0));
            put(ly, vx, clip, active ? CP_STATUS : CP_DIALOG);
            if (active) {
                size_t col = ed_col(&a->form.ed[i]);
                if (col > (size_t)vw) col = (size_t)vw;
                attron(COLOR_PAIR(CP_STATUS));
                mvaddch(ly, vx + (int)col, '_');
                attroff(COLOR_PAIR(CP_STATUS));
            }
            free(clip);
        }
    }

    put(y + h - 2, x + 2, "Enter save   Tab next field   Esc cancel", CP_DIALOG);
}

static void draw_picker(App *a)
{
    Picker *p = &a->picker;
    int items = (int)p->count;
    int h = items + 4;
    if (h > g_h - 2) h = g_h - 2;
    int w = 34;
    if (w > g_w - 4) w = g_w - 4;
    if (w < 20) w = 20;
    int x = (g_w - w) / 2;
    int y = (g_h - h) / 2;
    if (y < 0) y = 0;

    const char *title = p->kind == PICK_FILTER ? " Filter "
                       : p->kind == PICK_SORT ? " Sort " : " Tag ";
    draw_box(y, x, h, w, title);
    fill(y, x, w, CP_DIALOG);

    int vis = h - 4;
    if (vis < 1) vis = 1;
    if (p->sel < p->scroll) p->scroll = p->sel;
    if (p->sel >= p->scroll + vis) p->scroll = p->sel - vis + 1;

    for (int i = 0; i < vis; i++) {
        size_t idx = (size_t)(p->scroll + i);
        if (idx >= p->count) break;
        int selected = ((int)idx == p->sel);
        int ly = y + 1 + i;
        if (selected) fill(ly, x + 1, w - 2, CP_SEL);
        put(ly, x + 2, selected ? ">" : " ", selected ? CP_SEL : CP_DIALOG);
        put(ly, x + 4, p->items[idx], selected ? CP_SEL : CP_DIALOG);
    }

    if (p->count == 0) {
        put(y + 1, x + 4, "(no tags yet)", CP_DIALOG);
    }
}

static void draw_confirm(App *a)
{
    const char *msg = (a->confirm_action == CONFIRM_CLEAR_DONE)
                    ? "Remove all completed tasks?"
                    : "Delete this task?";
    int w = (int)utf8_len(msg) + 6;
    if (w > g_w - 4) w = g_w - 4;
    if (w < 20) w = 20;
    int h = 4;
    int x = (g_w - w) / 2;
    int y = (g_h - h) / 2;
    if (y < 0) y = 0;

    draw_box(y, x, h, w, " Confirm ");
    fill(y, x, w, CP_DIALOG);
    put(y + 1, x + 2, msg, CP_DIALOG);
    put(y + 2, x + 2, "[y] yes    [n] no", CP_DIALOG);
}

static const char *help_text[] = {
    "Navigation",
    "   j / Down        move down",
    "   k / Up          move up",
    "   g / Home        first task",
    "   G / End         last task",
    "   Ctrl-D / PgDn   half page down",
    "   Ctrl-U / PgUp   half page up",
    "",
    "Tasks",
    "   Space           toggle done",
    "   a               add task",
    "   e               edit task",
    "   d               delete task (asks)",
    "   p               cycle priority",
    "   X               remove all completed (asks)",
    "",
    "Find & view",
    "   /               search",
    "   f               change filter",
    "   s               change sort",
    "   t               filter by tag",
    "   Esc             clear search / tag filter",
    "",
    "Other",
    "   u / Ctrl-R      undo / redo",
    "   ?               toggle this help",
    "   q / Ctrl-C      quit",
    NULL
};

static void draw_help(App *a)
{
    fill(0, 0, g_w, CP_DIALOG);
    put(0, 2, " todo-tui — help ", CP_DIALOG);
    hline_at(1, 0, g_w);

    int total = 0;
    while (help_text[total]) total++;
    int vis = g_h - 4;
    if (vis < 1) vis = 1;
    if (a->help_scroll > total - vis) a->help_scroll = total - vis;
    if (a->help_scroll < 0) a->help_scroll = 0;

    for (int i = 0; i < vis; i++) {
        int idx = a->help_scroll + i;
        if (idx >= total) break;
        put(2 + i, 3, help_text[idx], CP_DIALOG);
    }
    hline_at(g_h - 2, 0, g_w);
    put(g_h - 1, 2, " j/k scroll   q or ? close", CP_DIALOG);
}

/* ----------------------------------------------------------- draw all --- */

void ui_draw(App *a)
{
    getmaxyx(stdscr, g_h, g_w);
    if (g_h < 6 || g_w < 30) {
        put(0, 0, "Terminal too small", CP_STATUS);
        refresh();
        return;
    }

    erase();

    int list_top = 2;
    int list_bottom = g_h - 4;
    int sep2 = g_h - 3;
    int status_row = g_h - 2;
    int hint_row = g_h - 1;

    a->top = list_top;
    a->bottom = list_bottom + 1;
    app_scroll_to_cursor(a);

    draw_header(a, g_w);
    hline_at(1, 0, g_w);
    draw_list(a, list_top, list_bottom, g_w);
    hline_at(sep2, 0, g_w);

    if (a->mode == MODE_SEARCH) {
        draw_search_line(a, status_row, g_w);
    } else {
        draw_status(a, status_row, g_w);
    }
    draw_hints(a, hint_row, g_w);

    switch (a->mode) {
    case MODE_FORM:    draw_form(a); break;
    case MODE_PICKER:  draw_picker(a); break;
    case MODE_CONFIRM: draw_confirm(a); break;
    case MODE_HELP:    draw_help(a); break;
    default: break;
    }

    /* show the real cursor only while editing text */
    if (a->mode == MODE_FORM) {
        int i = a->form.field;
        if (i != F_PRIO) {
            int w = g_w - 8; if (w > 68) w = 68; if (w < 30) w = 30;
            int h = 2 + F_COUNT + 2;
            int x = (g_w - w) / 2;
            int y = (g_h - h) / 2;
            if (y < 0) y = 0;
            int vx = x + 12;
            int vw = w - 14;
            size_t col = ed_col(&a->form.ed[i]);
            if (col > (size_t)vw) col = (size_t)vw;
            curs_set(1);
            move(y + 1 + i, vx + (int)col);
        } else {
            curs_set(0);
        }
    } else if (a->mode == MODE_SEARCH) {
        curs_set(1);
        size_t col = 7 + ed_col(&a->search);
        move(status_row, (int)col);
    } else {
        curs_set(0);
    }

    refresh();
}

/* ------------------------------------------------------- key reading --- */

int ui_read_key(KeyEvent *ev)
{
    ev->key = 0;
    ev->text[0] = '\0';

    int ch = getch();
    if (ch == ERR) return 0;

    if (ch >= 0x80) {
        /* UTF-8 lead byte: collect the continuation bytes */
        char seq[5];
        int n = 0;
        seq[n++] = (char)ch;
        int extra = 0;
        if ((ch & 0xE0) == 0xC0) extra = 1;
        else if ((ch & 0xF0) == 0xE0) extra = 2;
        else if ((ch & 0xF8) == 0xF0) extra = 3;

        for (int i = 0; i < extra; i++) {
            int c = getch();
            if (c == ERR || c < 0x80 || (c & 0xC0) != 0x80) break;
            seq[n++] = (char)c;
        }
        seq[n] = '\0';
        snprintf(ev->text, sizeof ev->text, "%s", seq);
        ev->key = -1; /* text event */
        return 1;
    }

    ev->key = ch;
    return 1;
}

/* ------------------------------------------------------ key handling --- */

static void goto_top(App *a)    { a->cursor = 0; a->scroll = 0; app_scroll_to_cursor(a); }
static void goto_bottom(App *a) { a->cursor = a->view_len ? a->view_len - 1 : 0; app_scroll_to_cursor(a); }

static void move_cursor(App *a, int delta)
{
    if (!a->view_len) return;
    long c = (long)a->cursor + delta;
    if (c < 0) c = 0;
    if (c >= (long)a->view_len) c = (long)a->view_len - 1;
    a->cursor = (size_t)c;
    app_scroll_to_cursor(a);
}

static void editor_key(Editor *e, const KeyEvent *ev)
{
    int ch = ev->key;
    switch (ch) {
    case KEY_BACKSPACE:
    case 127:
    case 8:
        ed_backspace(e);
        return;
    case KEY_DC:
        ed_delete(e);
        return;
    case KEY_LEFT:  ed_left(e);  return;
    case KEY_RIGHT: ed_right(e); return;
    case KEY_HOME:  ed_home(e);  return;
    case KEY_END:   ed_end(e);   return;
    case 1:  /* Ctrl-A */
        ed_home(e);
        return;
    case 5:  /* Ctrl-E */
        ed_end(e);
        return;
    case 23: /* Ctrl-W */
        ed_delete_word(e);
        return;
    case 21: /* Ctrl-U */
        ed_kill_to_start(e);
        return;
    case 11: /* Ctrl-K */
        ed_kill_to_end(e);
        return;
    default:
        break;
    }
    if (ev->text[0]) ed_insert(e, ev->text);
    else if (ch >= 32 && ch < 127) {
        char c = (char)ch;
        ed_insert(e, &c);
    }
}

static int handle_list_key(App *a, const KeyEvent *ev)
{
    int ch = ev->key;

    switch (ch) {
    case 'q': case 3: /* Ctrl-C */
        return 0;
    case 'j': case KEY_DOWN: case 16: /* Ctrl-N */
        move_cursor(a, 1);
        return 1;
    case 'k': case KEY_UP: case 14: /* Ctrl-P */
        move_cursor(a, -1);
        return 1;
    case KEY_NPAGE: case 4: /* Ctrl-D */
        move_cursor(a, (a->bottom - a->top) / 2);
        return 1;
    case KEY_PPAGE: case 21: /* Ctrl-U */
        move_cursor(a, -((a->bottom - a->top) / 2));
        return 1;
    case 'g': case KEY_HOME:
        goto_top(a);
        return 1;
    case 'G': case KEY_END:
        goto_bottom(a);
        return 1;
    case ' ':
    case KEY_ENTER:
    case '\r':
    case '\n':
        app_toggle_done(a);
        return 1;
    case 'a':
        app_start_form(a, -1);
        return 1;
    case 'e': {
        Task *t = app_current(a);
        if (!t) { app_set_status(a, "No task selected"); return 1; }
        app_start_form(a, t->id);
        return 1;
    }
    case 'd':
        if (!app_current(a)) { app_set_status(a, "No task selected"); return 1; }
        a->confirm_action = CONFIRM_DELETE;
        a->mode = MODE_CONFIRM;
        return 1;
    case 'X':
        a->confirm_action = CONFIRM_CLEAR_DONE;
        a->mode = MODE_CONFIRM;
        return 1;
    case 'p':
        app_cycle_priority(a);
        return 1;
    case '/':
        free(a->search_saved);
        a->search_saved = xstrdup(ed_text(&a->search));
        a->mode = MODE_SEARCH;
        return 1;
    case 'f':
        app_open_picker(a, PICK_FILTER);
        return 1;
    case 's':
        app_open_picker(a, PICK_SORT);
        return 1;
    case 't':
        app_open_picker(a, PICK_TAG);
        return 1;
    case 'u':
        app_undo(a);
        return 1;
    case 18: /* Ctrl-R */
        app_redo(a);
        return 1;
    case '?':
        a->mode = MODE_HELP;
        a->help_scroll = 0;
        return 1;
    case 27: /* Esc */
        if (*ed_text(&a->search)) {
            ed_clear(&a->search);
            app_rebuild_view(a);
            app_set_status(a, "Search cleared");
        } else if (a->tag_filter) {
            free(a->tag_filter);
            a->tag_filter = NULL;
            a->view_mode = VIEW_ALL;
            app_rebuild_view(a);
            app_set_status(a, "Tag filter cleared");
        }
        return 1;
    case 12: /* Ctrl-L */
        clear();
        return 1;
    default:
        return 1;
    }
}

static void handle_search_key(App *a, const KeyEvent *ev)
{
    switch (ev->key) {
    case '\r':
    case '\n':
    case KEY_ENTER:
        a->mode = MODE_LIST;
        app_rebuild_view(a);
        app_set_status(a, "Search: %s", ed_text(&a->search));
        return;
    case 27: /* Esc */
        ed_set(&a->search, a->search_saved);
        a->mode = MODE_LIST;
        app_rebuild_view(a);
        return;
    case 3: /* Ctrl-C */
        ed_set(&a->search, a->search_saved);
        a->mode = MODE_LIST;
        app_rebuild_view(a);
        return;
    default:
        editor_key(&a->search, ev);
        app_rebuild_view(a);
        return;
    }
}

static void handle_form_key(App *a, const KeyEvent *ev)
{
    Form *f = &a->form;
    int ch = ev->key;

    switch (ch) {
    case '\r':
    case '\n':
    case KEY_ENTER:
        app_form_commit(a);
        return;
    case 27: /* Esc */
        app_form_cancel(a);
        return;
    case 3: /* Ctrl-C */
        app_form_cancel(a);
        return;
    case '\t':
        f->field = (f->field + 1) % F_COUNT;
        return;
    case KEY_BTAB:
        f->field = (f->field + F_COUNT - 1) % F_COUNT;
        return;
    case KEY_UP:
        f->field = (f->field + F_COUNT - 1) % F_COUNT;
        return;
    case KEY_DOWN:
        f->field = (f->field + 1) % F_COUNT;
        return;
    case KEY_LEFT:
    case KEY_RIGHT:
        if (f->field == F_PRIO) {
            int dir = (ch == KEY_RIGHT) ? 1 : -1;
            f->prio = (f->prio + dir + 3) % 3;
            return;
        }
        editor_key(&f->ed[f->field], ev);
        return;
    default:
        if (f->field != F_PRIO) editor_key(&f->ed[f->field], ev);
        return;
    }
}

static int handle_picker_key(App *a, const KeyEvent *ev)
{
    Picker *p = &a->picker;
    switch (ev->key) {
    case '\r':
    case '\n':
    case KEY_ENTER:
        app_picker_apply(a);
        return 1;
    case 27:
    case 3:
        app_picker_cancel(a);
        return 1;
    case 'j': case KEY_DOWN:
        if (p->count) p->sel = (p->sel + 1) % (int)p->count;
        return 1;
    case 'k': case KEY_UP:
        if (p->count) p->sel = (p->sel + (int)p->count - 1) % (int)p->count;
        return 1;
    case 'g': case KEY_HOME:
        p->sel = 0;
        return 1;
    case 'G': case KEY_END:
        p->sel = p->count ? (int)p->count - 1 : 0;
        return 1;
    default:
        return 1;
    }
}

static int handle_help_key(App *a, const KeyEvent *ev)
{
    switch (ev->key) {
    case 'q':
    case '?':
    case 27:
    case 3:
        a->mode = MODE_LIST;
        return 1;
    case 'j': case KEY_DOWN:
        a->help_scroll++;
        return 1;
    case 'k': case KEY_UP:
        if (a->help_scroll > 0) a->help_scroll--;
        return 1;
    case 'g': case KEY_HOME:
        a->help_scroll = 0;
        return 1;
    case 'G': case KEY_END:
        a->help_scroll += 40;
        return 1;
    default:
        return 1;
    }
}

static int handle_confirm_key(App *a, const KeyEvent *ev)
{
    switch (ev->key) {
    case 'y': case 'Y':
        if (a->confirm_action == CONFIRM_CLEAR_DONE) app_clear_done(a);
        else app_delete_current(a);
        a->mode = MODE_LIST;
        return 1;
    case 'n': case 'N': case 27: case 3:
        a->mode = MODE_LIST;
        app_set_status(a, "Cancelled");
        return 1;
    default:
        return 1;
    }
}

int ui_handle_key(App *a, const KeyEvent *ev)
{
    switch (a->mode) {
    case MODE_LIST:    return handle_list_key(a, ev);
    case MODE_SEARCH:  handle_search_key(a, ev); return 1;
    case MODE_FORM:    handle_form_key(a, ev); return 1;
    case MODE_PICKER:  return handle_picker_key(a, ev);
    case MODE_HELP:    return handle_help_key(a, ev);
    case MODE_CONFIRM: return handle_confirm_key(a, ev);
    default:           return 1;
    }
}

/* --------------------------------------------------------------- init --- */

int ui_init(void)
{
    if (!initscr()) return -1;
    g_inited = 1;
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_colors();
    timeout(250);
    getmaxyx(stdscr, g_h, g_w);
    return 0;
}

void ui_shutdown(void)
{
    if (!g_inited) return;
    curs_set(1);
    endwin();
    g_inited = 0;
}
