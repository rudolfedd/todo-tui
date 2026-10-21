# todo-tui

A fast, keyboard-driven todo list for the terminal. Written in C, no runtime
dependencies beyond ncurses (Linux/macOS) or PDCurses (Windows).

```
 todo-tui   3/8 done  view:active  sort:newest  find:milk
──────────────────────────────────────────────────────────────────────────
 > [ ] Buy milk                              #home #errands  HIGH  2026-10-24
   [x] Renew the domain                       #work          MED   2026-10-10
   [ ] Write the release notes                #work          LOW   2026-10-30
   [ ] Call the dentist                                     LOW   overdue 2026-10-19
──────────────────────────────────────────────────────────────────────────
 2/8  ####----   shown 4                          #2/4
 j/k move  space done  a add  e edit  d del  p prio  / find  f filter  …
```

## Features

- **CRUD** — add, edit, complete, delete, with a confirmation dialog
- **Filters** — all / active / done / today / overdue / by tag
- **Sorting** — newest, priority, due date, alphabetically (stable sort)
- **Incremental search** over titles, notes, tags and due dates
- **Tags, priorities and due dates** (overdue items are highlighted in red)
- **JSON storage** — human readable, atomic writes, portable between platforms
- **Undo / redo** — snapshot based, up to 120 steps
- **Live progress bar** and per-task counters in the status bar
- **Context sensitive help** (`?`), inline editing with a real cursor
- **Single binary**, zero runtime deps, C11, no memory leaks (ASan/UBSan clean)

## Install

### Linux / macOS

```sh
sudo apt install libncurses-dev make gcc   # Debian/Ubuntu
# Fedora: sudo dnf install ncurses-devel
# macOS:  brew install ncurses && brew link ncurses
```

```sh
git clone https://github.com/<you>/todo-tui.git
cd todo-tui
make
./todo-tui
```

`make install` copies the binary to `$PREFIX/bin` (default `/usr/local`).

### Windows

The easiest route is [MSYS2](https://www.msys2.org/) (it provides both the
compiler and PDCurses):

```sh
# In an MSYS2 "MSYS2 UCRT64" or "MINGW64" shell
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-pdcurses make
./build-msys2.sh
./todo-tui.exe
```

The script compiles everything into `todo-tui.exe`; the only runtime dependency
is `libpdcurses.dll`, which you can copy next to the executable or put on
`PATH`.

### Build options

```sh
make DEBUG=1     # -O0 -g with AddressSanitizer + UndefinedBehaviorSanitizer
make test        # run the unit tests
make clean
```

## Usage

```
todo-tui [options]

  -c, --config PATH   use PATH as the database file
  -h, --help          show help
  -v, --version       show the version
```

The database location defaults to:

| Platform | Path |
|---|---|
| Linux | `$XDG_CONFIG_HOME/todo-tui/todos.json` (`~/.config/todo-tui/todos.json`) |
| Windows | `%APPDATA%\todo-tui\todos.json` |

Override it with `--config` or the `TODO_TUI_CONFIG` environment variable.

## Key bindings

### Navigation

| Key | Action |
|---|---|
| `j` / `↓` | move down |
| `k` / `↑` | move up |
| `g` / `Home` | first task |
| `G` / `End` | last task |
| `Ctrl-D` / `PgDn` | half page down |
| `Ctrl-U` / `PgUp` | half page up |

### Tasks

| Key | Action |
|---|---|
| `Space` / `Enter` | toggle done |
| `a` | add task |
| `e` | edit task |
| `d` | delete task (asks first) |
| `p` | cycle priority (low → med → high) |
| `X` | remove all completed tasks (asks first) |

### Find and view

| Key | Action |
|---|---|
| `/` | incremental search |
| `f` | change filter |
| `s` | change sort order |
| `t` | filter by tag |
| `Esc` | clear the search / tag filter |

### Other

| Key | Action |
|---|---|
| `u` | undo |
| `Ctrl-R` | redo |
| `?` | toggle the help screen |
| `q` / `Ctrl-C` | quit |

While editing text: `Ctrl-W` deletes a word, `Ctrl-U` clears the line,
`Ctrl-A`/`Ctrl-E` jump to the start/end, `Tab` moves between form fields.

## Data format

The database is a single JSON file that is safe to edit by hand and to keep in
version control:

```json
{
  "version": 1,
  "next_id": 3,
  "tasks": [
    {
      "id": 1,
      "title": "Buy milk",
      "notes": "2.5%, two bottles",
      "done": false,
      "priority": "high",
      "created": 1761000000,
      "completed": 0,
      "tags": ["home", "errands"],
      "due": "2026-10-24"
    }
  ]
}
```

Writes are atomic: the new content is written to `todos.json.tmp` and then
renamed over the original, so an interrupted save never corrupts the file.

## Project layout

```
src/
  main.c            entry point, argument parsing, signal handling
  app.c/.h          application state: filters, sorting, undo/redo, actions
  ui.c/.h           curses rendering and key handling
  model.c/.h        the Task data model
  store.c/.h        locating, loading and saving the JSON database
  json.c/.h         a small dependency-free JSON parser and writer
  editor.c/.h       a UTF-8 aware single line editor
  util.c/.h         allocation helpers, dynamic buffers, string/UTF-8 utilities
  curses_compat.h   picks ncurses or PDCurses
tests/
  test_json.c       unit tests for the JSON parser and the helpers
```

The code is deliberately split so that the business logic (`app.c`, `model.c`,
`store.c`) has no dependency on curses — that keeps it testable and portable.

## Tests

```sh
make test
```

The suite covers the JSON parser (including escapes, surrogate pairs and
malformed input), the UTF-8 helpers and the list splitting logic.

## License

MIT — see [LICENSE](LICENSE).

---

## Русский

Терминальный todo-лист на C: ncurses на Linux/macOS, PDCurses на Windows.

**Установка**

```sh
# Linux
sudo apt install libncurses-dev make gcc
make && ./todo-tui

# Windows (MSYS2, MINGW64 shell)
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-pdcurses make
./build-msys2.sh && ./todo-tui.exe
```

**Горячие клавиши**

| Клавиши | Действие |
|---|---|
| `j` `k` `↑` `↓` | навигация по списку |
| `g` `G` | первая / последняя задача |
| `Space` | отметить выполненной |
| `a` `e` `d` | добавить / изменить / удалить |
| `p` | сменить приоритет |
| `/` `f` `s` `t` | поиск / фильтр / сортировка / теги |
| `u` `Ctrl-R` | undo / redo |
| `X` | убрать выполненные |
| `?` | справка |
| `q` | выход |

**Где хранится база**

| Платформа | Путь |
|---|---|
| Linux | `$XDG_CONFIG_HOME/todo-tui/todos.json` |
| Windows | `%APPDATA%\todo-tui\todos.json` |

Переопределяется флагом `--config PATH` или переменной `TODO_TUI_CONFIG`.
Файл в обычном JSON — его можно править руками и держать в git.
