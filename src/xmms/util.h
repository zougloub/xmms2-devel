#ifndef __XMMS_UTIL_H__

#include <stdio.h>

#define XMMS_PATH_MAX 255

#define DEBUG

#ifdef DEBUG
#define XMMS_DBG(fmt, args...) { \
    fprintf (stderr, __FILE__ ": "); \
    fprintf (stderr, fmt, ## args); \
    fprintf (stderr, "\n"); \
}
#else
#define XMMS_DBG(fmt,...)
#endif

#endif
