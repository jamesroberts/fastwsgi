"""
Adapted from https://github.com/cherrypy/cheroot/blob/master/cheroot/test/test_wsgi.py
"""

from cheroot import wsgi
from gunicorn_wsgi import application

server = wsgi.Server(bind_addr=("localhost", 8080), wsgi_app=application)
server.start()
