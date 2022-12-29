#include <Python.h>
#include "server.h"

static PyMethodDef FastWsgiFunctions[] = {
    { "init_server", init_server, METH_O, "" },
    { "change_setting", change_setting, METH_VARARGS, "" },
    { "run_server", run_server, METH_O, "" },
    { "run_nowait", run_nowait, METH_O, "" },
    { "close_server", close_server, METH_O, "" },
    { NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "fastwsgi",
    "fastwsgi Python module",
    -1,
    FastWsgiFunctions,
};

PyMODINIT_FUNC PyInit__fastwsgi(void)
{
    return PyModule_Create(&module);
}
