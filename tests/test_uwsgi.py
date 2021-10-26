import requests


def wsgi_app(environ, start_response):
    start_response('200 OK', [('Content-Type', 'text/html')])
    return [b"Hello, WSGI!"]


def test_uwsgi_hello_world(server_process):
    with server_process(wsgi_app) as server:
        result = requests.get(server.endpoint)
        assert result.status_code == 200
        assert result.text == "Hello, WSGI!"
