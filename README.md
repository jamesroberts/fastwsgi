<p align="center"><img src="./logo.png"></p>

--------------------------------------------------------------------
[![Tests](https://github.com/jamesroberts/fastwsgi/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/jamesroberts/fastwsgi/actions/workflows/tests.yml)
[![Pypi](https://img.shields.io/pypi/v/fastwsgi.svg?style=flat)](https://pypi.python.org/pypi/fastwsgi)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fastwsgi/context:cpp)
[![Language grade: Python](https://img.shields.io/lgtm/grade/python/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fastwsgi/context:python)


# FastWSGI

:construction: FastWSGI is still under development.

FastWSGI is an ultra fast WSGI server for Python 3.  

Its written in C and uses [libuv](https://github.com/libuv/libuv) and [llhttp](https://github.com/nodejs/llhttp) under the hood for blazing fast performance.


## Supported Platforms

| Platform | Linux | MacOs | Windows |
| :------: | :---: | :---: | :-----: |
| <b>Support</b>  | :white_check_mark: |  :white_check_mark: |  :white_check_mark: |


## Performance

FastWSGI is one of the fastest general use WSGI servers out there!

For a comparison against other popular WSGI servers, see [PERFORMANCE.md](./performance_benchmarks/PERFORMANCE.md)


## Installation

Install using the [pip](https://pip.pypa.io/en/stable/) package manager.

```bash
pip install fastwsgi
```


## Quick start

Create a new file `example.py` with the following:

```python
import fastwsgi

def app(environ, start_response):
    headers = [('Content-Type', 'text/plain')]
    start_response('200 OK', headers)
    return [b'Hello, World!']

if __name__ == '__main__':
    fastwsgi.run(wsgi_app=app, host='0.0.0.0', port=5000)
```

Run the server using:

```bash
python3 example.py
```

Or, by using the `fastwsgi` command:

```bash
fastwsgi example:app
```


## Example usage with Flask

See [example.py](https://github.com/jamesroberts/fast-wsgi/blob/main/example.py) for more details.

```python
import fastwsgi
from flask import Flask

app = Flask(__name__)


@app.get('/')
def hello_world():
    return 'Hello, World!', 200


if __name__ == '__main__':
    fastwsgi.run(wsgi_app=app, host='127.0.0.1', port=5000)
```


## Testing

To run the test suite using [pytest](https://docs.pytest.org/en/latest/getting-started.html), run the following command:

```bash
python3 -m pytest
```


## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

Please make sure to update tests where appropriate.


## Building

To build the `.whl` file you must have all dependencies installed and available at your [PATH](https://en.wikipedia.org/wiki/PATH_(variable)) variable.

Besides `Python >=3.6`, these are the dependencies for each platform:
- Windows
    - [Visual Studio](https://visualstudio.microsoft.com/vs/community/) with profile `Desktop development with C++`
- Linux
    - `gcc`
- macOS
    - `clang` (already comes with OS)

With everything alright, you must clone the project and its submodules:

```bash
git clone --recurse-submodules https://github.com/jamesroberts/fastwsgi.git
```

Then go to the repo and build it:

```bash
python setup.py bdist_wheel
```

If everything goes fine you will be able to find your `.whl` in the `dist/` directory.


## Significant Contributions

Special thank you to the following individuals who have made significant contributions to FastWSGI:

- Oleg S. ([@remittor](https://github.com/remittor))


## TODO

- Comprehensive error handling
- Complete HTTP/1.1 compliance
- Unit tests running in CI workflow
