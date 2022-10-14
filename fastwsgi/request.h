#ifndef FASTWSGI_REQUEST_H_
#define FASTWSGI_REQUEST_H_

#ifdef _MSC_VER
// strncasecmp is not available on Windows
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#include "start_response.h"


PyObject* base_dict;
void init_request_dict();
void build_wsgi_environ(llhttp_t* parser);
void build_response(PyObject* wsgi_response, StartResponse* response, llhttp_t* parser);

llhttp_settings_t parser_settings;
void configure_parser_settings();

void logrepr(int level, PyObject* obj);
#define LOGREPR(_level_, _msg_) if (g_log_level >= _level_) logrepr(_level_, _msg_)

#endif
