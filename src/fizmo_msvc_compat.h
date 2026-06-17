/*
 * fizmo_msvc_compat.h
 *
 * Force-included (cl /FI) compatibility shim for building libfizmo + the
 * desktop bridge under MSVC. Only active for MSVC (_MSC_VER); a no-op
 * elsewhere so the file is safe to share. Companion header shims live in
 * src/msvc_shims/ (unistd.h, strings.h, dirent.h) reached via the include path.
 */
#ifndef FIZMO_MSVC_COMPAT_H
#define FIZMO_MSVC_COMPAT_H

#ifdef _MSC_VER

#include <string.h>
#include <stdlib.h>

/*
 * Locale declarations that libfizmo's placeholder locales/libfizmo_locales.h
 * does not provide. fizmo.c references locale_module_libfizmo and
 * init_libfizmo_locales(), which are defined in src/fizmo_locale_stubs.c. The
 * GCC/QUL build force-includes src/fizmo_embedded_compat.h for the same reason;
 * we declare them here so MSVC (strict C, no implicit declarations) sees them.
 */
#include "tools/i18n.h"
extern locale_module locale_module_libfizmo;
void init_libfizmo_locales(void);

#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

typedef int uid_t;

static inline uid_t getuid(void) { return 0; }

struct passwd {
    char *pw_name;
    char *pw_dir;
};
static inline struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    static struct passwd pw = { (char *)"windows", 0 };
    return &pw;
}

#endif /* _MSC_VER */
#endif /* FIZMO_MSVC_COMPAT_H */
