#ifndef PATH_H
#define PATH_H

#include "../../../lib/string.h"

#define PATH_INITIAL_CAPACITY 4
#define PATH_MAX_COMPONENTS 16
#define PATH_MAX_COMPONENT_LENGTH 255

typedef struct {
    char **components;
    size_t count;
    size_t capacity;
} Path;

void path_init(Path *path);
void path_resize(Path *path);
void path_add(Path *path, const char *component);
void path_remove_last(Path *path);
char *path_join(Path *path);
Path *path_split(char *path);
void path_free(Path *path);
Path *path_add_string(const char *input, Path *current_path);
Path *path_concatenate(Path *path1, Path *path2);
Path *path_duplicate(Path *original);

Path *path_get_final(Path *current, char *input);
Path *path_get_parent(Path *og);

#endif
