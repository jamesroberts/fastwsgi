import requests
from flask import Flask

app = Flask(__name__)


@app.get("/")
def hello_world():
    return "Hello, World!", 200


def test_uwsgi_hello_world(server_process):
    with server_process(app):
        result = requests.get("http://127.0.0.1:8080/")
        assert result.status_code == 200
        assert result.text == "Hello, World!"
