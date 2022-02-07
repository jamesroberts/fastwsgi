import requests


def test_no_response(general_test_server):
    url = f"{general_test_server.endpoint}/no_response"
    result = requests.get(url)
    assert result.status_code == 200
    assert result.text is ''
