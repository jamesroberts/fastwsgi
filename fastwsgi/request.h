#ifndef FASTWSGI_REQUEST_H_
#define FASTWSGI_REQUEST_H_

#include "common.h"
#include "start_response.h"


extern PyObject* base_dict;

void init_request_dict();
void build_wsgi_environ(llhttp_t* parser);

typedef enum {
    RF_EMPTY           = 0x00,
    RF_SET_KEEP_ALIVE  = 0x01,
    RF_HEADERS_PYLIST  = 0x02
} response_flag_t;

int build_response_ex(void * client, int flags, int status, const void * headers, const char * body, int body_size);
void build_response(PyObject* wsgi_response, StartResponse* response, llhttp_t* parser);

llhttp_settings_t parser_settings;
void configure_parser_settings();

void logrepr(int level, PyObject* obj);
#define LOGREPR(_level_, _msg_) if (g_log_level >= _level_) logrepr(_level_, _msg_)

#endif
