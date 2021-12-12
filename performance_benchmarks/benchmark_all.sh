pip install -r requirements.txt

echo "Benchmarking Flask"
./benchmark_basic_flask.sh

echo "Benchmarking Flask + Gunicorn"
./benchmark_gunicorn_flask.sh

echo "Benchmarking Flask + FastWSGI"
./benchmark_fastwsgi_flask.sh

echo "Benchmarking Flask + Bjoern"
./benchmark_bjoern_flask.sh

echo "Benchmarking CherryPy"
./benchmark_cherrypy.sh