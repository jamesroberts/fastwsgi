typedef struct {
    PyObject* headers;
} Request;

typedef struct {
    PyObject ob_base;
    PyObject* status;
    PyObject* headers;
    PyObject* exc_info;
} StartResponse;

Request* new_request(client_t* client);
void free_request(Request*);

void build_wsgi_environ(llhttp_t* parser);
void build_response(PyObject* wsgi_response, StartResponse* response);

PyObject* current_header;

llhttp_settings_t parser_settings;
void configure_parser_settings();
