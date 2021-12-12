fuser -k 5000/tcp;
rm -rf nohup.out
cd servers/
nohup gunicorn gunicorn_flask:app --bind 127.0.0.1:5000 &
sleep 3
wrk -t8 -c100 -d60 http://localhost:5000 --latency > ../results/gunicorn_flask_results.txt
fuser -k 5000/tcp;