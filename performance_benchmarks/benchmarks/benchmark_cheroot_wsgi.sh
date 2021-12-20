fuser -k 5000/tcp;
fuser -k 8080/tcp;
rm -rf nohup.out
nohup python3 ../servers/cheroot_wsgi.py &
sleep 3
wrk -t8 -c100 -d60 http://localhost:8080 --latency > results/cheroot_wsgi_results.txt
fuser -k 8080/tcp;