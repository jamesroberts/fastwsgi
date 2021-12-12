fuser -k 5000/tcp;
rm -rf nohup.out
nohup python3 ../servers/fastwsgi_wsgi.py &
sleep 3
wrk -t8 -c100 -d60 http://localhost:5000 --latency > results/fastwsgi_wsgi_results.txt
fuser -k 5000/tcp;