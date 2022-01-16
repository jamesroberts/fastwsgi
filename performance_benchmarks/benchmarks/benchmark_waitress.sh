fuser -k 5000/tcp;
rm -rf nohup.out
nohup python3 ../servers/waitress_wsgi.py &
sleep 3
wrk -t8 -c100 -d60 http://localhost:5000 --latency > ../results/waitress_results.txt
fuser -k 5000/tcp;
