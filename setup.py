from distutils.core import Extension, setup


SOURCE_FILES = ['fast-wsgi/fast-wsgimodule.c',
                'fast-wsgi/server.c',
                'llhttp/src/api.c',
                'llhttp/src/http.c',
                'llhttp/src/llhttp.c']

module = Extension(
    "fast_wsgi",
    sources=SOURCE_FILES,
    libraries=['uv'],
    include_dirs=["llhttp/include", "/usr/include"],
)

setup(
    name="fast_wsgi",
    version="1.0",
    description="This is a package for fast_wsgi",
    ext_modules=[module]
)
