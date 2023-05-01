#include "asgi.h"
#include "server.h"
#include "constants.h"


bool asgi_app_check(PyObject * app)
{
    int hr = -1;
    PyObject * func = get_function(app);
    FIN_IF(!func, -1);
    PyCodeObject * code = (PyCodeObject *)PyFunction_GET_CODE(func);
    if ((code->co_flags & CO_COROUTINE) != 0 || code->co_argcount == 1) {
        FIN(0);
    }
fin:
    Py_DECREF(func);
    return (hr == 0) ? true : false;
}

int64_t g_idle_num = 0;

PyObject * uni_loop(PyObject * self, PyObject * not_used)
{
    bool relax = false;
    PyObject * res = NULL;

    g_srv.num_loop_cb = 0;  // reset cb counter

    uv_run(g_srv.loop, UV_RUN_NOWAIT);

    if (g_srv.num_loop_cb == 0 && g_srv.num_writes == 0) {
        g_idle_num++;
    } else {
        g_idle_num = 0;
    }
    if (g_idle_num > 10) {
        relax = true;
    }
    if (relax == false) {
        res = PyObject_CallFunctionObjArgs(g_srv.aio.loop.call_soon, g_srv.aio.uni_loop, NULL);
    } else {
        res = PyObject_CallFunctionObjArgs(g_srv.aio.loop.call_later, g_cv.f0_001, g_srv.aio.uni_loop, NULL);
    }
    Py_XDECREF(res);
    Py_RETURN_NONE;
}

static PyMethodDef uni_loop_method = {
    "uni_loop", uni_loop, METH_NOARGS, ""
}; 

int asyncio_init(asyncio_t * aio)
{
    int hr = 0;
    memset(aio, 0, sizeof(asyncio_t));
    
    aio->asyncio = PyImport_ImportModule("asyncio");
    FIN_IF(!aio->asyncio, -4500010);

    PyObject * mdict = PyModule_GetDict(aio->asyncio);
    // "new_event_loop"); // "get_running_loop");
    PyObject * get_event_loop = PyDict_GetItemString(mdict, "get_event_loop");
    FIN_IF(!get_event_loop, 4500015);
    FIN_IF(!PyCallable_Check(get_event_loop), -4500016);
    
    aio->loop.self = PyObject_CallObject(get_event_loop, NULL);
    FIN_IF(!aio->loop.self, -4500020);

    aio->loop.run_forever = PyObject_GetAttrString(aio->loop.self, "run_forever");
    FIN_IF(!aio->loop.run_forever, -4500027);
    FIN_IF(!PyCallable_Check(aio->loop.run_forever), -4500028);

    aio->loop.run_until_complete = PyObject_GetAttrString(aio->loop.self, "run_until_complete");
    FIN_IF(!aio->loop.run_until_complete, -4500031);
    FIN_IF(!PyCallable_Check(aio->loop.run_until_complete), -4500032);

    aio->loop.call_soon = PyObject_GetAttrString(aio->loop.self, "call_soon");
    FIN_IF(!aio->loop.call_soon, -4500041);
    FIN_IF(!PyCallable_Check(aio->loop.call_soon), -4500042);

    aio->loop.call_later = PyObject_GetAttrString(aio->loop.self, "call_later");
    FIN_IF(!aio->loop.call_later, -4500045);
    FIN_IF(!PyCallable_Check(aio->loop.call_later), -4500046);

    aio->loop.create_future = PyObject_GetAttrString(aio->loop.self, "create_future");
    FIN_IF(!aio->loop.create_future, -4500051);
    FIN_IF(!PyCallable_Check(aio->loop.create_future), -4500052);

    aio->loop.create_task = PyObject_GetAttrString(aio->loop.self, "create_task");
    FIN_IF(!aio->loop.create_task, -4500061);
    FIN_IF(!PyCallable_Check(aio->loop.create_task), -4500062);

    aio->loop.add_reader = PyObject_GetAttrString(aio->loop.self, "add_reader");
    FIN_IF(!aio->loop.add_reader, -4500071);
    FIN_IF(!PyCallable_Check(aio->loop.add_reader), -4500072);

    aio->loop.remove_reader = PyObject_GetAttrString(aio->loop.self, "remove_reader");
    FIN_IF(!aio->loop.remove_reader, -4500081);
    FIN_IF(!PyCallable_Check(aio->loop.remove_reader), -4500082);

    aio->future.self = PyObject_CallObject(aio->loop.create_future, NULL);
    FIN_IF(!aio->future.self, -4500111);

    aio->future.set_result = PyObject_GetAttrString(aio->future.self, "set_result");
    FIN_IF(!aio->future.set_result, -4500113);
    FIN_IF(!PyCallable_Check(aio->future.set_result), -4500115);

    aio->uni_loop = PyCFunction_New(&uni_loop_method, NULL);
    FIN_IF(!aio->uni_loop, -4500213);
    FIN_IF(!PyCallable_Check(aio->uni_loop), -4500214);

    hr = 0;
fin:
    if (hr) {
        asyncio_free(aio, false);
    }
    return hr;
}

int asyncio_free(asyncio_t * aio, bool free_self)
{
    if (aio) {
        Py_XDECREF(aio->uni_loop);
        Py_XDECREF(aio->future.set_result);
        Py_XDECREF(aio->future.self);
        Py_XDECREF(aio->loop.remove_reader);
        Py_XDECREF(aio->loop.add_reader);
        Py_XDECREF(aio->loop.create_task);
        Py_XDECREF(aio->loop.create_future);
        Py_XDECREF(aio->loop.call_soon);
        Py_XDECREF(aio->loop.run_until_complete);
        Py_XDECREF(aio->loop.run_forever);
        Py_XDECREF(aio->loop.self);
        Py_XDECREF(aio->asyncio);
        memset(aio, 0, sizeof(asyncio_t));
        if (free_self)
            free(aio);
    }
    return 0;
}

// -----------------------------------------------------------------------------------

PyObject * g_scope = NULL;

static
void create_asgi_scope(void)
{
    if (!g_scope) {
        char buf[32];
        sprintf(buf, "%d", g_srv.port);
        PyObject * port = PyUnicode_FromString(buf);
        PyObject * host = PyUnicode_FromString(g_srv.host);
        // only constant values!!!
        PyObject * scope_asgi = PyDict_New();
        PyDict_SetItem(scope_asgi, g_cv.version, g_cv.v3_0);
        PyDict_SetItem(scope_asgi, g_cv.spec_version, g_cv.v2_0);
        g_scope = PyDict_New();
        PyDict_SetItem(g_scope, g_cv.type, g_cv.http);
        PyDict_SetItem(g_scope, g_cv.scheme, g_cv.http);
        PyDict_SetItem(g_scope, g_cv.query_string, g_cv.empty_bytes);
        PyDict_SetItem(g_scope, g_cv.asgi, scope_asgi);
        Py_DECREF(scope_asgi);
        //PyDict_SetItem(g_scope, g_cv.server, g_cv.empty_string); // FIXME
        //PyDict_SetItem(g_scope, g_cv.SERVER_NAME, host);
        //PyDict_SetItem(g_scope, g_cv.SERVER_PORT, port);
        Py_DECREF(port);
        Py_DECREF(host);
    }
}

int asgi_init(void * _client)
{
    int hr = 0;
    client_t * client = (client_t *)_client;
    FIN_IF(!g_srv.asgi_app, 0);
    asgi_free(client);
    PyObject * asgi = create_asgi(client);
    FIN_IF(!asgi, -4510001);
    client->asgi = (asgi_t *)asgi;
    create_asgi_scope();
    client->asgi->scope = PyDict_Copy(g_scope);
    hr = 0;
    LOGt("%s: asgi = %p ", __func__, asgi);
fin:
    return hr;
}

int asgi_free(void * _client)
{
    client_t * client = (client_t *)_client;
    asgi_t * asgi = client->asgi;
    if (asgi) {
        asgi->client = NULL;
        LOGd("%s: RefCnt(asgi) = %d, RefCnt(task) = %d", __func__, (int)Py_REFCNT(asgi), asgi->task ? (int)Py_REFCNT(asgi->task) : -333);
        Py_DECREF(asgi);
    }
    client->asgi = NULL;
    return 0;
}

int asgi_call_app(void * _client)
{
    int hr = 0;
    client_t * client = (client_t *)_client;
    asgi_t * asgi = client->asgi;
    PyObject * coroutine = NULL;
    PyObject * task = NULL;
    PyObject * result = NULL;

    LOGd("%s: ....", __func__);
    PyObject * receive = PyObject_GetAttrString((PyObject *)asgi, "receive");
    PyObject * send = PyObject_GetAttrString((PyObject *)asgi, "send");
    PyObject * done = PyObject_GetAttrString((PyObject *)asgi, "done");
    FIN_IF(!receive || !send || !done, -4502011);

    // call ASGI 3.0 app
    coroutine = PyObject_CallFunctionObjArgs(g_srv.asgi_app, asgi->scope, receive, send, NULL);
    LOGc_IF(!coroutine, "%s: cannot call ASGI 3.0 app", __func__);
    FIN_IF(!coroutine, -4502031);
    FIN_IF(!PyCoro_CheckExact(coroutine), -4502033);

    task = PyObject_CallFunctionObjArgs(g_srv.aio.loop.create_task, coroutine, NULL);
    FIN_IF(!task, -4502041);

    result = PyObject_CallMethodObjArgs(task, g_cv.add_done_callback, done, NULL);
    LOGe_IF(!result, "%s: error on task.add_done_callback", __func__);
    FIN_IF(!result, -4502051);

    LOGi("%s: ASGI TASK created", __func__);
    asgi->task = task;
    Py_INCREF(task);
    hr = 0;
fin:
    LOGe_IF(hr, "%s: FIN with error = %d", __func__, hr);
    Py_XDECREF(receive);
    Py_XDECREF(send);
    Py_XDECREF(done);
    Py_XDECREF(coroutine);
    Py_XDECREF(task);
    Py_XDECREF(result);
    return hr;
}

// -----------------------------------------------------------------------------------

int asgi_get_info_from_response(client_t * client, PyObject * dict)
{
    int hr = 0;
    PyObject * iterator = NULL;
    PyObject * item = NULL;

    client->response.wsgi_content_length = -1;  // unknown
    PyObject * headers = PyDict_GetItem(dict, g_cv.headers);
    FIN_IF(!headers, -4);
    iterator = PyObject_GetIter(headers);
    FIN_IF(!iterator, -5);

    for (size_t i = 0; /* nothing */ ; i++) {
        Py_XDECREF(item);
        item = PyIter_Next(iterator);
        if (!item)
            break;

        const char * key;
        Py_ssize_t key_len = asgi_get_data_from_header(item, 0, &key);
        FIN_IF(key_len < 0, -10);

        const char * val;
        Py_ssize_t val_len = asgi_get_data_from_header(item, 1, &val);
        FIN_IF(val_len < 0, -11);

        if (key_len == 14 && key[7] == '-' && strcasecmp(key, "Content-Length") == 0) {
            FIN_IF(val_len == 0, -22);  // error
            int64_t clen;
            if (val_len == 1 && val[0] == '0') {
                clen = 0;
            } else {
                clen = strtoll(val, NULL, 10);
                FIN_IF(clen <= 0 || clen == LLONG_MAX, -33);  // error
            }
            LOGi("asgi response: content-length = %lld", (long long)clen);
            client->response.wsgi_content_length = clen;
        }
    }
    hr = 0;
fin:
    LOGc_IF(hr, "%s: error = %d", __func__, hr);
    Py_XDECREF(item);
    Py_XDECREF(iterator);
    return hr;
}

int asgi_build_response(client_t * client)
{
    int hr = 0;
    asgi_t * asgi = client->asgi;
    int status = asgi->send.status;
    PyObject * start_response = asgi->send.start_response;

    int flags = (client->request.keep_alive) ? RF_SET_KEEP_ALIVE : 0;
    int len = build_response(client, flags | RF_HEADERS_ASGI, status, start_response, NULL, -1);
    if (len <= 0) {
        LOGe("%s: error = %d", __func__, len);
        //err = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        reset_head_buffer(client);
        reset_response_body(client);
        FIN(len);
    }
    hr = 0;
fin:
    Py_CLEAR(asgi->send.start_response);
    return hr;
}

// -----------------------------------------------------------------------------------

int asgi_future_set_result_soon(void * _client, PyObject * future, PyObject * result)
{
    int hr = 0;
    client_t * client = (client_t *)_client;
    PyObject * set_result = NULL;
    PyObject * ret = NULL;

    FIN_IF(!future, -4530914);

    set_result = PyObject_GetAttr(future, g_cv.set_result);
    FIN_IF(!set_result, -4530916);
    FIN_IF(!PyCallable_Check(set_result), -4530917);

    ret = PyObject_CallFunctionObjArgs(g_srv.aio.loop.call_soon, set_result, result, NULL);
    FIN_IF(!ret, -4530921);
    hr = 0;
fin:
    Py_XDECREF(ret);
    Py_XDECREF(set_result);
    return hr;
}

int asgi_future_set_result(void * _client, PyObject ** ptr_future, PyObject * result)
{
    int hr = 0;
    client_t * client = (client_t *)_client;
    PyObject * future = NULL;
    PyObject * done = NULL;
    PyObject * ret = NULL;

    FIN_IF(!ptr_future, -4530964);
    future = *ptr_future;
    FIN_IF(!future, -4530965);

    done = PyObject_CallMethodObjArgs(future, g_cv.done, NULL);
    FIN_IF(!done, -4530966);
    FIN_IF(done == Py_True, -4530967);  // already completed

    ret = PyObject_CallMethodObjArgs(future, g_cv.set_result, result, NULL);
    FIN_IF(!ret, -4530971);
    hr = 0;
fin:
    Py_XDECREF(ret);
    Py_XDECREF(done);
    Py_XDECREF(future);
    if (ptr_future)
        *ptr_future = NULL;

    return hr;
}

int asgi_future_set_exception(void * _client, PyObject ** ptr_future, const char * fmt, ...)
{
    int hr = 0;
    char text[1024];
    va_list args;
    client_t * client = (client_t *)_client;
    PyObject * future = NULL;
    PyObject * exc_text = NULL;
    PyObject * exception = NULL;
    PyObject * ret = NULL;

    FIN_IF(!ptr_future, -4530981);
    future = *ptr_future;
    FIN_IF(!future, -4530982);

    va_start(args, fmt);
    vsprintf(text, fmt, args);
    va_end(args);

    exc_text = PyUnicode_FromString(text);
    FIN_IF(!exc_text, -4530984);

    exception = PyObject_CallFunctionObjArgs(PyExc_RuntimeError, exc_text, NULL);
    FIN_IF(!exception, -4530985);

    ret = PyObject_CallMethodObjArgs(future, g_cv.set_exception, exception, NULL);
    FIN_IF(!ret, -4530989);
    hr = 0;
fin:
    Py_XDECREF(ret);
    Py_XDECREF(exception);
    Py_XDECREF(exc_text);
    Py_XDECREF(future);
    if (ptr_future)
        *ptr_future = NULL;

    return hr;
}

// -----------------------------------------------------------------------------------

// ASGI coro "receive"
PyObject * asgi_receive(PyObject * self, PyObject * notused)
{
    int hr = 0;
    asgi_t * asgi = (asgi_t *)self;
    client_t * client = asgi->client;
    PyObject * future = NULL;
    PyObject * dict = NULL;
    PyObject * input_body = NULL;
    int64_t input_size = -1;
    
    update_log_prefix(client);
    LOGt("%s: ....", __func__);
    if (asgi->recv.completed) {
        future = PyObject_CallObject(g_srv.aio.loop.create_future, NULL);
        asgi->recv.future = future;
        Py_INCREF(future);
        FIN(0);
    }
    dict = PyDict_New();
    int rc = PyDict_SetItem(dict, g_cv.type, g_cv.http_request);
    FIN_IF(rc, -4560715);

    input_size = client->request.wsgi_input_size;
    if (input_size <= 0) {
        input_size = 0;
        input_body = g_cv.empty_bytes;
        Py_INCREF(input_body);
    } else {
        input_body = PyObject_CallMethodObjArgs(client->request.wsgi_input, g_cv.getbuffer, NULL);
    }
    rc = PyDict_SetItem(dict, g_cv.body, input_body);
    if (rc == 0) {
        Py_DECREF(input_body);
        input_body = NULL;
    }

    future = PyObject_CallObject(g_srv.aio.loop.create_future, NULL);

    int err = asgi_future_set_result_soon(client, future, dict);
    FIN_IF(err, -4560775);
    asgi->recv.completed = true;

    LOGd("%s: recv size = %d", __func__, input_size);
    hr = 0;
fin:
    if (hr) {
        Py_CLEAR(future);
        future = NULL;
    }
    Py_XDECREF(dict);
    Py_XDECREF(input_body);
    return future;
}

// ASGI coro "send"
PyObject * asgi_send(PyObject * self, PyObject * dict)
{
    int hr = 0;
    asgi_t * asgi = (asgi_t *)self;
    client_t * client = asgi->client;
    PyObject * type = NULL;
    PyObject * status = NULL;
    PyObject * body = NULL;
    PyObject * future = NULL;

    update_log_prefix(client);
    LOGt("%s: ....", __func__);
    FIN_IF(!PyDict_Check(dict), -4570005);
    type = PyDict_GetItem(dict, g_cv.type);
    FIN_IF(!type, -4570011);
    FIN_IF(!PyUnicode_Check(type), -4570012);
    const char * evt_type = PyUnicode_AsUTF8(type);
    LOGi("%s: event type = '%s' ", __func__, evt_type);

    if (asgi->send.status == 0) {
        int rc = strcmp(evt_type, "http.response.start");
        FIN_IF(rc != 0, -4570021);
        asgi->send.status = -1;  // error
        status = PyDict_GetItem(dict, g_cv.status);
        FIN_IF(!status, -4570031);
        FIN_IF(!PyLong_Check(status), -4570032);
        int _status = (int)PyLong_AS_LONG(status);
        FIN_IF(_status < 100, -4570037);
        FIN_IF(_status > 999, -4570038);
        asgi->send.status = _status;
        int err = asgi_get_info_from_response(client, dict);
        if (err) {
            LOGc("response header 'Content-Length' contain incorrect value!");
            //err = HTTP_STATUS_INTERNAL_SERVER_ERROR;
            FIN(-4570047);
        }
        asgi->send.start_response = dict;
        Py_INCREF(dict);
        FIN(0);
    }
    if (strcmp(evt_type, "http.response.body") == 0) {
        if (asgi->send.latest_chunk) {
            LOGe("%s: body already readed from APP", __func__);
            FIN(0);
        }
        asgi->send.num_body++;

        body = PyDict_GetItem(dict, g_cv.body);
        if (!body)
            body = g_cv.empty_bytes;
        
        int64_t body_size = (int64_t)PyBytes_GET_SIZE(body);
        asgi->send.body_size += body_size;

        PyObject * more_body = PyDict_GetItem(dict, g_cv.more_body);
        bool latest_body = (more_body == Py_True) ? false : true;

        if (latest_body)
            asgi->send.latest_chunk = true;

        LOGi("%s: body size = %d, latest_body = %d, chunked = %d", __func__, (int)body_size, (int)latest_body, client->response.chunked);

        if (asgi->send.num_body == 1) {
            if (latest_body) {
                if (body_size == 0) {
                    // response without body
                    client->response.body_total_size = 0;
                    client->response.body_preloaded_size = 0;
                } else {
                    Py_INCREF(body);  // Reasone: "body" inserted into body chunks array
                    client->response.body[0] = body;
                    client->response.body_chunk_num = 1;
                    client->response.body_total_size = body_size;
                    client->response.body_preloaded_size = body_size;
                }
            } else {
                client->response.chunked = 1;
                LOGi("%s: chunked content transfer begins (unknown size of body)", __func__);
                client->response.body_chunk_num = 0;
            }
        }
        if (client->response.chunked) {
            if (body_size == 0 && !latest_body) {
                LOGd("%s: skip empty chunk", __func__);
                FIN(0);
            }
            if (body_size > 0) {
                Py_INCREF(body);  // Reason: "body" inserted into body chunks array
                client->response.body[client->response.body_chunk_num++] = body;
                client->response.body_preloaded_size += body_size;
                LOGd("%s: added chunk, size = %d", __func__, (int)body_size);
            }
            if (latest_body) {
                client->response.chunked = 2;
                if (body_size > 0) {
                    client->response.body[client->response.body_chunk_num++] = g_cv.footer_last_chunk;
                    Py_INCREF(g_cv.footer_last_chunk);  // Reason: "footer_last_chunk" inserted into body chunks array
                    //client->response.body_preloaded_size += PyBytes_GET_SIZE(g_cv.footer_last_chunk);
                }
            }
        }
        if (asgi->send.start_response) {
            int err = asgi_build_response(client);
            LOGe_IF(err, "%s: asgi_build_response return error = %d", __func__, err);
            FIN_IF(err, err);
            LOGi("Response created! (len = %d+%lld)", client->head.size, (long long)client->response.body_preloaded_size);
        }
        else if (client->response.chunked) {
            int csize = (int)client->response.body_preloaded_size;
            xbuf_reset(&client->head);
            char * buf = xbuf_expand(&client->head, 48);
            if (csize > 0) {
                client->head.size += sprintf(buf, "%X\r\n", csize);
            } else {
                client->head.size += sprintf(buf, "0\r\n\r\n");
            }
            client->response.headers_size = client->head.size;
            LOGd("%s: added chunk prefix, size = %d ", __func__, csize);
        }        
        int act = stream_write(client);

        future = PyObject_CallObject(g_srv.aio.loop.create_future, NULL);
        FIN_IF(!future, -4570601);
        asgi->send.future = future;
        Py_INCREF(future);
        FIN(0);
    }
    LOGe("%s: unsupported event type: '%s' ", __func__, evt_type);
    hr = -4570901;
fin:
    if (hr) {
        LOGe("%s: FIN WITH error = %d", hr);
        PyObject * error = PyErr_Format(PyExc_RuntimeError, "%s: error = %d", __func__, hr);
        return error;
    }
    return (future != NULL) ? future : self;
}

// ASGI callback "done"
PyObject * asgi_done(PyObject * self, PyObject * future)
{
    int hr = 0;
    asgi_t * asgi = (asgi_t *)self;
    client_t * client = asgi->client;
    PyObject * res = NULL;
    update_log_prefix(client);

    res = PyObject_CallMethodObjArgs(future, g_cv.result, NULL);
    if (res == NULL) {
        LOGe("%s: Exception detected", __func__);
        PyErr_Clear();
    } else {
        LOGd("%s: result type = %s", __func__, Py_TYPE(res)->tp_name);
    }
    LOGd("%s: RefCnt(asgi) = %d", __func__, (int)Py_REFCNT(self));
    hr = 0;
//fin:
    if (client) {
        client->asgi = NULL;
        stream_read_start(client);
    }
    Py_XDECREF(res);
    Py_RETURN_NONE;
}

PyObject * asgi_await(PyObject * self)
{
    Py_INCREF(self);
    return self;
}

void asgi_dealloc(asgi_t * self)
{
    asgi_t * asgi = (asgi_t *)self;
    LOGd("%s: RefCnt(asgi) = %d, RefCnt(task) = %d,", __func__, (int)Py_REFCNT(self), self->task ? (int)Py_REFCNT(self->task) : -999);
    Py_CLEAR(self->scope);
    Py_CLEAR(self->recv.future);
    Py_CLEAR(self->send.future);
    Py_CLEAR(self->send.start_response);
    Py_CLEAR(self->task);
    PyObject_Del(self);
}

PyObject * asgi_iter(PyObject * self)
{
    Py_INCREF(self);
    return self;
}

PyObject * asgi_next(PyObject * self)
{
    return NULL;
}

// -----------------------------------------------------------------------------------

static PyMethodDef asgi_methods[] = {
    { "receive", asgi_receive, METH_NOARGS, 0 },
    { "send",    asgi_send,    METH_O,      0 },
    { "done",    asgi_done,    METH_O,      0 },
    { NULL,      NULL,         0,           0 }
};

static PyAsyncMethods asgi_async_methods = {
    .am_await = asgi_await
};

PyTypeObject ASGI_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "ASGI",
    .tp_basicsize = sizeof(asgi_t),
    .tp_dealloc   = (destructor) asgi_dealloc,
    .tp_as_async  = &asgi_async_methods,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_iter      = asgi_iter,
    .tp_iternext  = asgi_next,
    .tp_methods   = asgi_methods,
    .tp_finalize  = NULL
};

