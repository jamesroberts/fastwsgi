#include "common.h"
#include "llhttp.h"

void logrepr(int level, PyObject* obj)
{
    PyObject* repr = PyObject_Repr(obj);
    PyObject* str = PyUnicode_AsEncodedString(repr, "utf-8", "~E~");
    const char* bytes = PyBytes_AS_STRING(str);
    LOGX(level, "REPR: %s", bytes);
    Py_XDECREF(repr);
    Py_XDECREF(str);
}

const char * get_http_status_name(int status)
{
#define HTTP_STATUS_GEN(NUM, NAME, STRING) case HTTP_STATUS_##NAME: return #STRING;
    switch (status) {
        HTTP_STATUS_MAP(HTTP_STATUS_GEN)
    default: return NULL;
    }
#undef HTTP_STATUS_GEN
    return NULL;
}

int64_t get_env_int(const char * name)
{
    int64_t v;
    char buf[128];
    size_t len = sizeof(buf) - 1;
    int rv = uv_os_getenv(name, buf, &len);
    if (rv == UV_ENOENT)
        return -1;  // env not found
    if (rv != 0 || len == 0)
        return -2;
    if (len == 1 && buf[0] == '0')
        return 0;
    if (len > 2 && buf[0] == '0' && buf[1] == 'x') {
        v = strtoll(buf + 2, NULL, 16);
    } else {
        v = strtoll(buf, NULL, 10);
    }
    if (v <= 0 || v == LLONG_MAX)
        return -3;
    return v;
}

