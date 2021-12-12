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

echo "Benchmarking WSGI + Gunicorn"
./benchmarks/benchmark_gunicorn_wsgi.sh

echo "Benchmarking WSGI + FastWSGI"
./benchmarks/benchmark_fastwsgi_wsgi.sh

echo "Benchmarking WSGI + Bjoern"
./benchmarks/benchmark_bjoern_wsgi.sh

echo "Benchmarking ASGI Uvicorn"
./benchmarks/benchmark_uvicorn_asgi.sh
