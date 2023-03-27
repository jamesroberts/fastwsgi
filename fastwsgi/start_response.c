#include <stdbool.h>
#include "start_response.h"

void set_status_error()
{
    const char * err_msg = "'status' must be a 3-digit string";
    PyErr_SetString(PyExc_ValueError, err_msg);
}

void set_header_tuple_error()
{
    const char * err_msg = "start_response: argument 2 expects a list of 2-tuples (str, str)";
    PyErr_SetString(PyExc_TypeError, err_msg);
}

void set_header_list_error(PyObject * headers)
{
    const char * err_msg = "start_response: argument 2 expects a list of 2-tuples, got '%s' instead.";
    PyErr_Format(PyExc_TypeError, err_msg, Py_TYPE(headers)->tp_name);
}

void set_exc_info_type_error(PyObject * exc_info)
{
    const char * err_msg = "start_response: argument 3 expects a 3-tuple, got '%s' instead.";
    PyErr_Format(PyExc_TypeError, err_msg, Py_TYPE(exc_info)->tp_name);
}

void set_exc_info_missing_error()
{
    char* err_msg = "'exc_info' is required in the second call of 'start_response'";
    PyErr_SetString(PyExc_TypeError, err_msg);
}

static
bool is_valid_status(PyObject * status)
{
    if (PyUnicode_Check(status))
        if (PyUnicode_GET_LENGTH(status) >= 3)
            return true;

    set_status_error();
    return false;
}

static
bool is_valid_header_tuple(PyObject * tuple)
{
    if (PyTuple_Check(tuple))
        if (PyTuple_GET_SIZE(tuple) == 2)
            if (PyUnicode_Check(PyTuple_GET_ITEM(tuple, 0)))
                if (PyUnicode_Check(PyTuple_GET_ITEM(tuple, 1)))
                    return true;

    set_header_tuple_error();
    return false;
}

static
bool is_valid_headers(PyObject * headers)
{
    if (PyList_Check(headers)) {
        Py_ssize_t len = PyList_GET_SIZE(headers);
        for (Py_ssize_t i = 0; i < len; i++) {
            if (!is_valid_header_tuple(PyList_GET_ITEM(headers, i)))
                return false;
        }
        return true;
    }
    set_header_list_error(headers);
    return false;
}

static
bool is_valid_exc_info(StartResponse * sr)
{
    if (sr->exc_info && sr->exc_info != Py_None) {
        if (PyTuple_Check(sr->exc_info))
            if (PyTuple_GET_SIZE(sr->exc_info) == 3)
                return true;
        
        set_exc_info_type_error(sr->exc_info);
        return false;
    }
    if (sr->called == 1) {
        set_exc_info_missing_error();
        return false;
    }
    return true;
}

static
PyObject * start_response_call(StartResponse * sr, PyObject * args, PyObject * kwargs)
{
    if (sr->called == 1) {
        Py_CLEAR(sr->status);
        Py_CLEAR(sr->headers);
    }
    sr->exc_info = NULL;

    int rc = PyArg_UnpackTuple(args, "start_response", 2, 3, &sr->status, &sr->headers, &sr->exc_info);
    if (rc == (int)false)
        return NULL;

    if (!is_valid_status(sr->status))
        return NULL;

    if (!is_valid_headers(sr->headers))
        return NULL;

    if (!is_valid_exc_info(sr))
        return NULL;

    sr->called = 1;

    Py_XINCREF(sr->status);
    Py_XINCREF(sr->headers);
    Py_XINCREF(sr->exc_info);

    Py_RETURN_NONE;
}

static
void start_response_dealloc(StartResponse * self)
{
    Py_CLEAR(self->status);
    Py_CLEAR(self->headers);
    Py_CLEAR(self->exc_info);
    PyObject_Del(self);
}

PyTypeObject StartResponse_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "start_response",
    .tp_basicsize = sizeof(StartResponse),
    .tp_itemsize  = 0,
    .tp_dealloc   = (destructor) start_response_dealloc,
    .tp_call      = (ternaryfunc) start_response_call,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_finalize  = NULL
};
