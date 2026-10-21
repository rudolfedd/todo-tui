/* main.c — entry point. */
#include "app.h"
#include "store.h"
#include "ui.h"
#include "util.h"

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TODO_TUI_VERSION "1.0.0"

static volatile sig_atomic_t g_quit = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

static void usage(FILE *out)
{
    fprintf(out,
        "todo-tui %s — a keyboard driven todo list\n"
        "\n"
        "Usage: todo-tui [options]\n"
        "\n"
        "Options:\n"
        "  -c, --config PATH   use PATH as the database file\n"
        "  -h, --help          show this help and exit\n"
        "  -v, --version       show the version and exit\n"
        "\n"
        "Environment:\n"
        "  TODO_TUI_CONFIG     default database path\n"
        "\n"
        "Default database location:\n"
        "  Linux    $XDG_CONFIG_HOME/todo-tui/todos.json\n"
        "  Windows  %%APPDATA%%\\todo-tui\\todos.json\n",
        TODO_TUI_VERSION);
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    const char *config = getenv("TODO_TUI_CONFIG");

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(arg, "-v") || !strcmp(arg, "--version")) {
            printf("todo-tui %s\n", TODO_TUI_VERSION);
            return 0;
        }
        if (!strcmp(arg, "-c") || !strcmp(arg, "--config")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "todo-tui: --config needs a path\n");
                return 2;
            }
            config = argv[++i];
            continue;
        }
        if (!strncmp(arg, "--config=", 9)) {
            config = arg + 9;
            continue;
        }
        fprintf(stderr, "todo-tui: unknown option '%s'\n", arg);
        usage(stderr);
        return 2;
    }

    char *path = store_default_path();
    if (config && *config) {
        free(path);
        path = xstrdup(config);
    }

    App app;
    app_init(&app, path);
    free(path);

    char err[256];
    if (store_load(app.config_path, &app.tasks, err, sizeof err) != 0) {
        fprintf(stderr, "todo-tui: %s\n", err);
        fprintf(stderr, "todo-tui: starting with an empty list\n");
    }
    app_rebuild_view(&app);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (ui_init() != 0) {
        fprintf(stderr, "todo-tui: cannot initialise the terminal\n");
        app_free(&app);
        return 1;
    }

    app_set_status(&app, "%zu task(s) loaded — press ? for help", app.tasks.count);

    int running = 1;
    while (running && !g_quit) {
        KeyEvent ev;
        if (ui_read_key(&ev)) {
            running = ui_handle_key(&app, &ev);
        }
        app_tick_status(&app);
        ui_draw(&app);
    }

    ui_shutdown();

    if (app.dirty) app_save(&app);
    app_free(&app);
    return 0;
}
