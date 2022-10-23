#include "logx.h"
#ifdef _WIN32
#include <vadefs.h>
#endif

int g_log_level = 0;
log_type_t g_log_type = FW_LOG_TO_STDOUT;

static const char g_log_level_str[] = LL_STRING "--------------------------";

void set_log_type(int type)
{
    g_log_type = (log_type_t)type;
}

void set_log_level(int level)
{
    if (level < 0) {
        g_log_level = 0;
        return;
    }
    if (level >= 1000) {
        int type = level / 1000;
        if (type < 9)
            set_log_type(type);
        level = level % 1000;
    }
    if (level > LL_TRACE) {
        g_log_level = LL_TRACE;
        return;
    }
    g_log_level = level;
}

static const char log_prefix[] = "[FWSGI-X]";

void logmsg(int level, const char * fmt, ...)
{
    char buf[4096];
    if (level <= g_log_level) {
        va_list argptr;
        va_start(argptr, fmt);
        const int prefix_len = sizeof(log_prefix);     // include space
        memcpy(buf, log_prefix, prefix_len);           // add prefix
        buf[prefix_len - 3] = g_log_level_str[level];  // replace X to error type
        buf[prefix_len - 1] = 0x20;                    // add space delimiter
        int maxlen = sizeof(buf) - prefix_len - 8;
        int len = vsnprintf(buf + prefix_len, maxlen, fmt, argptr);
        va_end(argptr);
        if (len < 0) {
            buf[maxlen] = 0;
            len = (int)strlen(buf);
        } else {
            len += prefix_len;
            buf[len] = 0;
        }
        if (buf[len - 1] != '\n') {
            buf[len++] = '\n';
            buf[len] = 0;
        }
        if (g_log_type == FW_LOG_TO_SYSLOG) {
#ifdef _WIN32
            OutputDebugStringA(buf);
#else
            //FIXME: openlog, syslog
#endif
        } else {
            if (level <= LL_ERROR)
                fputs(buf, stderr);
            else
                fputs(buf, stdout);
        }
    }
}
