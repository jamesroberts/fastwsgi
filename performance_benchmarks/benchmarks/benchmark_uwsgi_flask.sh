fuser -k 5000/tcp;
rm -rf nohup.out
cd servers/
nohup uwsgi --http 127.0.0.1:5000 --wsgi-file basic_flask.py --callable app &
sleep 3
wrk -t8 -c100 -d60 http://localhost:5000 --latency > ../results/uwsgi_flask_results.txt
fuser -k 5000/tcp;