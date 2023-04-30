import os
import sys
import glob
import subprocess
import platform
import re
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

def get_compiler_version(exe_name):
    version = None
    ver_major = None
    ver_minor = None
    try:
        res = subprocess.run([exe_name, '--version'], stdout=subprocess.PIPE)
        if res.returncode == 0:
            version = res.stdout.decode('utf8')
            ver_s = re.search(r"[\D+](\d+)[.](\d+)[\D+]", version)
            if ver_s:
                ver_major = int(ver_s.group(1))
                ver_minor = int(ver_s.group(2))
    except:
        pass
    return version, ver_major, ver_minor


current_compiler = os.getenv('CC', "")

# Forced use clang compiler, if installed
if platform.system() == "Linux" and current_compiler == "":
    for cc_ver in range(40, 7, -1):
        cc = 'clang-%d' % cc_ver
        text, ver_major, ver_minor = get_compiler_version(cc)
        if text and ver_major:
            current_compiler = cc
            print('Change current compiler to {}'.format(cc))
            os.environ['CC'] = cc
            break


class build_all(build_ext):
    def initialize_options(self):
        build_ext.initialize_options(self)

    def build_extensions(self):
        global module

        setup_libuv.build_libuv(self)

        compiler_type = self.compiler.compiler_type
        print("Current compiler type:", compiler_type)
        try:
            compiler_cmd = self.compiler.compiler
            print('Current compiler: {}'.format(compiler_cmd))
        except:
            compiler_cmd = None
        
        compiler = ""
        if compiler_cmd:
            compiler = compiler_cmd[0]
        if compiler_type == "msvc":
            compiler = "msvc"
        
        compiler_ver_major = 0
        if compiler_type == 'unix' and compiler:
            version, c_ver_major, c_ver_minor = get_compiler_version(compiler)
            print('Current compiler version: {}.{}'.format(c_ver_major, c_ver_minor))
            if c_ver_major:
                compiler_ver_major = c_ver_major

        for ext in self.extensions:
            if ext == module:
                if compiler_type == 'msvc':
                    ext.extra_compile_args = [ '/Oi', '/Oy-', '/W3', '/WX-', '/Gd', '/GS' ]
                    ext.extra_compile_args += [ '/Zc:forScope', '/Zc:inline', '/fp:precise', '/analyze-' ]
                else:
                    ext.extra_compile_args = [ "-O3", "-fno-strict-aliasing", "-fcommon", "-g", "-Wall" ]
                    ext.extra_compile_args += [ "-Wno-unused-function", "-Wno-unused-variable" ]
                    if compiler.startswith("gcc") and compiler_ver_major >= 8:
                        ext.extra_compile_args += [ "-Wno-unused-but-set-variable" ]
                    if compiler.startswith("clang") and compiler_ver_major >= 13:
                        ext.extra_compile_args += [ "-Wno-unused-but-set-variable" ]
        
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
