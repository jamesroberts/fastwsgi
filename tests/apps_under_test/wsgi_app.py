def _get(environ, start_response):
    assert environ.get("REQUEST_METHOD") == "GET"
    headers = [("Content-Type", "text/plain")]
    start_response("200 OK", headers)
    return [b"OK"]


def _get_byte_string(environ, start_response):
    headers = [("Content-Type", "text/plain")]
    start_response("200 OK", headers)
    return b"basic byte string"


def _post(environ, start_response):
    assert environ.get("REQUEST_METHOD") == "POST"
    headers = [("Content-Type", "text/plain")]
    start_response("201 Created", headers)
    return [b"OK"]


def _delete(environ, start_response):
    assert environ.get("REQUEST_METHOD") == "DELETE"
    start_response("204 No Content", [])
    return [b""]


routes = {
    "/get": _get,
    "/post": _post,
    "/delete": _delete,
    "/get_byte_string": _get_byte_string,
}


def wsgi_app(environ, start_response):
    app = routes.get(environ["PATH_INFO"])
    return app(environ, start_response)
