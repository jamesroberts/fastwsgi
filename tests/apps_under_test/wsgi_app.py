def _get(environ, start_repsonse):
    assert environ.get("REQUEST_METHOD") == "GET"
    headers = [("Content-Type", "text/plain")]
    start_repsonse("200 OK", headers)
    return [b"OK"]


def _post(environ, start_repsonse):
    assert environ.get("REQUEST_METHOD") == "POST"
    headers = [("Content-Type", "text/plain")]
    start_repsonse("201 Created", headers)
    return [b"OK"]


def _delete(environ, start_response):
    assert environ.get("REQUEST_METHOD") == "DELETE"
    start_response("204 No Content", [])
    return [b""]


routes = {
    "/get": _get,
    "/post": _post,
    "/delete": _delete,
}


def wsgi_app(environ, start_response):
    app = routes.get(environ["PATH_INFO"])
    return app(environ, start_response)
