import os
import signal
import _fastwsgi

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
                _fastwsgi.run_server(app, HOST, PORT, BACKLOG, 0)
            except KeyboardInterrupt:
                exit()

    try:
        for _ in range(NUM_WORKERS):
            os.wait()
    except KeyboardInterrupt:
        print("\nStopping all workers")
        for worker in workers:
            os.kill(worker, signal.SIGINT)


def run(wsgi_app, host, port, backlog=1024):
    print("Starting server...")
    enable_logging = 0
    _fastwsgi.run_server(wsgi_app, host, port, backlog, enable_logging)
    # run_multi_process_server(wsgi_app)
