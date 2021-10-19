import os
import signal
import _fast_wsgi

from flask import Flask, request

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
                _fast_wsgi.run_server(app, HOST, PORT, BACKLOG, 0)
            except KeyboardInterrupt:
                exit()

    try:
        for _ in range(NUM_WORKERS):
            os.wait()
    except KeyboardInterrupt:
        print("\nStopping all workers")
        for worker in workers:
            os.kill(worker, signal.SIGINT)


app = Flask(__name__)


@app.route("/test", methods=["GET"])
def hello_world():
    return {"message": "Hello, World!"}, 200


if __name__ == "__main__":
    print("Starting server...")
    # run_multi_process_server(app)
    _fast_wsgi.run_server(app, HOST, PORT, BACKLOG, 1)
