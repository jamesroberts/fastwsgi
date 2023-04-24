import os
import glob
from setuptools import setup
from distutils.core import Extension
from distutils.command.build_ext import build_ext
import setup_libuv

SOURCES = glob.glob("fastwsgi/*.c") + glob.glob("llhttp/src/*.c")

module = Extension(
    "_fastwsgi",
    sources=SOURCES,
    include_dirs=["llhttp/include", "libuv/include"],
)

class build_all(build_ext):
    def initialize_options(self):
        build_ext.initialize_options(self)

    def build_extensions(self):
        global module

        setup_libuv.build_libuv(self)

        compiler = self.compiler.compiler_type
        print("Current compiler:", compiler)

        for ext in self.extensions:
            if ext == module:
                if compiler == 'msvc':
                    ext.extra_compile_args = [ '/Oi', '/Oy-', '/W3', '/WX-', '/Gd', '/GS' ]
                    ext.extra_compile_args += [ '/Zc:forScope', '/Zc:inline', '/fp:precise', '/analyze-' ]
                else:
                    ext.extra_compile_args = [ "-O3", "-fno-strict-aliasing", "-fcommon", "-g", "-Wall" ]
                    ext.extra_compile_args += [ "-Wno-unused-function", "-Wno-unused-variable" ]
        
        build_ext.build_extensions(self)


with open("README.md", "r", encoding="utf-8") as read_me:
    long_description = read_me.read()

setup(
    name="fastwsgi",
    version="0.0.9",
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
    cmdclass={"build_ext": build_all},
    entry_points={
        "console_scripts": [
            "fastwsgi = fastwsgi:run_from_cli",
        ],
    },
)
