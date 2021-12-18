from flask import Flask

app = Flask(__name__)


@app.get("/get")
def get():
    return "get", 200


@app.post("/post")
def post():
    return "post", 201


@app.delete("/delete")
def delete():
    return "delete", 204