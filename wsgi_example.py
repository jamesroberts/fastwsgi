import fastwsgi

def app(environ, start_response):
    headers = [('Content-Type', 'text/plain')]
    start_response('200 OK', headers)
    return [b'Hello, World!']

if __name__ == '__main__':
    fastwsgi.run(wsgi_app=app, host='0.0.0.0', port=5000)
