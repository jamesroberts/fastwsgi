import os
import sys
import signal
import importlib
import click
import _fastwsgi
from pkg_resources import get_distribution

LL_DISABLED    = 0
LL_FATAL_ERROR = 1
LL_CRIT_ERROR  = 2
LL_ERROR       = 3
LL_WARNING     = 4
LL_NOTICE      = 5
LL_INFO        = 6
LL_DEBUG       = 7
LL_TRACE       = 8

class _Server():
    def __init__(self):
        self.app = None
        self.host = "0.0.0.0"
        self.port = 5000
        self.backlog = 1024
        self.loglevel = LL_ERROR
        self.hook_sigint = 2
        self.allow_keepalive = 1
        self.max_content_length = None  # def value: 999999999
        self.max_chunk_size = None      # def value: 256 KiB
        self.read_buffer_size = None    # def value: 64 KiB
        
    def init(self, app, host = None, port = None, backlog = None, loglevel = None):
        self.app = app
        self.host = host if host else self.host
        self.port = port if port else self.port
        self.backlog = backlog if backlog else self.backlog
        self.loglevel = loglevel if loglevel is not None else self.loglevel
        return _fastwsgi.init_server(self)

    def set_allow_keepalive(self, value):
        self.allow_keepalive = value
        _fastwsgi.change_setting(self, "allow_keepalive")

    def run(self):
        ret = _fastwsgi.run_server(self)
        self.close()
        return ret
        
    def close(self):
        return _fastwsgi.close_server(self)

server = _Server()

# -------------------------------------------------------------------------------------

NUM_WORKERS = 4

def run_multi_process_server(app):
    workers = []
    for _ in range(NUM_WORKERS):
        pid = os.fork()
        if pid > 0:
            workers.append(pid)
            print(f"Worker process added with PID: {pid}")
        else:
            try:
                server.init(app)
                server.run()
            except KeyboardInterrupt:
                sys.exit(0)

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


def print_server_details(host, port):
    print(f"\n==== FastWSGI ==== ")
    print(f"Host: {host}\nPort: {port}")
    print("==================\n")

# -------------------------------------------------------------------------------------

@click.command()
@click.version_option(version=get_distribution("fastwsgi").version, message="%(version)s")
@click.option("--host", help="Host the socket is bound to.", type=str, default=server.host, show_default=True)
@click.option("-p", "--port", help="Port the socket is bound to.", type=int, default=server.port, show_default=True)
@click.option("-l", "--loglevel", help="Logging level.", type=int, default=server.loglevel, show_default=True)
@click.argument(
    "wsgi_app_import_string",
    type=str,
    required=True,
)
def run_from_cli(host, port, wsgi_app_import_string, loglevel):
    """
    Run FastWSGI server from CLI
    """
    try:
        wsgi_app = import_from_string(wsgi_app_import_string)
    except ImportError as e:
        print(f"Error importing WSGI app: {e}")
        sys.exit(1)

    print_server_details(host, port)
    server.init(wsgi_app, host, port, loglevel = loglevel)
    print(f"Server listening at http://{host}:{port}")
    server.run()

# -------------------------------------------------------------------------------------

def run(wsgi_app, host = server.host, port = server.port, backlog = server.backlog, loglevel = server.loglevel):
    print_server_details(host, port)
    print(f"Running on PID:", os.getpid())
    server.init(wsgi_app, host, port, backlog, loglevel)
    print(f"Server listening at http://{host}:{port}")
    server.run()    
    # run_multi_process_server(wsgi_app)
