fuser -k 5000/tcp;
rm -rf nohup.out
nohup python3 ../servers/basic_flask.py &
sleep 3
wrk -t8 -c100 -d60 http://localhost:5000 --latency > ../results/basic_flask_results.txt
fuser -k 5000/tcp;