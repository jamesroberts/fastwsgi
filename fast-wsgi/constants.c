#include "constants.h"

void init_constants() {
    REQUEST_METHOD = PyUnicode_FromString("REQUEST_METHOD");
    SCRIPT_NAME = PyUnicode_FromString("SCRIPT_NAME");
    SERVER_NAME = PyUnicode_FromString("SERVER_NAME");
    SERVER_PORT = PyUnicode_FromString("SERVER_PORT");
    SERVER_PROTOCOL = PyUnicode_FromString("SERVER_PROTOCOL");
    PATH_INFO = Py_BuildValue("s", "PATH_INFO");
    HTTP_ = PyUnicode_FromString("HTTP_");

    wsgi_version = PyUnicode_FromString("wsgi.version");
    wsgi_url_scheme = PyUnicode_FromString("wsgi.url_scheme");
    wsgi_errors = PyUnicode_FromString("wsgi.errors");
    wsgi_run_once = PyUnicode_FromString("wsgi.run_once");
    wsgi_multithread = PyUnicode_FromString("wsgi.multithread");
    wsgi_multiprocess = PyUnicode_FromString("wsgi.multiprocess");
    wsgi_input = PyUnicode_FromString("wsgi.input");
    version = PyTuple_Pack(2, PyLong_FromLong(1), PyLong_FromLong(0));

    http_scheme = PyUnicode_FromString("http");
    HTTP_1_1 = PyUnicode_FromString("HTTP/1.1");
    HTTP_1_0 = PyUnicode_FromString("HTTP/1.0");

    server_host = PyUnicode_FromString("0.0.0.0");
    server_port = PyUnicode_FromString("5000");
    empty_string = PyUnicode_FromString("");
}