#ifndef FASTWSGI_XBUF_H_
#define FASTWSGI_XBUF_H_

#include "common.h"

typedef struct {
    char * data;
    int size;
    int capacity;
} xbuf_t;


INLINE
char * xbuf_expand(xbuf_t * buf, size_t expand_size)
{
    int need_size = buf->size + (int)expand_size + 8;
    if (need_size >= buf->capacity) {
        int new_cap = need_size;
        if (new_cap < 1*1024*1024) {
            new_cap *= 2;
        } else {
            new_cap += 512*1024;
        }
        char * new_ptr = (char *)malloc(new_cap);
        if (!new_ptr)
            return NULL; // error
        new_ptr[0] = 0;
        if (buf->data) {
            if (buf->size) {
                memcpy(new_ptr, buf->data, buf->size);
                new_ptr[buf->size] = 0;
            }    
            free(buf->data);
        }
        buf->data = new_ptr;
        buf->capacity = new_cap;
    }
    return buf->data + buf->size;
}

INLINE
int xbuf_init(xbuf_t * buf, const void * data, size_t size)
{
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
    if (size > 0) {
        char * ptr = xbuf_expand(buf, size);
        if (!ptr)
            return -1;  // error
        if (data) {
            memcpy(buf->data, data, size);
            buf->data[size] = 0;
            buf->size = size;
        }
    }
    return 0;
}

INLINE
int xbuf_init_str(xbuf_t * buf, const char * str)
{
    return xbuf_init(buf, str, strlen(str));
}

INLINE
int xbuf_add(xbuf_t * buf, const void * data, size_t size)
{
    char * ptr = xbuf_expand(buf, size);
    if (!ptr)
        return -1;  // error
    if (data && size) {
        memcpy(ptr, data, size);
        ptr[size] = 0;
        buf->size += size;
    }
    return buf->size;
}

INLINE
int xbuf_add_str(xbuf_t * buf, const char * str)
{
    return xbuf_add(buf, str, strlen(str));
}

INLINE
void xbuf_free(xbuf_t * buf)
{
    if (buf->data)
        free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

#endif
