#include <Python.h>
#include "server.h"

static PyObject* test(PyObject* self, PyObject* args) {
  Py_RETURN_NONE;
}

static PyMethodDef FastWsgiFunctions[] = {
    {"test", test, METH_VARARGS, "test function."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "fast_wsgi",
    "fast_wsgi Python module",
    -1,
    FastWsgiFunctions,
};

PyMODINIT_FUNC PyInit_fast_wsgi(void) {
  return PyModule_Create(&module);
}