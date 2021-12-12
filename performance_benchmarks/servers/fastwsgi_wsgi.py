import fastwsgi


def application(environ, start_response):
    headers = [("Content-Type", "text/plain")]
    start_response("200 OK", headers)
    return [b"Hello, World!"]


if __name__ == "__main__":
    fastwsgi.run(wsgi_app=application, host="127.0.0.1", port=5000)
