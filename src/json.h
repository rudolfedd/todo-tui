/* json.h — minimal, dependency-free JSON parser (values) and string writer. */
#ifndef TODO_JSON_H
#define TODO_JSON_H

#include "util.h"

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue {
    JsonType type;
    union {
        int         boolean;
        double      number;
        char       *string;
        struct { JsonValue **items; size_t count; } array;
        struct { char **keys; JsonValue **values; size_t count; } object;
    } u;
};

/* Parse `text`. On failure returns NULL and fills `err` (if not NULL). */
JsonValue *json_parse(const char *text, char *err, size_t errsz);

/* Object member lookup (NULL if absent or `obj` is not an object). */
JsonValue *json_get(const JsonValue *obj, const char *key);

/* Convenience accessors with defaults. */
const char *json_str(const JsonValue *v, const char *def);
double      json_num(const JsonValue *v, double def);
int         json_bool(const JsonValue *v, int def);

void json_free(JsonValue *v);

/* Append `s` as a quoted, escaped JSON string. */
void json_write_string(const char *s, Buf *out);

#endif /* TODO_JSON_H */
