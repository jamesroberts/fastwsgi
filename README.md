[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fast-wsgi/context:cpp)
[![Language grade: Python](https://img.shields.io/lgtm/grade/python/g/jamesroberts/fast-wsgi.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/jamesroberts/fast-wsgi/context:python)

# Fast WSGI

Fast WSGI is an ultra fast WSGI server for Python 3. 

It is mostly written in C. It makes use of [libuv](https://github.com/libuv/libuv) and [llhttp](https://github.com/nodejs/llhttp) under the hood for blazing fast performance. 

** Fast WSGI is still under development...

## TODO

- Memory lifecycle management
- Python object ref count tracking
- WSGI app invoking
- Test integration with flask app
- Basic error handling

