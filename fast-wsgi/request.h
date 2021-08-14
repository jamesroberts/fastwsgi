typedef struct {
    PyObject* headers;
} Request;

Request* new_request(client_t* client);
void free_request(Request*);

llhttp_settings_t parser_settings;
void configure_parser_settings();
int on_message_begin(llhttp_t* parser);
int on_url(llhttp_t* parser, const char* url, size_t length);
int on_body(llhttp_t* parser, const char* body, size_t length);
int on_header_field(llhttp_t* parser, const char* header, size_t length);
int on_header_value(llhttp_t* parser, const char* value, size_t length);
int on_message_complete(llhttp_t* parser);
