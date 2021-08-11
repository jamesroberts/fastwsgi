typedef struct {
    PyObject* headers;
} Request;

Request* new_request(client_t* client);
void free_request(Request*);