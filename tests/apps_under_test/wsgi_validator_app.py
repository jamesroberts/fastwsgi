from wsgiref.validate import validator


def simple_app(environ, start_response):
    status = "200 OK"
    headers = [("Content-type", "text/plain")]
    start_response(status, headers)
    return [b"Valid"]


validator_app = validator(simple_app)