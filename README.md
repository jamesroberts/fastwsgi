[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fast-wsgi/context:cpp)
[![Language grade: Python](https://img.shields.io/lgtm/grade/python/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fast-wsgi/context:python)

# Fast WSGI
#### Note: Fast WSGI is still under development...

Fast WSGI is an ultra fast WSGI server for Python 3. 

It is mostly written in C. It makes use of [libuv](https://github.com/libuv/libuv) and [llhttp](https://github.com/nodejs/llhttp) under the hood for blazing fast performance. 



## Example usage with Flask

See [example.py](https://github.com/jamesroberts/fast-wsgi/blob/main/example.py) for more details.

```python
import fast_wsgi
from flask import Flask

app = Flask(__name__)


@app.get("/")
def hello_world():
    return "Hello, World!", 200


if __name__ == "__main__":
    fast_wsgi.run(wsgi_app=app, host="0.0.0.0", port=5000)
```


## Example usage with uWSGI

```python
def application(environ, start_response):
    start_response('200 OK', [('Content-Type', 'text/html')])
    return [b"Hello, World!"]

if __name__ == "__main__":
    fast_wsgi.run(wsgi_app=application, host="0.0.0.0", port=5000)
```


## TODO

- Test integration with other frameworks (uWSGI, Django, etc)
- Comprehensive error handling
- Complete HTTP/1.1 compliance
- Test on multiple platforms (Windows/MacOS)
- Unit Tests
- CI/CD