import glob
from distutils.core import Extension, setup


SOURCES = glob.glob("fastwsgi/*.c") + glob.glob("llhttp/src/*.c")

module = Extension(
    "_fastwsgi",
    sources=SOURCES,
    libraries=['uv'],
    include_dirs=["llhttp/include"],
    extra_compile_args=["-O3", "-fno-strict-aliasing"]
)

with open("README.md", "r", encoding="utf-8") as read_me:
    long_description = read_me.read()

setup(
    name="fastwsgi",
    version="0.0.1",
    author="James Roberts",
    py_modules=["fastwsgi"],
    ext_modules=[module],
    author_email="jamesroberts.dev@gmail.com",
    description="An ultra fast WSGI server for Python 3",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/jamesroberts/fastwsgi",
    project_urls={
        "Bug Tracker": "https://github.com/jamesroberts/fastwsgi/issues",
    },
    classifiers=[
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Topic :: Internet :: WWW/HTTP :: WSGI :: Server",
        "Programming Language :: Python :: 3",
        "Development Status :: 3 - Alpha",
    ],
    python_requires=">=3.6",
)
