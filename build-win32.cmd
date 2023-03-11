@echo off

:: set PATH=d:\python\python38-32;d:\python\python38-32\Scripts;%PATH%

rmdir /s /q build
rmdir /s /q dist

python setup.py sdist
python setup.py build_ext
python setup.py bdist_wheel
