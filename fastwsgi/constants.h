#include <Python.h>

PyObject* REQUEST_METHOD, * SCRIPT_NAME, * SERVER_NAME, * SERVER_PORT, * SERVER_PROTOCOL;
PyObject* wsgi_version, * wsgi_url_scheme, * wsgi_errors, * wsgi_run_once, * wsgi_multithread, * wsgi_multiprocess, * version;
PyObject* http_scheme, * HTTP_1_1, * HTTP_1_0;
PyObject* server_host, * server_port, * empty_string;
PyObject* HTTP_, * PATH_INFO, * wsgi_input;

void init_constants();