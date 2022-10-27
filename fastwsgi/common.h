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


static
int64_t get_env_int(const char * name)
{
    int64_t v;
    char buf[128];
    size_t len = sizeof(buf) - 1;
    int rv = uv_os_getenv(name, buf, &len);
    if (rv == UV_ENOENT)
        return -1;  // env not found
    if (rv != 0 || len == 0)
        return -2;
    if (len == 1 && buf[0] == '0')
        return 0;
    if (len > 2 && buf[0] == '0' && buf[1] == 'x') {
        v = strtoll(buf + 2, NULL, 16);
    } else {
        v = strtoll(buf, NULL, 10);
    }
    if (v <= 0 || v == LLONG_MAX)
        return -3;
    return v;
}

#endif
