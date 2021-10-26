import requests
from flask import Flask

app = Flask(__name__)


@app.get("/")
def hello_world():
    return "Hello, Flask!", 200


def test_uwsgi_hello_world(server_process):
    with server_process(app) as server:
        result = requests.get(server.endpoint)
        assert result.status_code == 200
        assert result.text == "Hello, Flask!"
