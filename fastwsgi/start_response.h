#ifndef START_RESPONSE_H_
#define START_RESPONSE_H_

#include "common.h"

typedef struct {
    PyObject ob_base;
    PyObject* status;
    PyObject* headers;
    PyObject* exc_info;
    int called;
} StartResponse;

extern PyTypeObject StartResponse_Type;

INLINE
static StartResponse * create_start_response(void)
{
    StartResponse * response = PyObject_NEW(StartResponse, &StartResponse_Type);
    response->status = NULL;
    response->headers = NULL;
    response->exc_info = NULL;
    response->called = 0;
    return response;
}

void set_status_error();
void set_header_tuple_error();
void set_header_list_error(PyObject* headers);
void set_exc_info_type_error(PyObject* exc_info);

#endif
