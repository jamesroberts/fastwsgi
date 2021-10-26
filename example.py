import fastwsgi
from flask import Flask

app = Flask(__name__)


@app.get("/")
def hello_world():
    return "Hello, World!", 200


def application(environ, start_response):
    start_response('200 OK', [('Content-Type', 'text/html')])
    return [b"Hello, World!"]


if __name__ == "__main__":
    fastwsgi.run(wsgi_app=application, host="0.0.0.0", port=5000)
