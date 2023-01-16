#ifndef FASTWSGI_LOGX_H_
#define FASTWSGI_LOGX_H_

#include "common.h"

// log levels the same as syslog
#define LL_FATAL_ERROR 1
#define LL_CRIT_ERROR  2
#define LL_ERROR       3
#define LL_WARNING     4
#define LL_NOTICE      5
#define LL_INFO        6
#define LL_DEBUG       7
#define LL_TRACE       8 

#ifndef MAX_LOG_LEVEL
#ifdef FASTWSGI_DEBUG
#define MAX_LOG_LEVEL   LL_TRACE
#else
#define MAX_LOG_LEVEL   LL_INFO
#endif
#endif

#define LL_STRING   "#FCEwnidt"

typedef enum {
    FW_LOG_TO_STDOUT = 0,
    FW_LOG_TO_SYSLOG = 1
} log_type_t;

extern int g_log_level;
void set_log_level(int level);
void set_log_client_addr(const char * addr);
void logmsg(int level, const char * fmt, ...);

#define LOGMSG(_level_, ...) if (_level_ <= g_log_level) logmsg(_level_, __VA_ARGS__)

#define logger(_msg_) LOGMSG(LL_INFO, _msg_)


#define LOGX_IF(_level_, _cond_, ...)   if (_cond_) LOGMSG(_level_, __VA_ARGS__)
#define LOGX(_level_, ...)              LOGMSG(_level_, __VA_ARGS__)


#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_TRACE
#define LOGt(...)                  LOGX(LL_TRACE, __VA_ARGS__)
#define LOGt_IF(_cond_, ...)    LOGX_IF(LL_TRACE, (_cond_), __VA_ARGS__)
#else
#define LOGt(...)               do{}while(0)
#define LOGt_IF(_cond_, ...)    do{}while(0)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_DEBUG
#define LOGd(...)                  LOGX(LL_DEBUG, __VA_ARGS__)
#define LOGd_IF(_cond_, ...)    LOGX_IF(LL_DEBUG, (_cond_), __VA_ARGS__)
#else
#define LOGd(...)               do{}while(0)
#define LOGd_IF(_cond_, ...)    do{}while(0)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_INFO
#define LOGi(...)                  LOGX(LL_INFO, __VA_ARGS__)
#define LOGi_IF(_cond_, ...)    LOGX_IF(LL_INFO, (_cond_), __VA_ARGS__)
#else
#define LOGi(...)               do{}while(0)
#define LOGi_IF(_cond_, ...)    do{}while(0)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_NOTICE
#define LOGn(...)                  LOGX(LL_NOTICE, __VA_ARGS__)
#define LOGn_IF(_cond_, ...)    LOGX_IF(LL_NOTICE, (_cond_), __VA_ARGS__)
#else
#define LOGn(...)               do{}while(0)
#define LOGn_IF(_cond_, ...)    do{}while(0)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_WARNING
#define LOGw(...)                  LOGX(LL_WARNING, __VA_ARGS__)
#define LOGw_IF(_cond_, ...)    LOGX_IF(LL_WARNING, (_cond_), __VA_ARGS__)
#else
#define LOGw(...)               do{}while(0)
#define LOGw_IF(_cond_, ...)    do{}while(0)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_ERROR
#define LOGe(...)                  LOGX(LL_ERROR, __VA_ARGS__)
#define LOGe_IF(_cond_, ...)    LOGX_IF(LL_ERROR, (_cond_), __VA_ARGS__)
#else
#define LOGe(...)               do{}while(0)
#define LOGe_IF(_cond_, ...)    do{}while(0)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_CRIT_ERROR
#define LOGc(...)                  LOGX(LL_CRIT_ERROR, __VA_ARGS__)
#define LOGc_IF(_cond_, ...)    LOGX_IF(LL_CRIT_ERROR, (_cond_), __VA_ARGS__)
#else
#define LOGc(...)               do{}while(0)
#define LOGc_IF(_cond_, ...)    do{}while(0)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_FATAL_ERROR
#define LOGf(...)                  LOGX(LL_FATAL_ERROR, __VA_ARGS__)
#define LOGf_IF(_cond_, ...)    LOGX_IF(LL_FATAL_ERROR, (_cond_), __VA_ARGS__)
#else
#define LOGf(...)               do{}while(0)
#define LOGf_IF(_cond_, ...)    do{}while(0)
#endif

#endif
