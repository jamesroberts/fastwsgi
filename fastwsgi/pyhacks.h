#ifndef FASTWSGI_PYHACKS_H_
#define FASTWSGI_PYHACKS_H_

#include <Python.h>

typedef struct bytesio bytesio_t;

bytesio_t * get_bytesio_object(PyObject * io_BytesIO);
Py_ssize_t io_BytesIO_write_bytes(bytesio_t * bio, const char * bytes, Py_ssize_t len);

#endif
