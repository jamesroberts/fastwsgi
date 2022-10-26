#ifndef FASTWSGI_COMMON_H_
#define FASTWSGI_COMMON_H_

#include <Python.h>
#include "uv.h"
#include "uv-common.h"

#if defined(_MSC_VER)
#define vsnprintf _vsnprintf
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#if defined(_MSC_VER)
# define INLINE __inline
#elif defined(__GNUC__) || defined(__MVS__)
# define INLINE __inline__
#else
# define INLINE inline
#endif

typedef union {
    struct sockaddr_storage storage;
    struct sockaddr addr; 
    struct sockaddr_in in4;
    struct sockaddr_in6 in6;
} sockaddr_t;


// forced use this only for alpha version!
#ifndef FASTWSGI_DEBUG
#define FASTWSGI_DEBUG
#endif
#include "logx.h"

#endif
