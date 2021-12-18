import os
import sys
import pytest
import fastwsgi
from enum import Enum

from contextlib import contextmanager
from multiprocessing import Process, set_start_method

from tests.apps_under_test import (
    basic_app,
    wsgi_app,
    flask_app,
    validator_app,
)

HOST = "127.0.0.1"
PORT = 5000


class ServerProcess:
    def __init__(self, application, host=HOST, port=PORT) -> None:
        self.process = Process(target=fastwsgi.run, args=(application, host, port))
        self.endpoint = f"http://{host}:{port}"
        self.host = host
        self.port = port

    def start(self):
        self.process.start()

    def kill(self):
        self.process.kill()


class Servers(Enum):
    BASIC_TEST_SERVER = 1
    WSGI_TEST_SERVER = 2
    FLASK_TEST_SERVER = 3
    VALIDATOR_TEST_SERVER = 4


servers = {
    Servers.BASIC_TEST_SERVER: basic_app,
    Servers.WSGI_TEST_SERVER: wsgi_app,
    Servers.FLASK_TEST_SERVER: flask_app,
    Servers.VALIDATOR_TEST_SERVER: validator_app,
}


@contextmanager
def mute_stdout():
    old_out = sys.stdout
    sys.stdout = open(os.devnull, "w")
    yield
    sys.stdout = old_out


def pytest_sessionstart(session):
    set_start_method("fork")
    for i, server in enumerate(servers.items()):
        with mute_stdout():
            name, app = server
            server_process = ServerProcess(app, port=PORT + i)
            server_process.start()
        print(f"{name} is listening on port={PORT+i}")
        servers[name] = server_process


def pytest_sessionfinish(session, exitstatus):
    for server_process in servers.values():
        server_process.kill()


@pytest.fixture
def server_process():
    return servers.get(Servers.BASIC_TEST_SERVER)


@pytest.fixture
def basic_test_server():
    return servers.get(Servers.BASIC_TEST_SERVER)


@pytest.fixture
def flask_test_server():
    return servers.get(Servers.FLASK_TEST_SERVER)


@pytest.fixture
def wsgi_test_server():
    return servers.get(Servers.WSGI_TEST_SERVER)


@pytest.fixture
def validator_test_server():
    return servers.get(Servers.VALIDATOR_TEST_SERVER)
