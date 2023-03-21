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
        self.tcp_nodelay = 0            # 0 = Nagle's algo enabled; 1 = Nagle's algo disabled;
        self.tcp_keepalive = 0          # -1 = disabled; 0 = system default; 1...N = timeout in seconds
        self.tcp_send_buf_size = 0      # 0 = system default; 1...N = size in bytes
        self.tcp_recv_buf_size = 0      # 0 = system default; 1...N = size in bytes
        self.nowait = 0
        self.num_workers = 1
        self.worker_list = [ ]
        
    def init(self, app, host = None, port = None, loglevel = None, workers = None):
        self.app = app
        self.host = host if host else self.host
        self.port = port if port else self.port
        self.loglevel = loglevel if loglevel is not None else self.loglevel
        self.num_workers = workers if workers is not None else self.num_workers
        if self.num_workers > 1:
            return 0
        return _fastwsgi.init_server(self)

    def set_allow_keepalive(self, value):
        self.allow_keepalive = value
        _fastwsgi.change_setting(self, "allow_keepalive")

    def run(self):
        if self.nowait:
            if self.num_workers > 1:
                raise Exception('Incorrect server options')
            return _fastwsgi.run_nowait(self)
        if self.num_workers > 1:
            return self.multi_run()
        ret = _fastwsgi.run_server(self)
        self.close()
        return ret
        
    def close(self):
        return _fastwsgi.close_server(self)

    def multi_run(self, num_workers = None):
        if num_workers is not None:
            self.num_workers = num_workers
        for _ in range(self.num_workers):
            pid = os.fork()
            if pid > 0:
                self.worker_list.append(pid)
                print(f"Worker process added with PID: {pid}")
                continue
            try:
                _fastwsgi.init_server(self)
                _fastwsgi.run_server(self)
            except KeyboardInterrupt:
                pass
            sys.exit(0)
        try:
            for _ in range(self.num_workers):
                os.wait()
        except KeyboardInterrupt:
            print("\n" + "Stopping all workers")
            for worker in self.worker_list:
                os.kill(worker, signal.SIGINT)
        return 0

server = _Server()

# -------------------------------------------------------------------------------------

def import_from_string(import_str):
    module_str, _, attrs_str = import_str.partition(":")
    if not module_str or not attrs_str:
        raise ImportError("Import string should be in the format <module>:<attribute>")

    try:
        relpath = f"{module_str}.py"
        if os.path.isfile(relpath):
            spec = importlib.util.spec_from_file_location(module_str, relpath)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)

        else:
            module = importlib.import_module(module_str)

        for attr_str in attrs_str.split("."):
            module = getattr(module, attr_str)
    except AttributeError:
        raise ImportError(f'Attribute "{attrs_str}" not found in module "{module_str}"')

    return module

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

    server.init(wsgi_app, host, port, loglevel)
    print(f"FastWSGI server listening at http://{server.host}:{server.port}")
    server.run()

# -------------------------------------------------------------------------------------

def run(wsgi_app, host = None, port = None, loglevel = None, workers = None):
    print("FastWSGI server running on PID:", os.getpid())
    server.init(wsgi_app, host, port, loglevel, workers)
    addon = " multiple workers" if server.num_workers > 1 else ""
    print(f"FastWSGI server{addon} listening at http://{server.host}:{server.port}")
    server.run()
