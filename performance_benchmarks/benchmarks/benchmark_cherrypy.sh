fuser -k 5000/tcp;
fuser -k 8080/tcp;
rm -rf nohup.out
nohup python3 ../servers/cherry_py.py &
sleep 3
wrk -t8 -c100 -d60 http://localhost:8080 --latency > results/cherrypy_results.txt
fuser -k 8080/tcp;