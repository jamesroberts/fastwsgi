import requests
from flask import Flask

app = Flask(__name__)


@app.get("/")
def get():
    return "get", 200


@app.post("/")
def post():
    return "post", 201


@app.delete("/")
def delete():
    return "", 204


def test_flask_get(server_process):
    with server_process(app) as server:
        result = requests.get(server.endpoint)
        assert result.status_code == 200
        assert result.text == "get"


def test_flask_post(server_process):
    with server_process(app) as server:
        result = requests.post(server.endpoint, json={"test": "data"})
        assert result.status_code == 201
        assert result.text == "post"


def test_flask_delete(server_process):
    with server_process(app) as server:
        result = requests.delete(server.endpoint)
        assert result.status_code == 204
        assert result.text == ""
