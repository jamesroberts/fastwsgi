import requests


def wsgi_app(environ, start_response):
    start_response('200 OK', [('Content-Type', 'text/html')])
    return [b"Hello, World!"]


def test_uwsgi_hello_world(server_process):
    with server_process(wsgi_app):
        result = requests.get("http://127.0.0.1:8080/")
        assert result.status_code == 200
        assert result.text == "Hello, World!"
