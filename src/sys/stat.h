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
 * on the include path ahead of the UCRT. MSVC has no #include_next, so map the
 * POSIX names libfizmo uses onto the UCRT's _-prefixed equivalents. <sys/stat.h>
 * here would re-enter this stub, so pull the real struct stat / _stat / _fstat
 * declarations in via the lower-level CRT headers that do not route through it.
 */
#include <io.h>          /* _fstat, _open, _close, _fileno */
#include <fcntl.h>       /* _O_RDONLY */
#include <direct.h>      /* _mkdir */
#include <corecrt_io.h>

/* The UCRT declares struct _stat / _fstat in <sys/stat.h>, which we shadow.
 * Re-declare the minimal surface libfizmo needs, matching the UCRT layout. */
#ifndef _STAT_DEFINED
struct stat {
    unsigned int   st_dev;
    unsigned short st_ino;
    unsigned short st_mode;
    short          st_nlink;
    short          st_uid;
    short          st_gid;
    unsigned int   st_rdev;
    long           st_size;
    long long      st_atime;
    long long      st_mtime;
    long long      st_ctime;
};
int fstat(int fd, struct stat *buf);
int stat(const char *path, struct stat *buf);
#endif

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
