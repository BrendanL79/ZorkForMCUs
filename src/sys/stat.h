/*
 * sys/stat.h - Minimal stub for embedded ARM builds
 * 
 * This stub header prevents compilation errors when libfizmo includes <sys/stat.h>.
 * The stat functions are not actually used when DISABLE_CONFIGFILES=1.
 */

#ifndef _SYS_STAT_H_STUB
#define _SYS_STAT_H_STUB

#ifdef __ARM_EABI__

/* Minimal type/constant definitions */
struct stat {
    long st_mode;
    long st_ctime;
};

#define S_IFDIR  0040000
#define S_IRWXU  00700

/* Function declarations (implementations not needed - unused when DISABLE_CONFIGFILES=1) */
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int mkdir(const char *path, int mode);

#elif defined(_MSC_VER)

/*
 * MSVC desktop build: this stub shadows the real <sys/stat.h> because src/ is
 * on the include path ahead of the UCRT, and MSVC has no #include_next to fall
 * through to the real header. So map the POSIX names libfizmo uses onto the
 * UCRT's 64-bit equivalents (_stat64 / _fstat64) and declare the UCRT _stat64
 * layout *exactly*. A hand-rolled struct is ABI-unsafe: the UCRT's st_size is
 * 64-bit, so declaring it as `long` (32-bit) shifts st_mtime/st_ctime and the
 * CRT writes past/into the wrong fields.
 */
#include <io.h>          /* _open, _close, _fileno */
#include <fcntl.h>       /* _O_RDONLY */
#include <direct.h>      /* _mkdir */

/* Matches the UCRT <sys/stat.h> definition of struct _stat64 (which we shadow).
 * Guarded so it never collides if a real declaration is ever pulled in. */
#ifndef _STAT64_SHIM_DEFINED
#define _STAT64_SHIM_DEFINED
struct _stat64 {
    unsigned int   st_dev;
    unsigned short st_ino;
    unsigned short st_mode;
    short          st_nlink;
    short          st_uid;
    short          st_gid;
    unsigned int   st_rdev;
    __int64        st_size;
    __int64        st_atime;
    __int64        st_mtime;
    __int64        st_ctime;
};
int __cdecl _stat64(const char *_Path, struct _stat64 *_Stat);
int __cdecl _fstat64(int _FileHandle, struct _stat64 *_Stat);
#endif

/* libfizmo's filesys_c.c uses the POSIX names; route them to the 64-bit CRT
 * entry points so the struct above matches what the CRT actually writes. */
#define stat  _stat64
#define fstat _fstat64

#ifndef S_IFMT
#define S_IFMT  0xF000
#endif
#ifndef S_IFDIR
#define S_IFDIR 0x4000
#endif
#ifndef S_IRWXU
#define S_IRWXU 0x01C0
#endif

#else
/* On non-ARM, non-MSVC platforms, use the real sys/stat.h */
#include_next <sys/stat.h>
#endif

#endif /* _SYS_STAT_H_STUB */
