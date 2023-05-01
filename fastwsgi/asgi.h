#ifndef _FASTWSGI_ASGI_
#define _FASTWSGI_ASGI_

#include "common.h"
#include "request.h"


typedef struct {
    PyObject * asyncio;  // module
    PyObject * uni_loop; // united loop
    struct {
        PyObject * self;
        PyObject * run_forever;
        PyObject * run_until_complete;
        PyObject * call_soon;
        PyObject * call_later;
        PyObject * create_future;
        PyObject * create_task;
        PyObject * add_reader;
        PyObject * remove_reader;
    } loop;
    struct {
        PyObject * self;
        PyObject * set_result;
    } future;
} asyncio_t;

int asyncio_init(asyncio_t * aio);
int asyncio_free(asyncio_t * aio, bool free_self);


typedef struct {
    PyObject   ob_base;
    void     * client;
    PyObject * task;   // task for coroutine
    PyObject * scope;  // PyDict
    struct {
        PyObject * future;
        bool       completed;
    } recv;
    struct {
        PyObject * future;
        int        status;   // response status
        PyObject * start_response;  // PyDict
        int        num_body;
        int64_t    body_size;
        bool       latest_chunk;
    } send;
} asgi_t;

extern PyTypeObject ASGI_Type;

INLINE
PyObject * create_asgi(void * client)
{
    asgi_t * asgi = PyObject_New(asgi_t, &ASGI_Type);
    if (asgi) {
        size_t prefix = offsetof(asgi_t, client);
        memset((char *)asgi + prefix, 0, sizeof(asgi_t) - prefix);
        asgi->client = client;
    }
    return (PyObject *)asgi;
}


bool asgi_app_check(PyObject * app);
int  asgi_init(void * client);
int  asgi_free(void * client);
int  asgi_call_app(void * _client);

int  asgi_future_set_result(void * client, PyObject ** ptr_future, PyObject * result);
int  asgi_future_set_exception(void * _client, PyObject ** ptr_future, const char * fmt, ...);


INLINE
Py_ssize_t asgi_get_data_from_header(PyObject * object, size_t index, const char ** data)
{
    PyObject * item = NULL;

    if (PyList_Check(object)) {
        if (PyList_GET_SIZE(object) >= 2)
            item = PyList_GET_ITEM(object, index);
    }
    else if (PyTuple_Check(object)) {
        if (PyTuple_GET_SIZE(object) >= 2)
            item = PyTuple_GET_ITEM(object, index);
    }
    else {
        return -39001;  // error
    }
    if (item) {
        if (PyBytes_Check(item)) {
            *data = PyBytes_AS_STRING(item);
            return PyBytes_GET_SIZE(item);
        }
        if (PyUnicode_Check(item)) {
            *data = PyUnicode_DATA(item);
            return PyUnicode_GET_LENGTH(item);
        }
    }
    return -39004;
}


#endif
