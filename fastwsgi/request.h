#ifndef FASTWSGI_REQUEST_H_
#define FASTWSGI_REQUEST_H_

#include "common.h"
#include "start_response.h"


extern PyObject* g_base_dict;

void init_request_dict();
void configure_parser_settings(llhttp_settings_t * ps);
void close_iterator(PyObject * iterator);

#endif
