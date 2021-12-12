fuser -k 5000/tcp;
rm -rf nohup.out
cd servers/
nohup uvicorn uvicorn_asgi:app &
sleep 3
wrk -t8 -c100 -d60 http://localhost:8000 --latency > ../results/uvicorn_asgi_results.txt
fuser -k 8000/tcp;