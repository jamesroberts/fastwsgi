import os
import sys
import signal
import importlib
import _fastwsgi

NUM_WORKERS = 4
HOST = "0.0.0.0"
PORT = 5000
BACKLOG = 1024
LOGGING = 0


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


def import_from_string(import_str):
    module_str, _, attrs_str = import_str.partition(":")
    if not module_str or not attrs_str:
        raise ImportError("Import string should be in the format <module>:<attribute>")

    try:
        module = importlib.import_module(module_str)
        for attr_str in attrs_str.split("."):
            module = getattr(module, attr_str)
    except AttributeError:
        raise ImportError(f'Attribute "{attrs_str}" not found in module "{module_str}"')

    return module


def print_server_details():
    print(f"\n==== FastWSGI ==== ")
    print(f"Host: {HOST}\nPort: {PORT}")
    print("==================\n")


def run_from_cli():
    if len(sys.argv[1:]) < 1:
        raise ValueError("No import string provided")

    sys.path.insert(0, ".")
    wsgi_app = import_from_string(sys.argv[1])
    print_server_details()
    print(f"Server listening at http://{HOST}:{PORT}")
    _fastwsgi.run_server(wsgi_app, "", PORT, BACKLOG, LOGGING)


def run(wsgi_app, host, port, backlog=1024):
    print_server_details()
    print(f"Server listening at http://{host}:{port}")
    _fastwsgi.run_server(wsgi_app, host, port, backlog, LOGGING)
    # run_multi_process_server(wsgi_app)
