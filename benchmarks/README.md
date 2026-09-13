# WebCraft Benchmarks

Benchmark suite comparing **WebCraft** async TCP performance against **Node.js** and **Nginx** using identical "Hello, World!" HTTP servers.

## What's Being Measured

All three servers return the same tiny HTML response. The benchmark tool (`wrk`) measures:

| Metric | Description |
|---|---|
| **Requests/sec** | Throughput — how many HTTP requests the server completes per second |
| **Avg Latency** | Average time from sending a request to receiving a full response |
| **Transfer/sec** | Data throughput in MB/s |

All servers use **HTTP/1.1 keep-alive** so `wrk` can reuse connections and measure raw request throughput rather than connection setup overhead.

## Prerequisites

| Tool | Install (Ubuntu/Debian) | Install (macOS) |
|---|---|---|
| `wrk` | `sudo apt install wrk` | `brew install wrk` |
| `node` | `sudo apt install nodejs` | `brew install node` |
| `nginx` | `sudo apt install nginx` | `brew install nginx` |
| `cmake` + C++23 compiler | (already required for WebCraft) | |

## Building

Build WebCraft with benchmarks enabled:

```bash
cmake -S . -B build \
    -DWEBCRAFT_BUILD_BENCHMARKS=ON \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release
```

> **Important:** Always build with `Release` mode. Benchmarking a `Debug` build is meaningless.

## Running

### Quick Run (defaults: 10s, 100 connections, 4 threads)

```bash
chmod +x benchmarks/run_benchmarks.sh
./benchmarks/run_benchmarks.sh
```

### Custom Parameters

```bash
./benchmarks/run_benchmarks.sh \
    -d 30s \       # duration
    -c 256 \       # concurrent connections
    -t 8   \       # wrk threads
    -o results/    # output directory
```

### Running Servers Individually

You can also run each server manually for debugging:

```bash
# WebCraft (port 8080)
./build/benchmarks/webcraft_bench_server 8080

# Node.js (port 8081)
node benchmarks/node_server.js 8081

# Nginx (port 8082)
nginx -c "$(pwd)/benchmarks/nginx.conf"
```

Then benchmark with:

```bash
wrk -t4 -c100 -d10s http://127.0.0.1:8080/
```

## Understanding Results

The script prints a summary table:

```
  Server           Req/sec     Avg Latency    Transfer/sec
  ───────────────  ───────────────  ───────────────  ───────────────
  WebCraft              45000.00         2.21ms        6.50MB
  Node.js               32000.00         3.12ms        4.80MB
  Nginx                 55000.00         1.80ms        8.00MB
```

*(Numbers above are illustrative — your results will vary based on hardware.)*

Full `wrk` output (including latency percentiles) is saved to `benchmarks/results/<timestamp>/`.

### What to Expect

- **Nginx** is a mature, heavily optimized C server — it's the gold standard. Being within 2× of Nginx is excellent.
- **Node.js** uses libuv's event loop. It's a good baseline for single-threaded async runtimes.
- **WebCraft** uses `io_uring` on Linux for async I/O — it should be competitive, especially for I/O-bound workloads.

## Files

| File | Purpose |
|---|---|
| `webcraft_server.cpp` | WebCraft HTTP benchmark server (C++23, io_uring) |
| `node_server.js` | Node.js HTTP benchmark server (built-in http module) |
| `nginx.conf` | Nginx minimal benchmark config |
| `hello.html` | Static HTML file (alternative Nginx content) |
| `run_benchmarks.sh` | Orchestration script — runs all benchmarks |
| `CMakeLists.txt` | Build config for the WebCraft benchmark server |
| `results/` | Auto-created directory for benchmark output |

## Tuning for Fair Comparison

All servers default to **single-process** mode for a fair comparison:

- **WebCraft**: single-threaded with io_uring event loop
- **Node.js**: single-threaded with libuv event loop
- **Nginx**: `worker_processes 1` in the config

To test with multiple workers, edit `nginx.conf` and set `worker_processes auto;`, and use Node.js cluster mode.
