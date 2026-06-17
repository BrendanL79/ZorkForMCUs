#ifndef MSVC_SHIM_DIRENT_H
#define MSVC_SHIM_DIRENT_H

/*
 * Minimal POSIX <dirent.h> shim for MSVC. libfizmo's filesys_c.c uses
 * opendir/readdir/closedir for directory scanning (config/locale discovery).
 * DIR is an opaque type here; the concrete struct and a Win32 FindFirstFile
 * backing live in dirent_msvc.c.
 */
typedef struct DIR DIR;

struct dirent {
    char *d_name;
};

DIR *opendir(const char *name);
int closedir(DIR *d);
struct dirent *readdir(DIR *d);
void rewinddir(DIR *d);

#endif
