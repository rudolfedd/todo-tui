/* store.h — persistence: locate, load and save the task database (JSON). */
#ifndef TODO_STORE_H
#define TODO_STORE_H

#include "model.h"

#include <stddef.h>

/* Platform-appropriate default path for the database.
   Linux:   $XDG_CONFIG_HOME/todo-tui/todos.json  (or ~/.config/...)
   Windows: %APPDATA%\todo-tui\todos.json
   The returned string is malloc'ed; the caller frees it. */
char *store_default_path(void);

/* Ensure the parent directory of `path` exists. Returns 0 on success. */
int store_ensure_dir(const char *path, char *err, size_t errsz);

/* Load tasks from `path`. Returns 0 on success, -1 on failure (err filled). */
int store_load(const char *path, TaskList *out, char *err, size_t errsz);

/* Save tasks to `path` atomically (write to temp + rename). */
int store_save(const char *path, const TaskList *list, char *err, size_t errsz);

#endif /* TODO_STORE_H */
