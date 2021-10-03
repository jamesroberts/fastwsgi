typedef struct {
    PyObject* headers;
} Request;

typedef struct {
    PyObject_HEAD;
    PyObject* headers;
    PyObject* status;
    PyObject* exc_info;
    PyObject* body;
} Response;

typedef struct {
    PyObject_HEAD;
    PyObject* status;
    PyObject* headers;
    PyObject* exc_info;
    Response* response;
} StartResponse;

Request* new_request(client_t* client);
void free_request(Request*);

PyObject* base_dict;
void init_request_dict();
void build_wsgi_environ(llhttp_t* parser);
void build_response(PyObject* wsgi_response, Response* response);

char* current_header;
Response* response;

llhttp_settings_t parser_settings;
void configure_parser_settings();
