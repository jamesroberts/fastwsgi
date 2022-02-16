#include <stdbool.h>
#include "start_response.h"

void set_status_error() {
    char* err_msg = "'status' must be a 3-digit string";
    PyErr_SetString(PyExc_ValueError, err_msg);
}

void set_header_tuple_error() {
    char* err_msg = "start_response argument 2 expects a list of 2-tuples (str, str)";
    PyErr_SetString(PyExc_TypeError, err_msg);
};

void set_header_list_error(PyObject* headers) {
    PyErr_Format(
        PyExc_TypeError,
        "start_response argument 2 expects a list of 2-tuples, got '%s' instead.",
        Py_TYPE(headers)->tp_name
    );
};

void set_exc_info_type_error(PyObject* exc_info) {
    PyErr_Format(
        PyExc_TypeError,
        "start_response argument 3 expects a 3-tuple, got '%s' instead.",
        Py_TYPE(exc_info)->tp_name
    );
};

void set_exc_info_missing_error() {
    char* err_msg = "'exc_info' is required in the second call of 'start_response'";
    PyErr_SetString(PyExc_TypeError, err_msg);
};

bool is_valid_status(PyObject* status) {
    bool valid = PyUnicode_Check(status) && PyUnicode_GET_LENGTH(status) >= 3;
    if (!valid) set_status_error();
    return valid;
};

bool is_valid_header_tuple(PyObject* tuple) {
    bool valid = false;

    if (PyTuple_Check(tuple) && PyTuple_GET_SIZE(tuple) == 2) {
        int field = PyUnicode_Check(PyTuple_GET_ITEM(tuple, 0));
        int value = PyUnicode_Check(PyTuple_GET_ITEM(tuple, 1));
        valid = field && value;
    }
    if (!valid) set_header_tuple_error();
    return valid;
}

bool is_valid_headers(PyObject* headers) {
    bool valid = PyList_Check(headers);

    if (!valid) {
        set_header_list_error(headers);
        return valid;
    }

    for (Py_ssize_t i = 0; i < PyList_GET_SIZE(headers); i++) {
        valid = is_valid_header_tuple(PyList_GET_ITEM(headers, i));
        if (!valid) break;
    }
    return valid;
};

bool is_valid_exc_info(StartResponse* sr) {
    bool valid = true;

    if (sr->exc_info && sr->exc_info != Py_None) {
        valid = PyTuple_Check(sr->exc_info) && PyTuple_GET_SIZE(sr->exc_info);
        if (!valid) set_exc_info_type_error(sr->exc_info);
    }
    else if (sr->called == 1) {
        set_exc_info_missing_error();
        valid = false;
    }
    return valid;
};

PyObject* start_response_call(PyObject* self, PyObject* args, PyObject* kwargs) {
    StartResponse* sr = (StartResponse*)self;

    if (sr->called == 1) {
        Py_CLEAR(sr->status);
        Py_CLEAR(sr->headers);
    }

    sr->exc_info = NULL;
    if (!PyArg_UnpackTuple(args, "start_response", 2, 3, &sr->status, &sr->headers, &sr->exc_info))
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
};

PyTypeObject StartResponse_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "start_response",
    sizeof(StartResponse),
    0,
    (destructor)PyObject_FREE,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    start_response_call
};
