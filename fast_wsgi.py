import os
import signal
import _fast_wsgi

from flask import Flask

NUM_WORKERS = 4
HOST = "0.0.0.0"
PORT = 5000
BACKLOG = 1024


def run_multi_process_server(app):
    workers = []

    for _ in range(NUM_WORKERS):
        pid = os.fork()
        if pid > 0:
            workers.append(pid)
            print(f"Worker process added with PID: {pid}")
        else:
            try:
                _fast_wsgi.run_server(app, HOST, PORT, BACKLOG)
            except KeyboardInterrupt:
                exit()

    try:
        for _ in range(NUM_WORKERS):
            os.wait()
    except KeyboardInterrupt:
        print("\nStopping all workers")
        for worker in workers:
            os.kill(worker, signal.SIGINT)


class TestMiddleware:
    def __init__(self, app):
        self.app = app

    def __call__(self, environ, start_response):
        print("Middleware!")
        print(environ)
        response = self.app(environ, start_response)
        print(response)
        return response


app = Flask(__name__)


@app.route("/test")
def hello_world():
    print("Request recieved!")
    return {"message": "Hello, World!"}, 200


app = TestMiddleware(app.wsgi_app)

if __name__ == "__main__":
    print("Starting server...")
    # run_multi_process_server(app)
    _fast_wsgi.run_server(app, HOST, PORT, BACKLOG)
