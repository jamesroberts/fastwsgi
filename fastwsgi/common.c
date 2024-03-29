#include "common.h"
#include "llhttp.h"
#include <time.h>

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

int64_t get_obj_attr_int(PyObject * obj, const char * name)
{
    int64_t hr = 0;
    PyObject * attr = PyObject_GetAttrString(obj, name);
    FIN_IF(!attr, LLONG_MIN);  // error
    FIN_IF(attr == Py_True, 1);
    FIN_IF(attr == Py_False, 0);
    FIN_IF(!PyLong_CheckExact(attr), LLONG_MIN);
    hr = PyLong_AsSsize_t(attr);
fin:
    Py_XDECREF(attr);
    return hr;
}

const char * get_obj_attr_str(PyObject * obj, const char * name)
{
    PyObject * attr = PyObject_GetAttrString(obj, name);
    Py_XDECREF(attr);
    if (!attr || !PyUnicode_CheckExact(attr)) {
        return NULL;
    }
    return PyUnicode_AsUTF8(attr);
}

static const char weekDays[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char monthList[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

time_t g_actual_time = 0;
char g_actual_asctime[32] = { 0 };
int g_actual_asctime_len = 0;

int get_asctime(char ** asc_time)
{
    time_t curr_time = time(NULL);
    if (curr_time == g_actual_time) {
        *asc_time = g_actual_asctime;
        return g_actual_asctime_len;
    }
    struct tm tv;
#ifdef _WIN32
    gmtime_s(&tv, &curr_time);
#else
    gmtime_r(&curr_time, &tv);
#endif
    char buf[64];
    int len = sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
        weekDays[tv.tm_wday], tv.tm_mday, monthList[tv.tm_mon],
        1900 + tv.tm_year, tv.tm_hour, tv.tm_min, tv.tm_sec);
    if (len > 0 && len < 32) {
        g_actual_time = curr_time;
        g_actual_asctime_len = len;
        memcpy(g_actual_asctime, buf, 32);
        *asc_time = g_actual_asctime;
        return len;
    }
    *asc_time = "";
    return 0;
}

PyObject * get_function(PyObject * object)
{
    if (PyFunction_Check(object)) {
        Py_INCREF(object);
        return object;
    }
    if (PyMethod_Check(object)) {
        PyObject * met = PyMethod_GET_FUNCTION(object);
        Py_INCREF(met);
        return met;
    }
    PyObject * call = PyObject_GetAttrString(object, "__call__");
    if (call) {
        if (PyFunction_Check(call))
            return call;

        if (PyMethod_Check(call)) {
            PyObject * met = PyMethod_GET_FUNCTION(call);
            if (PyFunction_Check(met)) {
                Py_INCREF(met);
                Py_DECREF(call);
                return met;
            }
        }
        Py_DECREF(call);
    }
    return NULL;
}
