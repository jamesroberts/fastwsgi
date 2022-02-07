def _no_response(environ, start_response):
    start_response("200 OK", [])
    return []


routes = {
    "/no_response": _no_response,
}


def general_test_app(environ, start_response):
    app = routes.get(environ["PATH_INFO"])
    return app(environ, start_response)
