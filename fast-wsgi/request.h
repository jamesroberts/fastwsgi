typedef struct {
    PyObject* headers;
} Request;

typedef struct {
    PyObject ob_base;
} StartResponse;

Request* new_request(client_t* client);
void free_request(Request*);

void build_wsgi_environ(llhttp_t* parser);

PyObject* current_header;

llhttp_settings_t parser_settings;
void configure_parser_settings();
