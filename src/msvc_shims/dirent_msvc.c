/*
 * dirent_msvc.c
 *
 * Minimal Win32-backed implementation of the POSIX directory API
 * (opendir/readdir/closedir) for building libfizmo's filesys_c.c under MSVC.
 * Only the surface libfizmo calls is provided. Compiled only for _MSC_VER.
 */
#ifdef _MSC_VER

#include "dirent.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct DIR {
    HANDLE          handle;     /* INVALID_HANDLE_VALUE until first read */
    WIN32_FIND_DATAA find_data; /* most recently found entry */
    int             pending;    /* 1 if find_data holds an unconsumed entry */
    struct dirent   entry;      /* returned to caller */
    char            pattern[MAX_PATH];
};

DIR *opendir(const char *name) {
    DIR *d;
    size_t len;

    if (name == NULL || name[0] == '\0')
        return NULL;

    d = (DIR *)calloc(1, sizeof(*d));
    if (d == NULL)
        return NULL;

    /* Build "<name>\*" search pattern. */
    len = strlen(name);
    if (len + 3 > sizeof(d->pattern)) {
        free(d);
        return NULL;
    }
    memcpy(d->pattern, name, len + 1);
    if (len > 0 && d->pattern[len - 1] != '\\' && d->pattern[len - 1] != '/')
        strcat(d->pattern, "\\");
    strcat(d->pattern, "*");

    d->handle = FindFirstFileA(d->pattern, &d->find_data);
    if (d->handle == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }
    d->pending = 1;
    return d;
}

struct dirent *readdir(DIR *d) {
    if (d == NULL)
        return NULL;

    if (!d->pending) {
        if (!FindNextFileA(d->handle, &d->find_data))
            return NULL;
    }
    d->pending = 0;

    d->entry.d_name = d->find_data.cFileName;
    return &d->entry;
}

int closedir(DIR *d) {
    if (d == NULL)
        return -1;
    if (d->handle != INVALID_HANDLE_VALUE)
        FindClose(d->handle);
    free(d);
    return 0;
}

void rewinddir(DIR *d) {
    if (d == NULL)
        return;
    if (d->handle != INVALID_HANDLE_VALUE)
        FindClose(d->handle);
    d->handle = FindFirstFileA(d->pattern, &d->find_data);
    d->pending = (d->handle != INVALID_HANDLE_VALUE);
}

#endif /* _MSC_VER */
