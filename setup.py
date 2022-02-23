import glob
from setuptools import setup
from distutils.core import Extension
from setup_libuv import build_libuv


SOURCES = glob.glob("fastwsgi/*.c") + glob.glob("llhttp/src/*.c")

module = Extension(
    "_fastwsgi",
    sources=SOURCES,
    include_dirs=["llhttp/include", "libuv/include"],
    extra_compile_args=["-O3", "-fno-strict-aliasing", "-fcommon", "-g", "-Wall"],
)

with open("README.md", "r", encoding="utf-8") as read_me:
    long_description = read_me.read()

setup(
    name="fastwsgi",
    version="0.0.6",
    license="MIT",
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
        "Source": "https://github.com/jamesroberts/fastwsgi",
    },
    classifiers=[
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Topic :: Internet :: WWW/HTTP :: WSGI :: Server",
        "Programming Language :: Python :: 3",
        "Development Status :: 3 - Alpha",
    ],
    python_requires=">=3.6",
    install_requires=["click>=7.0"],
    cmdclass={"build_ext": build_libuv},
    entry_points={
        "console_scripts": [
            "fastwsgi = fastwsgi:run_from_cli",
        ],
    },
)
