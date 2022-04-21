def _no_response(environ, start_response):
    start_response("200 OK", [])
    return []

def _invalid_return_type(environ, start_response):
    start_response("200 OK", [])
    return "non-bytestring"

routes = {
    "/no_response": _no_response,
    "/invalid_return_type": _invalid_return_type,
}


def general_test_app(environ, start_response):
    app = routes.get(environ["PATH_INFO"])
    return app(environ, start_response)
