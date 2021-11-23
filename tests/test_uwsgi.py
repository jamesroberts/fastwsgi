import requests


def wsgi_app(environ, start_response):
    start_response('200 OK', [('Content-Type', 'text/plain')])
    return [b"Hello, WSGI!"]


def wsgi_app_delete(environ, start_response):
    start_response('204 No Content', [])
    return [b""]


def test_uwsgi_get(server_process):
    with server_process(wsgi_app) as server:
        result = requests.get(server.endpoint)
        assert result.status_code == 200
        assert result.text == "Hello, WSGI!"


def test_uwsgi_post(server_process):
    with server_process(wsgi_app) as server:
        # Post with no data
        result = requests.get(server.endpoint)
        assert result.status_code == 200
        assert result.text == "Hello, WSGI!"

        # Post with data
        result = requests.get(server.endpoint, {"test": "data"})
        assert result.status_code == 200
        assert result.text == "Hello, WSGI!"


def test_uwsgi_delete(server_process):
    with server_process(wsgi_app_delete) as server:
        result = requests.get(server.endpoint)
        assert result.status_code == 204
        assert result.text == ""
