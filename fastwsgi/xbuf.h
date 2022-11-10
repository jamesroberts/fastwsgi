#ifndef FASTWSGI_XBUF_H_
#define FASTWSGI_XBUF_H_

#include "common.h"

typedef struct {
    char * data;
    int size;
    int capacity;
} xbuf_t;


static
char * xbuf_resize(xbuf_t * buf, size_t need_size, int addon)
{
    size_t new_cap = need_size;
    if (addon) {
        if (new_cap < 1*1024*1024) {
            new_cap *= 2;
        } else {
            new_cap += 512*1024;
        }
    }
    if ((new_cap & 1) == 0)
        new_cap += 1;  // make uneven value

    char * new_ptr = (char *)malloc(new_cap + 2);
    if (new_ptr) {
        new_ptr[0] = 0;
        if (buf->data) {
            if (buf->size) {
                memcpy(new_ptr, buf->data, buf->size);
                new_ptr[buf->size] = 0;
            }    
            if (buf->capacity & 1)
                free(buf->data);
        }
        buf->data = new_ptr;
        buf->capacity = (int)new_cap;
    }
    return new_ptr;
}

INLINE
static
char * xbuf_expand(xbuf_t * buf, size_t expand_size)
{
    const size_t need_size = buf->size + expand_size + 8;
    if ((ssize_t)need_size >= (ssize_t)buf->capacity) {
        if (xbuf_resize(buf, need_size, 1) == NULL)
            return NULL; // error
    }
    return buf->data + buf->size;
}

static
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

static
int xbuf_init_str(xbuf_t * buf, const char * str)
{
    return xbuf_init(buf, str, strlen(str));
}

static
int xbuf_init2(xbuf_t * buf, void * data, size_t capacity)
{
    buf->data = data;  // using preallocated buffer
    buf->size = 0;
    buf->capacity = (capacity & 1) ? capacity - 3 : capacity - 2; // make even value
    buf->data[0] = 0;
    return 0;
}

static
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
static
int xbuf_add_str(xbuf_t * buf, const char * str)
{
    return xbuf_add(buf, str, strlen(str));
}

INLINE
static
void xbuf_reset(xbuf_t * buf)
{
    buf->size = 0;
    if (buf->data)
        buf->data[0] = 0;
}

INLINE
static
void xbuf_free(xbuf_t * buf)
{
    if (buf->data)
        if (buf->capacity & 1)  // skip if preallocated buffer
            free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

#endif
