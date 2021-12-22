import requests


def test_flask_get(flask_test_server):
    url = f"{flask_test_server.endpoint}/get"
    result = requests.get(url)
    assert result.status_code == 200
    assert result.text == "get"

# POST tests are flakey. Need to fix.
def test_flask_post(flask_test_server):
    url = f"{flask_test_server.endpoint}/post"
    result = requests.post(url, json={"test": "data"})
    assert result.status_code == 201
    assert result.text == "post"


def test_flask_delete(flask_test_server):
    url = f"{flask_test_server.endpoint}/delete"
    result = requests.delete(url)
    assert result.status_code == 204
    assert result.text == ""
