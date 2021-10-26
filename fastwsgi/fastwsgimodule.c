#include <Python.h>
#include "server.h"

static PyMethodDef FastWsgiFunctions[] = {
    {"run_server", run_server, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "fastwsgi",
    "fastwsgi Python module",
    -1,
    FastWsgiFunctions,
};

PyMODINIT_FUNC PyInit__fastwsgi(void) {
  return PyModule_Create(&module);
}