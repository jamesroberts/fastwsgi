import bjoern
from flask import Flask

app = Flask(__name__)


@app.get("/")
def hello_world():
    return "Hello, World!", 200


if __name__ == "__main__":
    bjoern.run(app, "127.0.0.1", 5000)
