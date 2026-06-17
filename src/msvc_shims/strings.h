#ifndef MSVC_SHIM_STRINGS_H
#define MSVC_SHIM_STRINGS_H
#include <string.h>
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#endif
