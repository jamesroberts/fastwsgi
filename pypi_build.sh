sudo rm -rf dist/
sudo python3 setup.py sdist
sudo python3 setup.py build_ext
sudo pip3 install dist/fast*
