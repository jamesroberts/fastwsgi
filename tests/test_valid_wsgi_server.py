import requests

from wsgiref.validate import validator


def simple_app(environ, start_response):
    status = '200 OK'
    headers = [('Content-type', 'text/plain')]
    start_response(status, headers)
    return [b"Valid"]


validator_app = validator(simple_app)


def test_get_valid_wsgi_server(server_process):
    with server_process(validator_app) as server:
        result = requests.get(server.endpoint)
        assert result.text == "Valid"
