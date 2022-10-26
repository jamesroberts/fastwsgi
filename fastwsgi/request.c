#include "server.h"
#include "request.h"
#include "llhttp.h"
#include "constants.h"
#include "start_response.h"

PyObject* g_base_dict = NULL;

void build_response(client_t * client, StartResponse* response);
int get_info_from_wsgi_response(client_t * client, StartResponse * response);

#define PYTYPE_ERR(...) \
    do { \
        LOGc(__VA_ARGS__); \
        PyErr_Format(PyExc_TypeError, __VA_ARGS__); \
    } while(0)

void logrepr(int level, PyObject* obj) {
    PyObject* repr = PyObject_Repr(obj);
    PyObject* str = PyUnicode_AsEncodedString(repr, "utf-8", "~E~");
    const char* bytes = PyBytes_AS_STRING(str);
    LOGX(level, "REPR: %s", bytes);
    Py_XDECREF(repr);
    Py_XDECREF(str);
}

typedef enum {
    SH_LATIN1       = 0x01,
    SH_CONCAT       = 0x02,
    SH_COMMA_DELIM  = 0x04
} set_header_flag_t;

static
int set_header(client_t * client, PyObject * key, const char * value, ssize_t length, int flags)
{
    PyObject * headers = client->request.headers;
    ssize_t vlen = (length >= 0) ? length : strlen(value);
    LOGi("set_header: %s = '%.*s' %s", PyUnicode_AsUTF8(key), (int)vlen, value, (flags & SH_CONCAT) ? "(concat)" : "");
    PyObject * val = NULL;
    PyObject * setval = NULL;
    if (flags & SH_LATIN1) {
        val = PyUnicode_DecodeLatin1(value, vlen, NULL);
    } else {
        val = PyUnicode_FromStringAndSize(value, vlen);  // as UTF-8
    }
    if (!val)
        return -3;

    if (flags & SH_CONCAT) {  // concat PyUnicode value
        PyObject * existing_val = PyDict_GetItem(headers, key);
        if (existing_val) {
            if (vlen == 0) {  // skip concat empty string
                Py_XDECREF(val);
                return 0;
            }
            setval = PyUnicode_Concat(existing_val, val);
        }
    }
    PyDict_SetItem(headers, key, (setval != NULL) ? setval : val);
    Py_XDECREF(val);
    Py_XDECREF(setval);
    return 0;
}

static 
int set_header_v(client_t * client, const char * key, const char * value, ssize_t length, int flags)
{
    if (!key)
        return -11;
    size_t klen = strlen(key);
    if (klen == 0)
        return -12;
    PyObject * pkey = PyUnicode_FromStringAndSize(key, klen);
    int retval = set_header(client, pkey, value, length, flags);
    Py_DECREF(pkey);
    return retval;
}

int reset_wsgi_input(client_t * client)
{
    PyObject* wsgi_input = client->request.wsgi_input;
    if (wsgi_input == NULL) {
        wsgi_input = PyObject_CallMethodObjArgs(g_cv.module_io, g_cv.BytesIO, NULL);
        client->request.wsgi_input = wsgi_input;
    } else {
        PyObject* result = PyObject_CallMethodObjArgs(wsgi_input, g_cv.seek, g_cv.i0, NULL);
        Py_DECREF(result);
    }
    client->request.wsgi_input_size = 0;
    return 0;
}

void reset_response_body(void * _client)
{
    client_t * client = (client_t *)_client;
    int chunks = client->response.body_chunk_num;
    if (chunks || client->response.wsgi_body) {
        LOGd("reset_response_body: chunks = %d, wsgi_body = %p", chunks, client->response.wsgi_body);
    }
    for (int i = 0; i < chunks; i++) {
        Py_XDECREF(client->response.body[i]);
    }
    client->response.body_chunk_num = 0;
    client->response.body_preloaded_size = 0;
    client->response.body_total_size = 0;

    if (client->response.wsgi_body) {
        if (client->response.body_iterator == client->response.wsgi_body) {
            // ClosingIterator.close() or FileWrapper.close()
            close_iterator(client->response.wsgi_body);
        } else {
            Py_CLEAR(client->response.body_iterator);
        }
    }
    Py_CLEAR(client->response.wsgi_body);
}

int on_message_begin(llhttp_t * parser)
{
    LOGi("on_message_begin: ==============================");
    client_t * client = (client_t *)parser->data;
    client->request.state.keep_alive = 0;
    client->request.state.error = 0;
    if (client->response.write_req.client != NULL) {
        client->request.state.error = 1;
        LOGc("Received new HTTP request while sending response! Disconnect client!");
        return -1;
    }
    Py_CLEAR(client->request.headers);  // wsgi_input: refcnt 2 -> 1
    // Sets up base request dict for new incoming requests
    // https://www.python.org/dev/peps/pep-3333/#specification-details
    client->request.headers = PyDict_Copy(g_base_dict);
    reset_wsgi_input(client);
    PyDict_SetItem(client->request.headers, g_cv.wsgi_input, client->request.wsgi_input); // wsgi_input: refcnt 1 -> 2
    xbuf_reset(&client->response.head);
    reset_response_body(client);
    client->response.wsgi_content_length = -1;
    return 0;
}

int on_url(llhttp_t* parser, const char* data, size_t length) {
    LOGi("on url");
    client_t * client = (client_t *)parser->data;
    size_t path_len = length;
    const char* query = NULL;
    size_t query_len = 0;
    for (size_t i = 0; i < length; i++) {
        if (data[i] == '?') {
            path_len = i;
            query_len = length - i - 1;
            if (query_len > 0)
                query = data + i + 1;
            break;
        }
    }
    if (query) {
        set_header(client, g_cv.QUERY_STRING, query, query_len, 0);
    }
    set_header(client, g_cv.PATH_INFO, data, path_len, 0);
    return 0;
}

int on_body(llhttp_t* parser, const char* body, size_t length)
{
    LOGi("on body (len = %d)", (int)length);
    client_t * client = (client_t *)parser->data;
    if (length == 0)
        return 0;

    PyObject* body_content = PyBytes_FromStringAndSize(body, length);
    LOGREPR(LL_TRACE, body_content);
    PyObject* result = PyObject_CallMethodObjArgs(client->request.wsgi_input, g_cv.write, body_content, NULL);
    if (!PyLong_CheckExact(result)) {
        client->request.state.error = 1;
        LOGf("Cannot write PyBytes to wsgi_input stream! (ret = None)");
    } else {
        size_t rv = PyLong_AsSize_t(result);
        if (rv != length) {
            client->request.state.error = 1;
            LOGf("Failed write PyBytes to wsgi_input stream! (ret = %d, expected = %d)", (int)rv, (int)length);
        }
    }
    Py_XDECREF(result);
    Py_XDECREF(body_content);
    client->request.wsgi_input_size += (int)length;
    return 0;
}

int on_header_field(llhttp_t* parser, const char* header, size_t length) {
    LOGi("on header field");
    client_t * client = (client_t *)parser->data;
    client->request.current_header[0] = 0;   // CVE-2015-0219

    const size_t max_len = sizeof(client->request.current_header) - 1;
    if (length >= max_len - 8)
        return 0;

    char upperHeader[sizeof(client->request.current_header)];
    for (size_t i = 0; i < length; i++) {
        char current = header[i];
        if (current == '_') {
            return 0;
        }
        if (current == '-') {
            upperHeader[i] = '_';
        }
        else {
            upperHeader[i] = toupper(current);
        }
    }
    upperHeader[length] = 0;

    const char* prefix = "HTTP_";
    if ((length == 14 && strcmp(upperHeader, "CONTENT_LENGTH") == 0) ||
        (length == 12 && strcmp(upperHeader, "CONTENT_TYPE") == 0)) {
        prefix = NULL;
    }
    if (prefix) {
        strcpy(client->request.current_header, prefix);
    }
    strcat(client->request.current_header, upperHeader);
    return 0;
}

int on_header_value(llhttp_t* parser, const char* value, size_t length) {
    LOGi("on header value");
    client_t * client = (client_t *)parser->data;
    if (client->request.current_header[0]) {
        set_header_v(client, client->request.current_header, value, length, 0);
    }
    return 0;
}

void close_iterator(PyObject* iterator)
{
    if (iterator != NULL && PyIter_Check(iterator)) {
        if (PyObject_HasAttrString(iterator, "close")) {
            PyObject* close = PyObject_GetAttrString(iterator, "close");
            if (close != NULL) {
                PyObject* close_result = PyObject_CallObject(close, NULL);
                Py_XDECREF(close_result);
                Py_XDECREF(close);
            }
        }
    }
}

int get_info_from_wsgi_response(client_t * client, StartResponse * response)
{
    client->response.wsgi_content_length = -1;  // unknown
    Py_ssize_t hsize = PyList_GET_SIZE(response->headers);
    for (Py_ssize_t i = 0; i < hsize; i++) {
        PyObject* tuple = PyList_GET_ITEM(response->headers, i);
        size_t key_len = 0;
        const char * key = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 0), &key_len);
        size_t value_len = 0;
        const char * value = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 1), &value_len);
        if (key_len == 14 && key[7] == '-' && strcasecmp(key, "Content-Length") == 0) {
            if (value_len == 0)
                return -2;  // error
            if (value_len == 1 && value[0] == '0')
                return 0;
            int clen = atoi(value);
            if (clen == 0 || clen == INT_MAX)
                return -3;  // error
            LOGi("wsgi response: content-length = %d", clen);
            client->response.wsgi_content_length = clen;
        }
    }
    return 0; // without parsing error
}

PyObject* wsgi_iterator_get_next_chunk(void * _client)
{
    client_t * client = (client_t *)_client;
    if (client->response.body_iterator == NULL)
        return NULL;
    PyObject* item;
    while (item = PyIter_Next(client->response.body_iterator)) {
        if (!PyBytes_Check(item)) {
            client->request.state.error = 1;
            PYTYPE_ERR("wsgi_body: ITERATOR: contain item type = '%s' (expected bytes)", Py_TYPE(item)->tp_name);
            Py_DECREF(item);
            return NULL;
        }
        Py_ssize_t size = PyBytes_GET_SIZE(item);
        if (size > 0) {
            LOGd("wsgi_body: ITERATOR: get bytes size = %d", (int)size);
            return item;
        }
        Py_DECREF(item); // skip empty items
    }
    if (PyErr_Occurred()) {
        if (client->request.state.error == 0)
            client->request.state.error = 1;
        PyErr_Print();
    }
    return NULL;
}

int wsgi_body_pleload(client_t * client, PyObject * wsgi_body)
{
    int err = 0;
    PyObject** body = client->response.body;
    int wsgi_content_length = client->response.wsgi_content_length;

    if (PyBytes_CheckExact(wsgi_body)) {
        body[0] = wsgi_body;
        Py_INCREF(wsgi_body);  // Reasone: wsgi_body inserted into body chunks array
        client->response.body_chunk_num = 1;
        client->response.body_total_size = (int)PyBytes_GET_SIZE(wsgi_body);
        client->response.body_preloaded_size = (int)PyBytes_GET_SIZE(wsgi_body);
        return client->response.body_preloaded_size;
    }
    PyObject* iterator = client->response.body_iterator;
    if (!iterator)
        return -1;

    const char* body_type = Py_TYPE(wsgi_body)->tp_name;
    // wsgi_body types:
    // Flask: ClosingIterator, FileWrapper (buffer_size)
    // Falcon: CloseableStreamIterator (_block_size)
    if (body_type && body_type[0] == 'F' && strcmp(body_type, "FileWrapper") == 0) {
        // FIXME: add custom FileWrapper via wsgi.file_wrapper (sending unlimit body chunks)
        if (PyObject_HasAttrString(wsgi_body, "buffer_size")) {
            if (wsgi_content_length == 0) {
                LOGi("wsgi_body: detect file size = 0");
                return 0;  // zero size file
            }
            if (wsgi_content_length < 0) {
                LOGc("wsgi_body: unknown size of transferred file!");
                return -5;
            }
            if (wsgi_content_length > max_read_file_buffer_size) {
                LOGc("wsgi_body: transferred file is too large! (max len = %d)", max_read_file_buffer_size);
                return -6;
            }
            PyObject * buf_size = PyLong_FromLong(max_read_file_buffer_size);
            int error = PyObject_SetAttrString(wsgi_body, "buffer_size", buf_size);
            Py_DECREF(buf_size);
            if (error) {
                LOGc("wsgi_body: failed to change file read buffer size");
                return -8;
            }
        }
    }
    PyObject* item = NULL;
    int chunks = 0;
    while (item = wsgi_iterator_get_next_chunk(client)) {
        body[chunks++] = item;
        client->response.body_chunk_num = chunks;
        client->response.body_preloaded_size += (int)PyBytes_GET_SIZE(item);
        if (chunks == max_preloaded_body_chunks + 1) {
            // FIXME: add support sending unlimit body chunks
            err = -12;  // overflow!
            break;
        }
    }
    if (client->request.state.error) {
        // wsgi_body: incorrect content
        return -13;
    }
    if (err == 0) {
        LOGi("wsgi_body: response body fully loaded! (size = %d)", client->response.body_preloaded_size);
        client->response.body_total_size = client->response.body_preloaded_size;
        return client->response.body_total_size;
    }
    if (client->response.body_preloaded_size == wsgi_content_length) {
        LOGi("wsgi_body: response body fully loaded! (SIZE = %d)", wsgi_content_length);
        client->response.body_total_size = wsgi_content_length;
        return wsgi_content_length;
    }
    // FIXME: add support send body unknown length
    client->request.state.error = 1;
    LOGc("wsgi_body: body's chunks overflow (max = %d chunks)", max_preloaded_body_chunks);
    return err;
}

int on_message_complete(llhttp_t* parser) {
    LOGi("on message complete");
    PyObject* result;
    client_t * client = (client_t *)parser->data;
    PyObject * headers = client->request.headers;
    client->response.wsgi_content_length = -1;

    if (client->request.state.error)
        goto fin;

    // Truncate input byte stream to real request content length
    result = PyObject_CallMethodObjArgs(client->request.wsgi_input, g_cv.truncate, NULL);
    Py_DECREF(result);

    // Sets the input byte stream position back to 0
    result = PyObject_CallMethodObjArgs(client->request.wsgi_input, g_cv.seek, g_cv.i0, NULL);
    Py_DECREF(result);

    build_wsgi_environ(parser);

    StartResponse* start_response = PyObject_NEW(StartResponse, &StartResponse_Type);
    start_response->called = 0;

    LOGi("calling wsgi application");
    PyObject* wsgi_body = PyObject_CallFunctionObjArgs(g_srv.wsgi_app, headers, start_response, NULL);
    LOGi("called wsgi application");
    client->response.wsgi_body = wsgi_body;

    if (PyErr_Occurred() || wsgi_body == NULL) {
        client->request.state.error = 1;
        goto fin;
    }

    const char* body_type = Py_TYPE(wsgi_body)->tp_name;
    if (body_type == NULL)
        body_type = "<unknown_type_name>";

    if (PyBytes_CheckExact(wsgi_body)) {
        LOGd("wsgi_body: is PyBytes (size = %d)", (int)PyBytes_GET_SIZE(wsgi_body));
    } else {
        if (PyIter_Check(wsgi_body)) {
            LOGd("wsgi_body: is ITERATOR '%s'", body_type);
            client->response.body_iterator = wsgi_body;
        } else {
            LOGd("wsgi_body: type = '%s'", body_type);
            PyObject * iter = PyObject_GetIter(wsgi_body);
            if (iter == NULL) {
                PYTYPE_ERR("wsgi_body: has not iterable type = '%s'", body_type);
                client->request.state.error = 1;
                goto fin;
            }
            client->response.body_iterator = iter;
        }
    }

    int err = get_info_from_wsgi_response(client, start_response);
    if (err) {
        LOGc("response header 'Content-Length' contain incorrect value!");
        PYTYPE_ERR("response headers contain incorrect value!");
        client->request.state.error = 1;
        goto fin;
    }

    int len = wsgi_body_pleload(client, wsgi_body);
    if (len < 0) {
        err = len;
        LOGc("wsgi_body_pleload return error = %d", err);
        client->request.state.error = 1;
        goto fin;
    }

    build_response(client, start_response);

fin:
    if (PyErr_Occurred()) {
        if (client->request.state.error == 0)
            client->request.state.error = 1;
        PyErr_Print();
    }
    if (client->request.state.error) {
        xbuf_reset(&client->response.head);
        reset_response_body(client);
    }
    Py_CLEAR(start_response->headers);
    Py_CLEAR(start_response->status);
    Py_CLEAR(start_response->exc_info);
    Py_CLEAR(start_response);
    Py_CLEAR(client->request.headers);
    return 0;
}

int build_response_ex(void * _client, int flags, int status, const void * headers, const void * body_data, int body_size)
{
    client_t * client = (client_t *)_client;
    xbuf_t * head = &client->response.head;
    xbuf_reset(head);   // reset headers buffer
    PyObject** body = client->response.body;
    StartResponse * response = NULL;

    if (flags & RF_HEADERS_PYLIST) {
        response = (StartResponse *)headers;
        headers = NULL;
    }
    if (response) {
        char scode[4];
        size_t status_len = 0;
        const char * status_code = PyUnicode_AsUTF8AndSize(response->status, &status_len);
        if (status_len < 3)
            return -2;
        memcpy(scode, status_code, 4);
        scode[3] = 0;
        status = atoi(scode);
    }
    if (status == 204 || status == 304) {
        body_size = 0;
        reset_response_body(client);  // forced reset body buffers
    }

    const char * status_name = llhttp_status_name(status);
    if (!status_name)
        return -3;

    char * buf = xbuf_expand(head, 128);
    head->size += sprintf(buf, "HTTP/1.1 %d %s\r\n", status, status_name);

    if (response) {
        Py_ssize_t hsize = PyList_GET_SIZE(response->headers);
        for (Py_ssize_t i = 0; i < hsize; i++) {
            PyObject* tuple = PyList_GET_ITEM(response->headers, i);

            size_t key_len = 0;
            const char * key = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 0), &key_len);

            if (key_len == 14 && key[7] == '-')
                if (strcasecmp(key, "Content-Length") == 0)
                    continue;  // skip "Content-Length" header

            size_t value_len = 0;
            const char * value = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 1), &value_len);

            xbuf_add(head, key, key_len);
            xbuf_add(head, ": ", 2);
            xbuf_add(head, value, value_len);
            xbuf_add(head, "\r\n", 2);

            LOGi("added header '%s: %s'", key, value);
        }
    }
    else if (headers) {
        xbuf_add_str(head, (const char *)headers);
    }

    if (flags & RF_SET_KEEP_ALIVE) {
        xbuf_add_str(head, "Connection: Keep-Alive\r\n");
    } else {
        xbuf_add_str(head, "Connection: close\r\n");
    }

    if (body_size == 0) {
        reset_response_body(client);
    }
    if (body_data && body_size > 0) {
        reset_response_body(client);
        PyObject * buf = PyBytes_FromStringAndSize((const char *)body_data, body_size);
        body[0] = buf;
        client->response.body_chunk_num = 1;
        client->response.body_preloaded_size = body_size;
        client->response.body_total_size = body_size;
    }
    body_size = client->response.body_total_size;  // FIXME: add support "Transfer-Encoding: chunked"

    if (body_size == 0) {
        xbuf_add_str(head, "Content-Length: 0\r\n");
        LOGi("Added Header 'Content-Length: 0'");
    } else {
        char * buf = xbuf_expand(head, 48);
        head->size += sprintf(buf, "Content-Length: %d\r\n", (int)body_size);
        LOGi("Added Header 'Content-Length: %d'", (int)body_size);
    }
    xbuf_add(head, "\r\n", 2);  // end of headers

    LOGt(head->data);
    LOGt_IF(body_size > 0 && client->response.body_chunk_num, PyBytes_AS_STRING(body[0]));
    return head->size;
}

void build_response(client_t * client, StartResponse* response) {
    LOGi("building response");
    int flags = RF_HEADERS_PYLIST;
    if (client->request.state.keep_alive)
        flags |= RF_SET_KEEP_ALIVE;

    int len = build_response_ex(client, flags, 0, response, NULL, -1);
    if (len <= 0) {
        client->request.state.error = 1;
        xbuf_reset(&client->response.head);
        reset_response_body(client);
    }
}


void build_wsgi_environ(llhttp_t * parser)
{
    LOGi("build_wsgi_environ");
    client_t * client = (client_t *)parser->data;
    const char* method = llhttp_method_name(parser->method);
    set_header(client, g_cv.REQUEST_METHOD, method, -1, 0);
    const char* protocol = parser->http_minor == 1 ? "HTTP/1.1" : "HTTP/1.0";
    set_header(client, g_cv.SERVER_PROTOCOL, protocol, -1, 0);
    set_header(client, g_cv.REMOTE_ADDR, client->remote_addr, -1, 0);
}

void init_request_dict()
{
    // only constant values!!!
    g_base_dict = PyDict_New();
    PyDict_SetItem(g_base_dict, g_cv.SCRIPT_NAME, g_cv.empty_string);
    PyDict_SetItem(g_base_dict, g_cv.SERVER_NAME, g_cv.server_host);
    PyDict_SetItem(g_base_dict, g_cv.SERVER_PORT, g_cv.server_port);
    //PyDict_SetItem(g_base_dict, g_cv.wsgi_input, io_BytesIO);   // not const!!!
    PyDict_SetItem(g_base_dict, g_cv.wsgi_version, g_cv.version);
    PyDict_SetItem(g_base_dict, g_cv.wsgi_url_scheme, g_cv.http_scheme);
    PyDict_SetItem(g_base_dict, g_cv.wsgi_errors, PySys_GetObject("stderr"));
    PyDict_SetItem(g_base_dict, g_cv.wsgi_run_once, Py_False);
    PyDict_SetItem(g_base_dict, g_cv.wsgi_multithread, Py_False);
    PyDict_SetItem(g_base_dict, g_cv.wsgi_multiprocess, Py_True);
}

void configure_parser_settings() {
    llhttp_settings_init(&parser_settings);
    parser_settings.on_url = on_url;
    parser_settings.on_body = on_body;
    parser_settings.on_header_field = on_header_field;
    parser_settings.on_header_value = on_header_value;
    parser_settings.on_message_begin = on_message_begin;
    parser_settings.on_message_complete = on_message_complete;
}
