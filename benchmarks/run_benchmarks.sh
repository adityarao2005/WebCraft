#!/usr/bin/env bash
# =============================================================================
# run_benchmarks.sh — Benchmark WebCraft vs Node.js vs Nginx
#
# Usage:
#   ./run_benchmarks.sh [-d duration] [-c connections] [-t threads] [-o outdir]
#
# Prerequisites:
#   - wrk          (HTTP benchmarking tool)
#   - node         (Node.js runtime)
#   - nginx        (Nginx web server)
#   - The WebCraft benchmark server binary (built via CMake)
#
# This script starts each server one at a time, hammers it with wrk using
# identical parameters, captures the results, then prints a comparison table.
# =============================================================================

set -euo pipefail

# ---- Colour helpers ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Colour

# ---- Defaults ----
DURATION="10s"
CONNECTIONS=100
THREADS=4
OUTDIR=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Ports for each server
WEBCRAFT_PORT=8080
NODE_PORT=8081
NGINX_PORT=8082

# Path to the WebCraft benchmark binary (built by CMake)
# Try common build locations
WEBCRAFT_BIN=""
for candidate in \
    "${SCRIPT_DIR}/../build/benchmarks/webcraft_bench_server" \
    "${SCRIPT_DIR}/../build/Release/benchmarks/webcraft_bench_server" \
    "${SCRIPT_DIR}/../cmake-build-release/benchmarks/webcraft_bench_server" \
    "${SCRIPT_DIR}/../build/benchmarks/Release/webcraft_bench_server"; do
    if [[ -x "$candidate" ]]; then
        WEBCRAFT_BIN="$candidate"
        break
    fi
done

# ---- Parse CLI arguments ----
usage() {
    echo "Usage: $0 [-d duration] [-c connections] [-t threads] [-o output_dir] [-b webcraft_binary]"
    echo ""
    echo "Options:"
    echo "  -d   wrk duration    (default: ${DURATION})"
    echo "  -c   connections     (default: ${CONNECTIONS})"
    echo "  -t   wrk threads     (default: ${THREADS})"
    echo "  -o   output dir      (default: benchmarks/results/<timestamp>)"
    echo "  -b   path to webcraft_bench_server binary"
    echo "  -h   show this help"
    exit 1
}

while getopts "d:c:t:o:b:h" opt; do
    case "$opt" in
        d) DURATION="$OPTARG" ;;
        c) CONNECTIONS="$OPTARG" ;;
        t) THREADS="$OPTARG" ;;
        o) OUTDIR="$OPTARG" ;;
        b) WEBCRAFT_BIN="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done

# ---- Output directory ----
if [[ -z "$OUTDIR" ]]; then
    OUTDIR="${SCRIPT_DIR}/results/$(date +%Y%m%d_%H%M%S)"
fi
mkdir -p "$OUTDIR"

# ---- Logging helpers ----
log()   { echo -e "${CYAN}[bench]${NC} $*"; }
ok()    { echo -e "${GREEN}  ✓${NC} $*"; }
warn()  { echo -e "${YELLOW}  ⚠${NC} $*"; }
fail()  { echo -e "${RED}  ✗${NC} $*"; }

# ---- Prerequisite checks ----
log "Checking prerequisites..."

MISSING=()
command -v wrk   >/dev/null 2>&1 || MISSING+=("wrk")
command -v node  >/dev/null 2>&1 || MISSING+=("node")
command -v nginx >/dev/null 2>&1 || MISSING+=("nginx")

if [[ -z "$WEBCRAFT_BIN" || ! -x "$WEBCRAFT_BIN" ]]; then
    MISSING+=("webcraft_bench_server (build the project with -DWEBCRAFT_BUILD_BENCHMARKS=ON)")
fi

if [[ ${#MISSING[@]} -gt 0 ]]; then
    fail "Missing prerequisites:"
    for m in "${MISSING[@]}"; do
        echo "       - $m"
    done
    echo ""
    echo "Install missing tools and rebuild, then re-run this script."
    echo "  Ubuntu/Debian:  sudo apt install wrk nginx nodejs"
    echo "  macOS (brew):   brew install wrk nginx node"
    exit 1
fi

ok "All prerequisites found"

# ---- Helper: wait until a port is accepting connections ----
wait_for_port() {
    local port=$1
    local timeout=${2:-10}
    local elapsed=0
    while ! (echo > /dev/tcp/127.0.0.1/"$port") 2>/dev/null; do
        sleep 0.1
        elapsed=$(echo "$elapsed + 0.1" | bc)
        if (( $(echo "$elapsed >= $timeout" | bc -l) )); then
            fail "Timed out waiting for port $port"
            return 1
        fi
    done
}

# ---- Helper: kill a background process safely ----
kill_server() {
    local pid=$1
    if kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    fi
}

# ---- Helper: extract key metrics from wrk output ----
extract_metrics() {
    local file=$1
    local rps lat transfer

    rps=$(grep "Requests/sec:" "$file" | awk '{print $2}')
    lat=$(grep "Latency" "$file" | head -1 | awk '{print $2}')
    transfer=$(grep "Transfer/sec:" "$file" | awk '{print $2}')

    echo "$rps|$lat|$transfer"
}

# ---- Print benchmark parameters ----
echo ""
log "${BOLD}Benchmark Configuration${NC}"
echo "  Duration:    ${DURATION}"
echo "  Connections: ${CONNECTIONS}"
echo "  Threads:     ${THREADS}"
echo "  Results:     ${OUTDIR}"
echo ""

# =====================================================================
# 1. Benchmark WebCraft
# =====================================================================
log "${BOLD}[1/3] Benchmarking WebCraft${NC} (port ${WEBCRAFT_PORT})"

"$WEBCRAFT_BIN" "$WEBCRAFT_PORT" &
WC_PID=$!
wait_for_port "$WEBCRAFT_PORT"
ok "WebCraft server is up (PID ${WC_PID})"

wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" \
    "http://127.0.0.1:${WEBCRAFT_PORT}/" \
    2>&1 | tee "${OUTDIR}/webcraft.txt"

kill_server "$WC_PID"
ok "WebCraft server stopped"
echo ""

# =====================================================================
# 2. Benchmark Node.js
# =====================================================================
log "${BOLD}[2/3] Benchmarking Node.js${NC} (port ${NODE_PORT})"

node "${SCRIPT_DIR}/node_server.js" "$NODE_PORT" &
NODE_PID=$!
wait_for_port "$NODE_PORT"
ok "Node.js server is up (PID ${NODE_PID})"

wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" \
    "http://127.0.0.1:${NODE_PORT}/" \
    2>&1 | tee "${OUTDIR}/nodejs.txt"

kill_server "$NODE_PID"
ok "Node.js server stopped"
echo ""

# =====================================================================
# 3. Benchmark Nginx
# =====================================================================
log "${BOLD}[3/3] Benchmarking Nginx${NC} (port ${NGINX_PORT})"

# Nginx needs an absolute path for its config
NGINX_CONF="$(cd "${SCRIPT_DIR}" && pwd)/nginx.conf"

nginx -c "$NGINX_CONF" &
NGINX_PID=$!
wait_for_port "$NGINX_PORT"
ok "Nginx server is up (PID ${NGINX_PID})"

wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" \
    "http://127.0.0.1:${NGINX_PORT}/" \
    2>&1 | tee "${OUTDIR}/nginx.txt"

# Nginx may need SIGQUIT for graceful shutdown
kill -QUIT "$NGINX_PID" 2>/dev/null || true
wait "$NGINX_PID" 2>/dev/null || true
ok "Nginx server stopped"
echo ""

# =====================================================================
# Summary Table
# =====================================================================
log "${BOLD}Results Summary${NC}"
echo ""

WC_METRICS=$(extract_metrics "${OUTDIR}/webcraft.txt")
NODE_METRICS=$(extract_metrics "${OUTDIR}/nodejs.txt")
NGINX_METRICS=$(extract_metrics "${OUTDIR}/nginx.txt")

WC_RPS=$(echo "$WC_METRICS" | cut -d'|' -f1)
WC_LAT=$(echo "$WC_METRICS" | cut -d'|' -f2)
WC_XFER=$(echo "$WC_METRICS" | cut -d'|' -f3)

NODE_RPS=$(echo "$NODE_METRICS" | cut -d'|' -f1)
NODE_LAT=$(echo "$NODE_METRICS" | cut -d'|' -f2)
NODE_XFER=$(echo "$NODE_METRICS" | cut -d'|' -f3)

NGINX_RPS=$(echo "$NGINX_METRICS" | cut -d'|' -f1)
NGINX_LAT=$(echo "$NGINX_METRICS" | cut -d'|' -f2)
NGINX_XFER=$(echo "$NGINX_METRICS" | cut -d'|' -f3)

printf "  ${BOLD}%-15s %15s %15s %15s${NC}\n" "Server" "Req/sec" "Avg Latency" "Transfer/sec"
printf "  %-15s %15s %15s %15s\n"             "───────────────" "───────────────" "───────────────" "───────────────"
printf "  %-15s %15s %15s %15s\n"             "WebCraft"  "${WC_RPS:-N/A}"   "${WC_LAT:-N/A}"   "${WC_XFER:-N/A}"
printf "  %-15s %15s %15s %15s\n"             "Node.js"   "${NODE_RPS:-N/A}" "${NODE_LAT:-N/A}" "${NODE_XFER:-N/A}"
printf "  %-15s %15s %15s %15s\n"             "Nginx"     "${NGINX_RPS:-N/A}" "${NGINX_LAT:-N/A}" "${NGINX_XFER:-N/A}"

echo ""
log "Full wrk output saved to: ${OUTDIR}/"
log "Done!"
