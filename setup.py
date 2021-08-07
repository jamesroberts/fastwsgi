from distutils.core import Extension, setup


module = Extension("fast_wsgi", sources=["fast-wsgi/fast-wsgimodule.c"])

setup(
    name="Fast-WSGI",
    version="1.0",
    description="This is a package for Fast-WSGI",
    ext_modules=[module]
)
