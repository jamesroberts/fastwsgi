import requests


def test_no_response(general_test_server):
    url = f"{general_test_server.endpoint}/no_response"
    result = requests.get(url)
    assert result.status_code == 200
    assert result.text is ''

def test_invalid_return_type(general_test_server):
    url = f"{general_test_server.endpoint}/invalid_return_type"
    result = requests.get(url)
    assert result.status_code == 500
