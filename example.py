import fastwsgi
from flask import Flask

app = Flask(__name__)


@app.get("/")
def hello_world():
    return "Hello, World!", 200


if __name__ == "__main__":
    fastwsgi.run(wsgi_app=app, host="0.0.0.0", port=5000)
