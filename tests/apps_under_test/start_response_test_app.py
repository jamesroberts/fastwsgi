def _no_args(environ, start_response):
    start_response()
    return [b"OK"]

def _invalid_status(environ, start_response):
    headers = [("Content-Type", "text/plain")]
    start_response("x", headers)
    return [b"OK"]

def _valid_headers(environ, start_response):
    headers = [("Content-Type", "text/plain"), ("Server", "FastWSGI")]
    start_response("200 OK", headers)
    return [b"OK"]

def _empty_headers(environ, start_response):
    start_response("200 OK", [])
    return [b"OK"]

def _no_headers(environ, start_response):
    start_response("200 OK", None)
    return [b"OK"]

def _wrong_header_type(environ, start_response):
    start_response("200 OK", ("Wrong", "Headers"))
    return [b"OK"]

def _wrong_header_value_type(environ, start_response):
    start_response("200 OK", [("too", "many", "values")])
    return [b"OK"]

def _wrong_exc_info_type(environ, start_response):
    start_response("200 OK", [], "Wrong")
    return [b"Error"]


routes = {
    "/no_args": _no_args,
    "/invalid_status": _invalid_status,
    "/valid_headers": _valid_headers,
    "/empty_headers": _empty_headers,
    "/no_headers": _no_headers,
    "/wrong_headers": _wrong_header_type,
    "/wrong_header_values": _wrong_header_value_type,
    "/wrong_exc_info_type": _wrong_exc_info_type,
}


def start_response_app(environ, start_response):
    app = routes.get(environ["PATH_INFO"])
    return app(environ, start_response)
