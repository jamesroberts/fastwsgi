import _fast_wsgi


def run(wsgi_app, host, port, backlog=256):
    try:
        print(f"Running server on {host} and port {port}")
        _fast_wsgi.run_server(wsgi_app, host, port, backlog)
    finally:
        print("Closing...")


print("Running...")
run({}, "0.0.0.0", 5000)
# TODO: Shutdown on Ctrl-C somehow
print("Done")
