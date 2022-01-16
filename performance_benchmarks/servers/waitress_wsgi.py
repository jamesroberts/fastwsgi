from waitress import serve

def wsgiapp(environ, start_response):
    headers = [("Content-Type", "text/plain")]
    start_response("200 OK", headers)
    return [b"Hello World"] 

serve(wsgiapp, host='0.0.0.0', port=5000)
