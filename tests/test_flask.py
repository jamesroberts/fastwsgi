import requests
from flask import Flask

app = Flask(__name__)


@app.get("/")
def get():
    return "get", 200


@app.post("/")
def post():
    return "post", 201


def test_flask_get(server_process):
    with server_process(app) as server:
        result = requests.get(server.endpoint)
        assert result.status_code == 200
        assert result.text == "get"


def test_flask_post(server_process):
    with server_process(app) as server:
        # Post with no data
        result = requests.post(server.endpoint)
        assert result.status_code == 201
        assert result.text == "post"

        # Post with data
        result = requests.post(server.endpoint, {"test": "data"})
        assert result.status_code == 201
        assert result.text == "post"
