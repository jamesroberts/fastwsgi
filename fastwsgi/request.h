#ifdef _MSC_VER
// strncasecmp is not available on Windows
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

typedef struct {
    PyObject* headers;
    char remote_addr[17];
    llhttp_t parser;
    uv_buf_t response_buffer;
} Request;

typedef struct {
    PyObject ob_base;
    PyObject* status;
    PyObject* headers;
    PyObject* exc_info;
} StartResponse;

PyObject* base_dict;
void init_request_dict();
void build_wsgi_environ(llhttp_t* parser);
void build_response(PyObject* wsgi_response, StartResponse* response, llhttp_t* parser);

char* current_header;

llhttp_settings_t parser_settings;
void configure_parser_settings();
