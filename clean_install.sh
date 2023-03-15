sudo rm -rf /usr/local/lib/python3.8/dist-packages/_fastwsgi*;
sudo rm -rf /usr/local/lib/python3.8/dist-packages/fastwsgi*;
rm -rf build/ dist/ bin/ __pycache__/
sudo CC="gcc" python3 setup.py install
