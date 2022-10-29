#include "pyhacks.h"

typedef struct {
    PyObject_HEAD
    PyObject * buf;
    Py_ssize_t pos;
    Py_ssize_t string_size;
    PyObject * dict;
    PyObject * weakreflist;
    Py_ssize_t exports;
} bytesio;

bytesio_t * get_bytesio_object(PyObject * io_BytesIO)
{
    if (PY_MAJOR_VERSION != 3)
        return NULL;
    if (PY_MINOR_VERSION >= 8 && PY_MINOR_VERSION <= 11)
        return (bytesio_t *)io_BytesIO;
    return NULL;
}

#define SHARED_BUF(self) (Py_REFCNT((self)->buf) > 1)

static
int check_closed(bytesio * self)
{
    if (self->buf == NULL) {
        PyErr_SetString(PyExc_ValueError, "I/O operation on closed file.");
        return 1;
    }
    return 0;
}

static
int check_exports(bytesio * self)
{
    if (self->exports > 0) {
        PyErr_SetString(PyExc_BufferError, "Existing exports of data: object cannot be re-sized");
        return 1;
    }
    return 0;
}

static
int unshare_buffer(bytesio * self, size_t size)
{
    PyObject * new_buf;
    if (!SHARED_BUF(self))
        return -1;
    if (self->exports != 0)
        return -1;
    if (size < (size_t)self->string_size)
        return -1;
    new_buf = PyBytes_FromStringAndSize(NULL, size);
    if (new_buf == NULL)
        return -1;
    memcpy(PyBytes_AS_STRING(new_buf), PyBytes_AS_STRING(self->buf), self->string_size);
    Py_SETREF(self->buf, new_buf);
    return 0;
}

static
int resize_buffer(bytesio * self, size_t size)
{
    size_t alloc = PyBytes_GET_SIZE(self->buf);
    if (self->buf == NULL)
        return -1;

    if (size > PY_SSIZE_T_MAX)
        goto overflow;

    if (size < alloc / 2) {
        /* Major downsize; resize down to exact size. */
        alloc = size + 1;
    }
    else if (size < alloc) {
        /* Within allocated size; quick exit */
        return 0;
    }
    else if (size <= alloc * 1.125) {
        /* Moderate upsize; overallocate similar to list_resize() */
        alloc = size + (size >> 3) + (size < 9 ? 3 : 6);
    }
    else {
        /* Major upsize; resize up to exact size */
        alloc = size + 1;
    }

    if (alloc > ((size_t)-1) / sizeof(char))
        goto overflow;

    if (SHARED_BUF(self)) {
        if (unshare_buffer(self, alloc) < 0)
            return -1;
    }
    else {
        if (_PyBytes_Resize(&self->buf, alloc) < 0)
            return -1;
    }
    return 0;

overflow:
    PyErr_SetString(PyExc_OverflowError, "new buffer size too large");
    return -1;
}

static
Py_ssize_t write_bytes(bytesio * self, const char * bytes, Py_ssize_t len)
{
    size_t endpos;
    if (self->buf == NULL || self->pos < 0 || len < 0)
        return -1;

    if (check_closed(self)) {
        return -1;
    }
    if (check_exports(self)) {
        return -1;
    }

    endpos = (size_t)self->pos + len;
    if (endpos > (size_t)PyBytes_GET_SIZE(self->buf)) {
        if (resize_buffer(self, endpos) < 0)
            return -1;
    }
    else if (SHARED_BUF(self)) {
        if (unshare_buffer(self, Py_MAX(endpos, (size_t)self->string_size)) < 0)
            return -1;
    }

    if (self->pos > self->string_size) {
        char * dst = PyBytes_AS_STRING(self->buf) + self->string_size;
        memset(dst, '\0', (self->pos - self->string_size) * sizeof(char));
    }

    /* Copy the data to the internal buffer, overwriting some of the existing
    data if self->pos < self->string_size. */
    memcpy(PyBytes_AS_STRING(self->buf) + self->pos, bytes, len);
    self->pos = endpos;

    /* Set the new length of the internal string if it has changed. */
    if ((size_t)self->string_size < endpos) {
        self->string_size = endpos;
    }
    return len;
}

Py_ssize_t io_BytesIO_write_bytes(bytesio_t * self, const char * bytes, Py_ssize_t len)
{
    if (len == 0)
        return 0;

    return write_bytes((bytesio *)self, bytes, len);
}

PyObject * _io_BytesIO_write(bytesio * self, PyObject * b)
{
    Py_ssize_t n = 0;
    Py_buffer buf;

    if (check_closed(self)) {
        return NULL;
    }
    if (check_exports(self)) {
        return NULL;
    }
    if (PyObject_GetBuffer(b, &buf, PyBUF_CONTIG_RO) < 0)
        return NULL;

    if (buf.len != 0)
        n = write_bytes(self, buf.buf, buf.len);

    PyBuffer_Release(&buf);
    return n >= 0 ? PyLong_FromSsize_t(n) : NULL;
} 

