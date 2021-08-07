#include <Python.h>

typedef struct {
    PyObject* host;
    PyObject* port;
} ServerArgs;

void run_server(ServerArgs*);