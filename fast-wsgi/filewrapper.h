#include <Python.h>

typedef struct {
    PyObject ob_base;
    PyObject* filelike;
    PyObject* blksize;
} FileWrapper;

PyTypeObject FileWrapper_Type;

#define FileWrapper_CheckExact(object) ((object)->ob_type == &FileWrapper_Type)

void FileWrapper_Init(void);