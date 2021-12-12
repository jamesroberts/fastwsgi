pip install -r requirements.txt

echo "Benchmarking Flask"
./benchmarks/benchmark_basic_flask.sh

echo "Benchmarking Flask + Gunicorn"
./benchmarks/benchmark_gunicorn_flask.sh

echo "Benchmarking Flask + FastWSGI"
./benchmarks/benchmark_fastwsgi_flask.sh

echo "Benchmarking Flask + Bjoern"
./benchmarks/benchmark_bjoern_flask.sh

echo "Benchmarking CherryPy"
./benchmarks/benchmark_cherrypy.sh