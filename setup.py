import glob
from distutils.core import Extension, setup


SOURCE_FILES = glob.glob("fast-wsgi/*.c") + glob.glob("llhttp/src/*.c")

module = Extension(
    "_fast_wsgi",
    sources=SOURCE_FILES,
    libraries=['uv'],
    include_dirs=["llhttp/include"],
    extra_compile_args=["-O3", "-fno-strict-aliasing"]
)

setup(
    name="fast_wsgi",
    version="0.1",
    description="An ultra fast WSGI server for Python 3",
    ext_modules=[module]
)
