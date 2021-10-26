import glob
from distutils.core import Extension, setup


SOURCE_FILES = glob.glob("fastwsgi/*.c") + glob.glob("llhttp/src/*.c")

module = Extension(
    "_fastwsgi",
    sources=SOURCE_FILES,
    libraries=['uv'],
    include_dirs=["llhttp/include"],
    extra_compile_args=["-O3", "-fno-strict-aliasing"]
)

setup(
    name="fastwsgi",
    version="0.1",
    description="An ultra fast WSGI server for Python 3",
    ext_modules=[module]
)
