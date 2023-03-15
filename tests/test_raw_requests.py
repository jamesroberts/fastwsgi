import socket
import pytest

BAD_REQUEST_RESPONSE = b"HTTP/1.1 400 BAD_REQUEST\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"

raw_requests = [
    "GET\r\n\r\n",
    "/\r\n\r\n",
    "GET ???\r\n\r\n",
    "GET / HTTP\r\n\r\n",
]


def remove_date_header(response: bytes):
    start_index = response.find(b"Date:")
    delimiter = b"\r\n"
    end_index = response.find(delimiter, start_index) + len(delimiter)
    return response[:start_index] + response[end_index:]


@pytest.mark.parametrize("raw_request", raw_requests)
def test_bad_requests(basic_test_server, raw_request):
    host, port = basic_test_server.host, basic_test_server.port
    connection = socket.create_connection((host, port))
    connection.send(raw_request.encode())
    data = connection.recv(4096)
    assert remove_date_header(data) == BAD_REQUEST_RESPONSE
