import matplotlib
import matplotlib.pyplot as plt
import numpy as np

benchmarks = {
    "CherryPy": "results/cherrypy_results.txt",
    "Flask": "results/basic_flask_results.txt",
    "Flask+\nGunicorn": "results/gunicorn_flask_results.txt",
    "Flask+\nBjoern": "results/bjoern_flask_results.txt",
    "Flask+\nFastWSGI": "results/fastwsgi_flask_results.txt",
}

requests_per_second = []
requests_served = []

for key, file in benchmarks.items():
    with open(file, "r") as f:
        for line in f:
            if "Requests/sec:" in line:
                datapoint = (key, int(float(line.split("Requests/sec:")[1].strip())))
                requests_per_second.append(datapoint)

            if "requests in" in line:
                datapoint = (key, int(line.split("requests in")[0].strip()))
                requests_served.append(datapoint)

print(requests_per_second)

print(requests_served)

import matplotlib.pyplot as plt

fig1 = plt.figure()
fig1.set_size_inches(8, 6)
ax1 = fig1.add_axes([0, 0, 1, 1])

labels = [dp[0] for dp in requests_per_second]
rps = [dp[1] for dp in requests_per_second]
ax1.set_ylabel("Requests per second")
ax1.set_title("Requests per second per WSGI server")
ax1.set_yticks([x for x in range(0, 10000, 1000)])
ax1.bar(labels, rps)
plt.savefig(f"requests_per_second.jpg", bbox_inches="tight", pad_inches=0.3, dpi=200)

fig2 = plt.figure()
fig2.set_size_inches(8, 6)
ax2 = fig2.add_axes([0, 0, 1, 1])

labels = [dp[0] for dp in requests_served]
rps = [dp[1] for dp in requests_served]
ax2.set_ylabel("Requests served")
ax2.set_title("Requests serverd in 60 seconds")
ax2.set_yticks([x for x in range(0, 1000000, 30000)])
ax2.bar(labels, rps)
plt.savefig(f"requests_served.jpg", bbox_inches="tight", pad_inches=0.3, dpi=200)