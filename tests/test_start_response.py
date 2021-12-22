import requests

def test_no_start_response_args(start_response_server):
    url = f"{start_response_server.endpoint}/no_args"
    result = requests.get(url)
    assert result.status_code == 500

def test_invalid_status(start_response_server):
    url = f"{start_response_server.endpoint}/invalid_status"
    result = requests.get(url)
    assert result.status_code == 500

def test_valid_headers(start_response_server):
    url = f"{start_response_server.endpoint}/valid_headers"
    result = requests.get(url)
    assert result.status_code == 200
    assert result.text == "OK"    

def test_empty_headers(start_response_server):
    url = f"{start_response_server.endpoint}/empty_headers"
    result = requests.get(url)
    assert result.status_code == 200
    assert result.text == "OK"

def test_no_headers(start_response_server):
    url = f"{start_response_server.endpoint}/no_headers"
    result = requests.get(url)
    assert result.status_code == 500
    
def test_wrong_header_type(start_response_server):
    url = f"{start_response_server.endpoint}/wrong_headers"
    result = requests.get(url)
    assert result.status_code == 500
    
def test_wrong_header_value_types(start_response_server):
    url = f"{start_response_server.endpoint}/wrong_header_values"
    result = requests.get(url)
    assert result.status_code == 500

def test_exc_info(start_response_server):
    url = f"{start_response_server.endpoint}/wrong_exc_info_type"
    result = requests.get(url)
    assert result.status_code == 500
    



