"""
Adapted from https://github.com/cherrypy/cheroot/blob/master/cheroot/test/test_wsgi.py
"""

from cheroot import wsgi

def simple_wsgi_server():
    def app(_environ, start_response):
        status = '200 OK'
        response_headers = [('Content-type', 'text/plain')]
        start_response(status, response_headers)
        return [b'Hello world!']
    server = wsgi.Server(("localhost", 8080), app, timeout=100)
    return server

server = simple_wsgi_server()
server.start()
