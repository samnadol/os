#include "path.h"
#include "../../../hw/mem.h"
#include "../../../lib/string.h"

void path_init(Path *path) {
    path->count = 0;
    path->capacity = PATH_INITIAL_CAPACITY;
    path->components = (char **)malloc(PATH_INITIAL_CAPACITY * sizeof(char *));
}

void path_resize(Path *path) {
    size_t new_capacity = path->capacity * 2;
    char **new_components = (char **)malloc(new_capacity * sizeof(char *));
    
    for (size_t i = 0; i < path->count; i++) {
        new_components[i] = path->components[i];
    }

    mfree(path->components);
    path->components = new_components;
    path->capacity = new_capacity;
}

void path_add(Path *path, const char *component) {
    size_t length = 0;
    while (component[length] != '\0') length++;

    if (path->count >= path->capacity) {
        path_resize(path);
    }

    path->components[path->count] = (char *)malloc(length + 1);
    memcpy((void*)path->components[path->count], (void*)component, length);
    path->components[path->count][length] = '\0';

    path->count++;
}

void path_remove_last(Path *path) {
    if (path->count == 0) return;

    mfree(path->components[path->count - 1]);
    path->count--;
}

char *path_join(Path *path) {
    if (path->count == 0) {
        char *empty_path = (char *)malloc(2);
        empty_path[0] = '/';
        empty_path[1] = '\0';
        return empty_path;
    }

    size_t total_length = 1;
    for (size_t i = 0; i < path->count; i++) {
        size_t len = 0;
        while (path->components[i][len] != '\0') len++;
        total_length += len + 1;
    }

    char *full_path = (char *)malloc(total_length);
    full_path[0] = '/';
    size_t pos = 1;

    for (size_t i = 0; i < path->count; i++) {
        size_t len = 0;
        while (path->components[i][len] != '\0') len++;

        memcpy((void*)(full_path + pos), (void*)path->components[i], len);
        pos += len;
        
        if (i < path->count - 1) {
            full_path[pos] = '/';
            pos++;
        }
    }

    full_path[pos] = '\0';
    return full_path;
}

Path *path_split(char *path) {
    Path *result = (Path *)malloc(sizeof(Path));
    path_init(result); // Initialize the Path structure

    char component[PATH_MAX_COMPONENT_LENGTH];
    int comp_index = 0;

    while (*path) {
        if (*path == '/') {
            if (comp_index > 0) {
                component[comp_index] = '\0';
                path_add(result, component);
                comp_index = 0;
            }
        } else {
            if (comp_index < PATH_MAX_COMPONENT_LENGTH - 1) {
                component[comp_index++] = *path;
            }
        }
        path++;
    }

    if (comp_index > 0) {
        component[comp_index] = '\0';
        path_add(result, component);
    }

    return result;
}

void path_free(Path *path) {
    for (size_t i = 0; i < path->count; i++) {
        mfree(path->components[i]);
    }
    mfree(path->components);
}

Path *path_add_string(const char *input, Path *current_path) {
    if (input[0] == '/') {
        path_free(current_path);

        Path *new_path = (Path *)malloc(sizeof(Path));
        path_init(new_path);
        path_add(new_path, input);

        return new_path;
    } else {
        path_add(current_path, input);

        return current_path;
    }
}

Path *path_concatenate(Path *path1, Path *path2) {
    // Allocate a new Path structure to hold the concatenated result
    Path *new_path = (Path *)malloc(sizeof(Path));
    path_init(new_path);  // Initialize the new path

    // Add components of the first path
    for (size_t i = 0; i < path1->count; i++) {
        path_add(new_path, path1->components[i]);
    }

    // Add components of the second path, ensuring a '/' separator if needed
    for (size_t i = 0; i < path2->count; i++) {
        // Ensure the paths are properly separated by a '/'
        // if (i == 0 && new_path->count > 0) {
        //     path_add(new_path, "/"); // Add a separator between the paths
        // }
        path_add(new_path, path2->components[i]);
    }

    return new_path;
}

Path *path_duplicate(Path *original) {
    Path *new_path = (Path *)malloc(sizeof(Path));
    path_init(new_path);
    for (size_t i = 0; i < original->count; i++) {
        path_add(new_path, original->components[i]);
    }

    return new_path;
}

Path *path_get_final(Path *current, char *input)
{
    Path *new = path_split(input);

    if (input[0] == '/') {
        return new;
    }
    else
    {
        Path *old = path_duplicate(current);
        Path *final = path_concatenate(old, new);
        path_free(old);
        path_free(new);
        return final;
    }
}

Path *path_get_parent(Path *og)
{
    Path *parent = path_duplicate(og);
    path_remove_last(parent);
    return parent;
}