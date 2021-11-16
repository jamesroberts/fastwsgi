[![Tests](https://github.com/jamesroberts/fastwsgi/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/jamesroberts/fastwsgi/actions/workflows/tests.yml)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fastwsgi/context:cpp)
[![Language grade: Python](https://img.shields.io/lgtm/grade/python/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fastwsgi/context:python)

# FastWSGI
#### Note: FastWSGI is still under development...

FastWSGI is an ultra fast WSGI server for Python 3. 

It is mostly written in C. It makes use of [libuv](https://github.com/libuv/libuv) and [llhttp](https://github.com/nodejs/llhttp) under the hood for blazing fast performance. 


## Installation

Install using the [pip](https://pip.pypa.io/en/stable/) package manager.

```bash
pip install fastwsgi
```

## Example usage with Flask

See [example.py](https://github.com/jamesroberts/fast-wsgi/blob/main/example.py) for more details.

```python
import fastwsgi
from flask import Flask

app = Flask(__name__)


@app.get("/")
def hello_world():
    return "Hello, World!", 200


if __name__ == "__main__":
    fastwsgi.run(wsgi_app=app, host="127.0.0.1", port=5000)
```


## Example usage with uWSGI

```python
def application(environ, start_response):
    start_response("200 OK", [("Content-Type", "text/html")])
    return [b"Hello, World!"]

if __name__ == "__main__":
    fastwsgi.run(wsgi_app=application, host="127.0.0.1", port=5000)
```

## Testing

To run the test suite using [pytest](https://docs.pytest.org/en/latest/getting-started.html), run the following command:

```bash
python3 -m pytest
```

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

Please make sure to update tests as appropriate.

## TODO

- Comprehensive error handling
- Complete HTTP/1.1 compliance
- Test on multiple platforms (Windows/MacOS)
- Unit tests running in CI workflow