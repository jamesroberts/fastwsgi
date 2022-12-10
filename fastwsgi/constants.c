#include "constants.h"

cvar_t g_cv;

static int g_cv_inited = 0;

void init_constants()
{
    if (g_cv_inited)
        return;

    g_cv_inited = 1;
    g_cv.REQUEST_METHOD = PyUnicode_FromString("REQUEST_METHOD");
    g_cv.SCRIPT_NAME = PyUnicode_FromString("SCRIPT_NAME");
    g_cv.SERVER_NAME = PyUnicode_FromString("SERVER_NAME");
    g_cv.SERVER_PORT = PyUnicode_FromString("SERVER_PORT");
    g_cv.SERVER_PROTOCOL = PyUnicode_FromString("SERVER_PROTOCOL");
    g_cv.QUERY_STRING = PyUnicode_FromString("QUERY_STRING");
    g_cv.PATH_INFO = Py_BuildValue("s", "PATH_INFO");
    g_cv.HTTP_ = PyUnicode_FromString("HTTP_");
    g_cv.REMOTE_ADDR = PyUnicode_FromString("REMOTE_ADDR");
    g_cv.CONTENT_LENGTH = PyUnicode_FromString("CONTENT_LENGTH");

    g_cv.wsgi_version = PyUnicode_FromString("wsgi.version");
    g_cv.wsgi_url_scheme = PyUnicode_FromString("wsgi.url_scheme");
    g_cv.wsgi_errors = PyUnicode_FromString("wsgi.errors");
    g_cv.wsgi_run_once = PyUnicode_FromString("wsgi.run_once");
    g_cv.wsgi_multithread = PyUnicode_FromString("wsgi.multithread");
    g_cv.wsgi_multiprocess = PyUnicode_FromString("wsgi.multiprocess");
    g_cv.wsgi_input = PyUnicode_FromString("wsgi.input");
    g_cv.version = PyTuple_Pack(2, PyLong_FromLong(1), PyLong_FromLong(0));

    g_cv.http_scheme = PyUnicode_FromString("http");
    g_cv.HTTP_1_1 = PyUnicode_FromString("HTTP/1.1");
    g_cv.HTTP_1_0 = PyUnicode_FromString("HTTP/1.0");

    g_cv.server_host = PyUnicode_FromString("0.0.0.0");
    g_cv.server_port = PyUnicode_FromString("5000");
    g_cv.empty_string = PyUnicode_FromString("");

    g_cv.module_io = PyImport_ImportModule("io");
    g_cv.BytesIO = PyUnicode_FromString("BytesIO");
    g_cv.close = PyUnicode_FromString("close");
    g_cv.write = PyUnicode_FromString("write");
    g_cv.read = PyUnicode_FromString("read");
    g_cv.truncate = PyUnicode_FromString("truncate");
    g_cv.seek = PyUnicode_FromString("seek");
    g_cv.tell = PyUnicode_FromString("tell");
    g_cv.buffer_size = PyUnicode_FromString("buffer_size");
    g_cv.getvalue = PyUnicode_FromString("getvalue");
    g_cv.comma = PyUnicode_FromString(",");

    g_cv.i0 = PyLong_FromLong(0L);
}
