# ┌─ todo-tui /

## ├─ What is it
      Fast, keyboard-driven todo list for the terminal.
      Written in C: ncurses on Linux/macOS, PDCurses on Windows.

## ├─ Features
      ✅ Add, edit, complete and delete tasks with confirmation
      🔎 Incremental search over titles, notes, tags and due dates
      🏷️ Tags, priorities (low / med / high) and due dates, overdue in red
      🎚️ Filters — all, active, done, today, overdue, by tag
      ↕️ Sorting — newest, priority, due date, alphabetically
      💾 JSON storage — human readable, atomic writes, portable
      ↩️ Undo / redo, up to 120 steps
      📊 Live progress bar and per-task counters
      ❓ Context sensitive help (`?`), inline editing with a real cursor
      📦 Single binary, no runtime dependencies, ASan/UBSan clean

## ├─ How build
 1. install `gcc`, `make` and ncurses — `libncurses-dev` (Debian/Ubuntu),
    `ncurses-devel` (Fedora), `brew install ncurses` (macOS)
 2. `git clone https://github.com/rudolfedd/todo-tui.git && cd todo-tui`
 3. `make` — build `./todo-tui`
 4. `make test` — run the unit tests
 5. `sudo make install` — install into `$PREFIX/bin`
 6. `make DEBUG=1` — build with AddressSanitizer + UndefinedBehaviorSanitizer

      Windows — MSYS2 (MINGW64) shell:
      `pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-pdcurses make`
      `./build-msys2.sh` — builds `todo-tui.exe` (needs `libpdcurses.dll`)

## ├─ How run
      todo-tui [options]

      -c, --config PATH   database file
      -h, --help          show help
      -v, --version       show version

      Linux   — $XDG_CONFIG_HOME/todo-tui/todos.json  (~/.config/todo-tui/todos.json)
      Windows — %APPDATA%\todo-tui\todos.json
      Override with `--config PATH` or `TODO_TUI_CONFIG`.

## ├─ Keys
      j  ↓         — move down
      k  ↑         — move up
      g  Home      — first task
      G  End       — last task
      ^D  PgDn     — half page down
      ^U  PgUp     — half page up
      Space  Enter — toggle done
      a            — add task
      e            — edit task
      d            — delete task (asks first)
      p            — cycle priority low → med → high
      X            — remove all completed tasks (asks first)
      /            — incremental search
      f            — change filter
      s            — change sort order
      t            — filter by tag
      Esc          — clear the search / tag filter
      u            — undo
      ^R           — redo
      ?            — toggle the help screen
      q  ^C        — quit

      While editing: ^W delete word, ^U clear line, ^A / ^E jump to start / end,
      Tab moves between form fields.

## ├─ Data format
      One JSON file, safe to edit by hand and to keep in version control.
      Writes are atomic: the new content goes to `todos.json.tmp` and is then
      renamed over the original, so an interrupted save never corrupts the file.

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

## ├─ Project structure
      todo-tui/
      ├── src/
      │   ├── main.c            — entry point, argument parsing, signals
      │   ├── app.c/.h          — app state: filters, sorting, undo/redo, actions
      │   ├── ui.c/.h           — curses rendering and key handling
      │   ├── model.c/.h        — the Task data model
      │   ├── store.c/.h        — locating, loading and saving the JSON database
      │   ├── json.c/.h         — dependency-free JSON parser and writer
      │   ├── editor.c/.h       — UTF-8 aware single line editor
      │   ├── util.c/.h         — allocation helpers, buffers, string/UTF-8 utils
      │   └── curses_compat.h   — picks ncurses or PDCurses
      ├── tests/
      │   ├── test_json.c       — JSON parser and helper tests
      │   └── test_store.c      — storage tests
      ├── Makefile
      ├── CMakeLists.txt
      ├── build-msys2.sh
      ├── LICENSE
      └── README.md

## ├─ Русский
      Терминальный todo-лист на C: ncurses на Linux/macOS, PDCurses на Windows.

      `make && ./todo-tui` — Linux.
      `pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-pdcurses make`
      и `./build-msys2.sh` — Windows, MSYS2 shell (MINGW64).

      j k ↑ ↓ — навигация, g G — первая / последняя задача, Space — отметить
      выполненной, a e d — добавить / изменить / удалить, p — приоритет,
      / f s t — поиск / фильтр / сортировка / теги, u ^R — undo / redo,
      X — убрать выполненные, ? — справка, q — выход.

      База — обычный JSON: $XDG_CONFIG_HOME/todo-tui/todos.json на Linux,
      %APPDATA%\todo-tui\todos.json на Windows. Переопределяется `--config PATH`
      или переменной `TODO_TUI_CONFIG`.

## └─ Author = rudolfedd
