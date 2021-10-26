#include "filewrapper.h"

// Ref FileWrapper: https://github.com/python/cpython/blob/main/Lib/wsgiref/util.py#L11

PyObject* FileWrapper_New(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
    PyObject* filelike;
    PyObject* blksize = NULL;

    if (!PyArg_ParseTuple(args, "OO", &filelike, &blksize))
        return NULL;

    FileWrapper* wrapper = PyObject_NEW(FileWrapper, type);
    wrapper->filelike = filelike;
    wrapper->blksize = blksize;

    Py_INCREF(filelike);
    Py_XINCREF(blksize);

    return (PyObject*)wrapper;
}

PyObject* FileWrapper_Iter(PyObject* self) {
    Py_INCREF(self);
    return self;
}

PyObject* FileWrapper_Next(PyObject* self) {
    PyObject* read = PyUnicode_FromString("read");
    FileWrapper* wrapper = (FileWrapper*)self;
    return PyObject_CallMethodObjArgs(wrapper->filelike, read, wrapper->blksize, NULL);
}

PyObject* FileWrapper_Close(PyObject* self) {
    FileWrapper* wrapper = (FileWrapper*)self;
    PyObject* close = PyUnicode_FromString("close");
    if (PyObject_HasAttr(wrapper->filelike, close))
        return PyObject_CallMethodObjArgs(wrapper->filelike, close, NULL);

    Py_RETURN_NONE;
}

void FileWrapper_Clear(PyObject* self) {
    FileWrapper* wrapper = (FileWrapper*)self;
    Py_DECREF(wrapper->filelike);
    Py_XDECREF(wrapper->blksize);
    PyObject_FREE(self);
    free(wrapper);
}

PyTypeObject FileWrapper_Type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "FileWrapper",
  sizeof(FileWrapper),
  0,
  (destructor)FileWrapper_Clear,
};

static PyMethodDef Methods[] = {
    {"close", (PyCFunction)FileWrapper_Close, METH_NOARGS, NULL},
    {NULL}
};

void FileWrapper_Init(void) {
    FileWrapper_Type.tp_new = FileWrapper_New;
    FileWrapper_Type.tp_iter = FileWrapper_Iter;
    FileWrapper_Type.tp_flags |= Py_TPFLAGS_DEFAULT;
    FileWrapper_Type.tp_iternext = FileWrapper_Next;
    FileWrapper_Type.tp_methods = Methods;
}