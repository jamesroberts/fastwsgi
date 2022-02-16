#ifndef START_RESPONSE_H_
#define START_RESPONSE_H_
#include <Python.h>

typedef struct {
    PyObject ob_base;
    PyObject* status;
    PyObject* headers;
    PyObject* exc_info;
    int called;
} StartResponse;

PyTypeObject StartResponse_Type;

void set_status_error();
void set_header_tuple_error();
void set_header_list_error(PyObject* headers);
void exc_info_error(PyObject* exc_info);

#endif
