/* model.h — task data model. */
#ifndef TODO_MODEL_H
#define TODO_MODEL_H

#include <stddef.h>

typedef enum {
    PRIO_LOW = 0,
    PRIO_MED = 1,
    PRIO_HIGH = 2
} Priority;

const char *prio_name(Priority p);
Priority   prio_from_name(const char *name); /* accepts low/med/high/none, case-insensitive */

typedef struct {
    int        id;
    char      *title;
    char      *notes;
    int        done;
    Priority   prio;
    long long  created;
    long long  completed;   /* 0 = not completed */
    char      *tags;        /* comma separated */
    char      *due;         /* "YYYY-MM-DD" or "" */
} Task;

typedef struct {
    Task  *items;
    size_t count;
    size_t cap;
    int    next_id;
} TaskList;

void tasklist_init(TaskList *l);
void tasklist_free(TaskList *l);
void tasklist_clear(TaskList *l);

/* Appends a copy of `t`; returns the index of the new item. */
size_t tasklist_append(TaskList *l, const Task *t);
/* Removes and frees item at `idx`. */
void tasklist_remove(TaskList *l, size_t idx);

Task *tasklist_find_by_id(TaskList *l, int id);

/* Deep copy: `dst` must be initialised (it is cleared first). */
void tasklist_copy(const TaskList *src, TaskList *dst);

void task_free_strings(Task *t);

/* Number of comma separated tags in a task. */
size_t task_tag_count(const Task *t);
/* Get tag `i` of a task (caller frees the returned string). */
char  *task_tag_at(const Task *t, size_t i);
/* Does the task carry the given tag? (case-insensitive) */
int   task_has_tag(const Task *t, const char *tag);

#endif /* TODO_MODEL_H */
