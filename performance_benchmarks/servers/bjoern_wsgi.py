import bjoern


def application(environ, start_response):
    headers = [("Content-Type", "text/plain")]
    start_response("200 OK", headers)
    return [b"Hello, World!"]


if __name__ == "__main__":
    bjoern.run(application, "127.0.0.1", 5000)
