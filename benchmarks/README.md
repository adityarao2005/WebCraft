# WebCraft benchmarks

This suite compares tiny HTTP/1.1 keep-alive servers built with WebCraft, Node.js,
and nginx. Each returns the same fixed HTML body. It measures this workload,
not general runtime performance or production HTTP-server capability. WebCraft's
example responds to each socket read and is not a complete HTTP parser; use the
runner's GET workload without request bodies or HTTP pipelining.

## Build and prerequisites

Install Python 3.9+, `wrk`, Node.js, nginx, and WebCraft's build dependencies.
On Ubuntu/Debian the load/server tools are available through
`sudo apt install wrk nginx nodejs`; on macOS use `brew install wrk nginx node`.
The Bash/Python runner supports Linux and macOS. Native Windows orchestration is
not provided; WSL results describe the WSL environment.

Build the entire library and benchmark in Release, following the project
[setup guide](../docs/getting-started.md):

```sh
python scripts/webcraft.py configure --build-dir build-bench --config Release --define WEBCRAFT_BUILD_BENCHMARKS=ON
python scripts/webcraft.py build --build-dir build-bench --config Release
```

Optimizing only the benchmark executable does not make a Debug runtime suitable
for comparison. The runner rejects a known non-Release single-config CMake build.
For external binaries or multi-config builds, confirm Release yourself; the
runner reports the build type as unverified. It records binary hashes and current
checkout metadata, but cannot prove that an external binary matches the checkout.

## Run

```sh
./benchmarks/run_benchmarks.sh -b build-bench/benchmarks/webcraft_bench_server
```

Defaults: five repetitions per server, two-second warmups, ten-second measured
runs, 100 connections, and four wrk threads. Each sample starts a fresh server.
Server order rotates between repetitions to reduce fixed-order bias. Allow about
three minutes for the defaults, plus startup and shutdown time.

```sh
./benchmarks/run_benchmarks.sh \
  -b build-bench/benchmarks/webcraft_bench_server \
  -d 30s -w 5s -r 6 -c 100 -t 4 \
  --environment-note 'AC power; balanced mode; background apps closed' \
  -o /tmp/webcraft-comparison
```

Use `-c 1 -t 1` in a separate run to assess low-concurrency behavior. Results from
different connection counts, thread counts, builds, or CPU placement are separate
experiments. `--base-port` changes the three consecutive loopback ports (default
8080–8082). An occupied port, unexpected response body, failed process, wrk socket
error, or unsuccessful HTTP response fails the run; it does not enter the summary.
Output directories must be empty so previous measurements cannot be overwritten.

### CPU placement and host conditions

No affinity is imposed by default. On Linux, optionally assign the same allowed
CPU budget to every server and a separate budget to the load generator:

```sh
lscpu -e=CPU,CORE,SOCKET
./benchmarks/run_benchmarks.sh \
  -b build-bench/benchmarks/webcraft_bench_server \
  --server-cpus 0,2 --client-cpus 4,6,8,10
```

Choose CPUs from your own topology; those IDs are illustrative. Avoid sharing
reported cores between server and client, including SMT siblings. The runner
rejects overlapping logical CPU sets and records the topology, but CPU affinity
does not reserve cores or eliminate background activity. WSL's virtual topology
also does not guarantee physical placement on the Windows host.

The server CPU mask applies to the entire process tree, not individual runtime
threads. WebCraft has a runtime thread and a calling thread that currently spins
in `sync_wait`. Node has an event-loop thread plus auxiliary threads; nginx uses
one HTTP worker plus its master. These are not identical thread/process counts.
An extra core consumed by a waiting thread must count toward server resource use.

Use AC power and a consistent power mode. Reduce background load, avoid concurrent
builds, and let the machine settle. Record those conditions with
`--environment-note`, especially on laptops or WSL where Windows activity and
battery-saving policies can affect results. The runner cannot capture all host
activity or enforce a stable CPU frequency. Short runs validate the harness;
they are not evidence of comparative performance.

## Results and interpretation

Each output directory contains:

- `environment.json`: OS/kernel, reported CPU topology and allowed mask, CLI
  options/environment note, tool/compiler versions, available CMake settings,
  binary SHA256, and checkout revision/status.
- Copies of the benchmark inputs and the generated nginx configuration, with a
  private PID path for that run.
- Per-sample warmup/measurement wrk output and server logs.
- `runs.json`: throughput, mean/p50/p90/p99 latency in microseconds, commands, and
  Linux server process-tree CPU use for each successful measured sample. Load
  averages and Linux guest memory availability are recorded around measurements;
  these do not describe all Windows host resources.
- `summary.json`: median metrics and throughput minimum/maximum across repetitions.
  Percentiles are medians of per-run percentiles, not pooled request percentiles.
- `failure.txt` when orchestration or measurement fails. Partial raw results remain
  available; no complete summary is produced.

The console prints median throughput, mean/p99 latency, throughput range, and CPU
use. Linux CPU measurement includes all server threads and nginx's worker; 100%
means one logical CPU. CPU use is unavailable on macOS and shown as `N/A`. Load
process and total host CPU use are not included. Hardware interrupt/kernel worker
cost outside the server process tree is also not captured.

Compare throughput, tail latency, variability, and resource use together. A median
increase smaller than run-to-run variation is not a demonstrated improvement.
Repeat under stable conditions and, when possible, native Linux before drawing
conclusions from WSL. Old result directories used a different harness and should
not be treated as a controlled before/after comparison with new runs.

The runner checks HTTP 200 and body bytes before each sample. HTTP headers and
connection lifecycle policies still differ between the servers. The benchmark
uses default server signal termination after measurements; it is not a graceful
runtime-shutdown or cancellation test.

## Runner checks

```sh
python -m unittest discover -s benchmarks/tests -v
bash -n benchmarks/run_benchmarks.sh
```

The existing result archives remain under [results](results/). The runner is
[run_benchmarks.py](run_benchmarks.py), launched by
[run_benchmarks.sh](run_benchmarks.sh). Server inputs are
[webcraft_server.cpp](webcraft_server.cpp), [node_server.js](node_server.js), and
[nginx.conf](nginx.conf).
