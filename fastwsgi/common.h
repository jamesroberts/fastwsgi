#ifndef FASTWSGI_COMMON_H_
#define FASTWSGI_COMMON_H_

#include <Python.h>
#include "uv.h"
#include "uv-common.h"
#include <stdbool.h>

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

#define _max(a,b) (((a) > (b)) ? (a) : (b))
#define _min(a,b) (((a) < (b)) ? (a) : (b))

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


#define PYTYPE_ERR(...) \
    do { \
        LOGc(__VA_ARGS__); \
        PyErr_Format(PyExc_TypeError, __VA_ARGS__); \
    } while(0)


void logrepr(int level, PyObject * obj);
#define LOGREPR(_level_, _msg_) if (g_log_level >= _level_) logrepr(_level_, _msg_)

int64_t get_env_int(const char * name);

const char * get_http_status_name(int status);

int64_t get_obj_attr_int(PyObject * obj, const char * name);
const char * get_obj_attr_str(PyObject * obj, const char * name);

int get_asctime(char ** asc_time);

INLINE
bool is_stream_notconn(uv_stream_t * handle)
{
    if (!(handle->flags & UV_HANDLE_WRITABLE) ||
        handle->flags & UV_HANDLE_SHUTTING ||
        uv__is_closing(handle)) {
        return true;
    }
    return false;
}

#endif
