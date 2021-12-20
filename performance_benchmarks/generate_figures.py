import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter


def y_fmt(x, y):
    return "{:,.0f}".format(x)


def requests_per_second_graph(save_name, data):
    fig = plt.figure()
    fig.set_size_inches(6, 4)
    ax = fig.add_axes([0, 0, 1, 1])

    labels = [dp[0] for dp in data]
    rps = [dp[1] for dp in data]
    ax.set_ylabel("Requests per second")
    ax.set_title("Requests per second per WSGI server")
    ax.yaxis.set_major_formatter(FuncFormatter(y_fmt))
    ax.bar(labels, rps)
    plt.savefig(f"graphs/{save_name}", bbox_inches="tight", pad_inches=0.3, dpi=200)


def requests_served_graph(save_name, data):
    fig = plt.figure()
    fig.set_size_inches(6, 4)
    ax = fig.add_axes([0, 0, 1, 1])

    labels = [dp[0] for dp in data]
    rs = [dp[1] for dp in data]
    ax.set_ylabel("Requests served")
    ax.set_title("Requests served in 60 seconds")
    ax.yaxis.set_major_formatter(FuncFormatter(y_fmt))
    ax.bar(labels, rs)
    plt.savefig(f"graphs/{save_name}", bbox_inches="tight", pad_inches=0.3, dpi=200)


def extract_data(req_ps, req_served, benchmarks):
    for key, file in benchmarks.items():
        with open(file, "r") as f:
            for line in f:
                if "Requests/sec:" in line:
                    data = int(float(line.split("Requests/sec:")[1].strip()))
                    req_ps.append((key, data))

                if "requests in" in line:
                    datapoint = (key, int(line.split("requests in")[0].strip()))
                    req_served.append(datapoint)


flask_benchmarks = {
    "CherryPy": "results/cherrypy_results.txt",
    "Flask": "results/basic_flask_results.txt",
    "Flask+\nGunicorn": "results/gunicorn_flask_results.txt",
    "Flask+\nuWSGI": "results/uwsgi_flask_results.txt",
    "Flask+\nBjoern": "results/bjoern_flask_results.txt",
    "Flask+\nFastWSGI": "results/fastwsgi_flask_results.txt",
}

wsgi_benchmarks = {
    "Cheroot": "results/cheroot_wsgi_results.txt",
    "Gunicorn": "results/gunicorn_wsgi_results.txt",
    "Uvicorn": "results/uvicorn_asgi_results.txt",
    "uWSGI": "results/basic_uwsgi_results.txt",
    "Bjoern": "results/bjoern_wsgi_results.txt",
    "FastWSGI": "results/fastwsgi_wsgi_results.txt",
}


flask_requests_per_second, flask_requests_served = [], []
extract_data(flask_requests_per_second, flask_requests_served, flask_benchmarks)
requests_per_second_graph("flask_requests_per_second.jpg", flask_requests_per_second)
requests_served_graph("flask_requests_served.jpg", flask_requests_served)

wsgi_requests_per_second, wsgi_requests_served = [], []
extract_data(wsgi_requests_per_second, wsgi_requests_served, wsgi_benchmarks)
requests_per_second_graph("wsgi_requests_per_second.jpg", wsgi_requests_per_second)
requests_served_graph("wsgi_requests_served.jpg", wsgi_requests_served)