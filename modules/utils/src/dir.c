#if defined(_WIN32)
/* no POSIX feature-test macros needed on Windows */
#elif defined(__APPLE__)
#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 700
#else
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "blue-bird/utils/dir.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#endif

/* ---------- Growable name list ---------- */

typedef struct {
    char **names;
    size_t count;
    size_t cap;
} name_list_t;

static int name_list_add(name_list_t *list, const char *name)
{
    if (list->count == list->cap)
    {
        size_t new_cap = list->cap ? list->cap * 2 : 16;
        char **grown = realloc(list->names, new_cap * sizeof *grown);

        if (!grown)
            return -1;

        list->names = grown;
        list->cap = new_cap;
    }

    size_t len = strlen(name);
    char *copy = malloc(len + 1);

    if (!copy)
        return -1;

    memcpy(copy, name, len + 1);
    list->names[list->count++] = copy;
    return 0;
}

static int is_dot_entry(const char *name)
{
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static int compare_names(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* ---------- Public API ---------- */

void bb_dir_list_free(char **names, size_t count)
{
    if (!names)
        return;

    for (size_t i = 0; i < count; i++)
        free(names[i]);

    free(names);
}

int bb_dir_list(const char *path, char ***out_names, size_t *out_count)
{
    if (!path || !out_names || !out_count)
        return -1;

    *out_names = NULL;
    *out_count = 0;

    name_list_t list = { NULL, 0, 0 };

#if defined(_WIN32)

    size_t path_len = strlen(path);
    int has_sep = path_len > 0 && (path[path_len - 1] == '/' || path[path_len - 1] == '\\');

    /* "<path>\*" (or "<path>*" when path already ends in a separator). */
    char *pattern = malloc(path_len + 3);

    if (!pattern)
        return -1;

    memcpy(pattern, path, path_len);
    pattern[path_len] = '\0';

    if (!has_sep)
        strcat(pattern, "\\");

    strcat(pattern, "*");

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    free(pattern);

    if (h == INVALID_HANDLE_VALUE)
        return -1;

    do
    {
        if (is_dot_entry(fd.cFileName))
            continue;

        if (name_list_add(&list, fd.cFileName) != 0)
        {
            FindClose(h);
            bb_dir_list_free(list.names, list.count);
            return -1;
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);

#else

    DIR *dir = opendir(path);

    if (!dir)
        return -1;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (is_dot_entry(entry->d_name))
            continue;

        if (name_list_add(&list, entry->d_name) != 0)
        {
            closedir(dir);
            bb_dir_list_free(list.names, list.count);
            return -1;
        }
    }

    closedir(dir);

#endif

    if (list.count > 1)
        qsort(list.names, list.count, sizeof *list.names, compare_names);

    *out_names = list.names;
    *out_count = list.count;
    return 0;
}
