fuser -k 5000/tcp;
rm -rf nohup.out
nohup python3 ../servers/bjoern_flask.py &
sleep 3
wrk -t8 -c100 -d60 http://localhost:5000 --latency > results/bjoern_flask_results.txt
fuser -k 5000/tcp;