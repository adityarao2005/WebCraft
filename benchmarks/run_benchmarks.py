#!/usr/bin/env python3
"""Repeated loopback HTTP comparison. Results describe the recorded environment."""

import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import time
from datetime import datetime, timezone

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BODY = ("<!DOCTYPE html>\n<html><head><title>WebCraft Benchmark</title></head>\n"
        "<body><h1>Hello, World!</h1></body></html>\n").encode()


def cpus(value):
    try:
        result = set()
        for part in value.split(','):
            bounds = [int(x) for x in part.split('-')]
            if len(bounds) == 1:
                result.add(bounds[0])
            elif len(bounds) == 2 and bounds[0] <= bounds[1]:
                result.update(range(bounds[0], bounds[1] + 1))
            else:
                raise ValueError()
        if not result or min(result) < 0:
            raise ValueError()
        return sorted(result)
    except ValueError:
        raise argparse.ArgumentTypeError('expected CPUs such as 0,2 or 4-7')


def seconds(value):
    if not re.fullmatch(r'[1-9][0-9]*[smh]', value):
        raise argparse.ArgumentTypeError('expected a positive wrk duration such as 10s or 1m')
    return int(value[:-1]) * {'s': 1, 'm': 60, 'h': 3600}[value[-1]]


def parse_wrk(text):
    if re.search(r'Socket errors:|Non-2xx or 3xx responses:', text):
        raise ValueError('wrk reported socket errors or unsuccessful responses')

    def number(pattern):
        match = re.search(pattern, text, re.M)
        if not match:
            raise ValueError('missing wrk metric: ' + pattern)
        return float(match[1])

    def latency(pattern):
        match = re.search(pattern + r'([\d.]+)(us|ms|s)', text, re.M)
        if not match:
            raise ValueError('missing wrk latency: ' + pattern)
        return float(match[1]) * {'us': 1, 'ms': 1000, 's': 1000000}[match[2]]

    result = {'rps': number(r'^Requests/sec:\s+([\d.]+)'),
              'mean_latency_us': latency(r'^\s+Latency\s+')}
    if result['rps'] <= 0:
        raise ValueError('wrk completed no requests')
    for percentile in (50, 90, 99):
        result[f'p{percentile}_us'] = latency(r'^\s+' + str(percentile) + r'%\s+')
    return result


def capture(command):
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        return (result.stdout + result.stderr).strip()
    except (OSError, subprocess.TimeoutExpired) as error:
        return str(error)


def resources():
    result = {'load_average': list(os.getloadavg())}
    memory = Path('/proc/meminfo')
    if sys.platform.startswith('linux') and memory.is_file():
        result['guest_memory_kib'] = {
            match[1]: int(match[2]) for match in re.finditer(
                r'^(MemTotal|MemAvailable|SwapTotal|SwapFree):\s+(\d+) kB$',
                memory.read_text(), re.M)}
    return result


def cpu_snapshot(pid):
    """Linux process tree CPU seconds, including threads and nginx's worker."""
    if not sys.platform.startswith('linux'):
        return None
    result = {}

    def visit(current):
        try:
            fields = Path(f'/proc/{current}/stat').read_text().rsplit(')', 1)[1].split()
            result[f'{current}:{fields[19]}'] = (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')
            children = Path(f'/proc/{current}/task/{current}/children').read_text().split()
            for child in children:
                visit(int(child))
        except FileNotFoundError:
            pass

    visit(pid)
    return result


def stop_server(process):
    def send(signum):
        try:
            os.killpg(process.pid, signum)
        except ProcessLookupError:
            pass

    if process.poll() is None:
        send(signal.SIGTERM)
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        send(signal.SIGKILL)
        process.wait(timeout=3)
    # Also remove surviving children if their parent exited first.
    send(signal.SIGKILL)


def check_response(port):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=1)
    try:
        connection.request('GET', '/', headers={'Connection': 'close'})
        response = connection.getresponse()
        if response.status != 200 or response.read() != BODY:
            raise ValueError('server did not return the expected HTTP 200 body')
    finally:
        connection.close()


def run_sample(name, command, port, args, out, repeat):
    prefix = out / f'{repeat + 1:02d}-{name}'
    # Refuse to benchmark a pre-existing service on the requested port.
    with socket.socket() as probe:
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        probe.bind(('127.0.0.1', port))
    if args.server_cpus:
        command = ['taskset', '-c', ','.join(map(str, args.server_cpus))] + command
    with prefix.with_suffix('.server.log').open('w') as log:
        process = subprocess.Popen(command, stdout=log, stderr=log, start_new_session=True)
        try:
            deadline = time.monotonic() + 10
            while True:
                if process.poll() is not None:
                    raise RuntimeError(f'{name} exited during startup; see {prefix}.server.log')
                try:
                    check_response(port)
                    break
                except (OSError, http.client.HTTPException):
                    if time.monotonic() >= deadline:
                        raise RuntimeError(f'{name} startup timed out')
                    time.sleep(.05)
            base = ['wrk', f'-t{args.threads}', f'-c{args.connections}', '--latency']
            if args.client_cpus:
                base = ['taskset', '-c', ','.join(map(str, args.client_cpus))] + base
            url = f'http://127.0.0.1:{port}/'
            for phase, duration in [('warmup', args.warmup), ('measurement', args.duration)]:
                load_before = resources()
                before = cpu_snapshot(process.pid)
                start = time.monotonic()
                result = subprocess.run(base + [f'-d{duration}s', url], capture_output=True,
                                        text=True, timeout=duration + 15)
                wall = time.monotonic() - start
                after = cpu_snapshot(process.pid)
                raw = result.stdout + result.stderr
                prefix.with_suffix(f'.{phase}.txt').write_text(raw)
                result.check_returncode()
                metrics = parse_wrk(raw)
                if process.poll() is not None:
                    raise RuntimeError(f'{name} exited during {phase}')
            metrics.update(server=name, repeat=repeat + 1, wall_seconds=wall,
                           resources_before=load_before, resources_after=resources(),
                           command=command, wrk_command=base + [f'-d{args.duration}s', url])
            metrics['server_cpu_percent'] = None
            if before is not None and after is not None:
                if set(before) != set(after):
                    raise RuntimeError(f'{name} process tree changed during measurement')
                metrics['server_cpu_percent'] = 100 * sum(after[k] - before[k] for k in before) / wall
            return metrics
        finally:
            stop_server(process)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-d', '--duration', type=seconds, default=10)
    parser.add_argument('-w', '--warmup', type=seconds, default=2)
    parser.add_argument('-r', '--repeats', type=int, default=5)
    parser.add_argument('-c', '--connections', type=int, default=100)
    parser.add_argument('-t', '--threads', type=int, default=4)
    parser.add_argument('-o', '--output', type=Path)
    parser.add_argument('-b', '--binary', type=Path)
    parser.add_argument('--server-cpus', type=cpus)
    parser.add_argument('--client-cpus', type=cpus)
    parser.add_argument('--environment-note', default='', help='power mode, battery/AC, background load, etc.')
    parser.add_argument('--base-port', type=int, default=8080)
    args = parser.parse_args()
    if args.repeats < 1 or args.threads < 1 or args.connections < args.threads:
        parser.error('require repeats >= 1 and connections >= threads >= 1')
    if not 1024 <= args.base_port <= 65533:
        parser.error('base port must be between 1024 and 65533')
    for tool in ('wrk', 'node', 'nginx'):
        if not shutil.which(tool):
            parser.error(f'missing prerequisite: {tool}')
    if args.server_cpus or args.client_cpus:
        if not hasattr(os, 'sched_getaffinity') or not shutil.which('taskset'):
            parser.error('CPU placement requires Linux sched_getaffinity and taskset')
        allowed = os.sched_getaffinity(0)
        if not set((args.server_cpus or []) + (args.client_cpus or [])).issubset(allowed):
            parser.error('requested CPUs are outside the allowed affinity mask')
        if set(args.server_cpus or []) & set(args.client_cpus or []):
            parser.error('server and client CPU sets must not overlap')
    candidates = [ROOT / p for p in ('build/benchmarks/webcraft_bench_server',
                  'build/benchmarks/Release/webcraft_bench_server',
                  'cmake-build-release/benchmarks/webcraft_bench_server')]
    binary = args.binary or next((p for p in candidates if p.is_file()), None)
    if binary is None or not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error('specify the executable with -b PATH')
    binary = binary.resolve()
    out = (args.output or HERE / 'results' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        parser.error('output directory must be empty; old results will not be overwritten')
    cache_path = next((p / 'CMakeCache.txt' for p in binary.parents if (p / 'CMakeCache.txt').is_file()), None)
    cache = cache_path.read_text() if cache_path else ''
    keys = ('CMAKE_BUILD_TYPE', 'CMAKE_CXX_COMPILER', 'CMAKE_CXX_FLAGS', 'CMAKE_CXX_FLAGS_RELEASE',
            'CMAKE_GENERATOR', 'CMAKE_CONFIGURATION_TYPES')
    settings = {m[1]: m[2] for m in re.finditer(r'^([^/#:]+):[^=]+=(.*)$', cache, re.M) if m[1] in keys}
    if settings.get('CMAKE_BUILD_TYPE') not in (None, '', 'Release'):
        parser.error('WebCraft binary must come from a Release build')
    print('Results:', out, flush=True)
    metadata = {'started_utc': datetime.now(timezone.utc).isoformat(),
                'platform': platform.platform(), 'uname': list(platform.uname()),
                'cpu_count': os.cpu_count(), 'cpu_topology': capture(['lscpu', '-e=CPU,CORE,SOCKET']),
                'allowed_cpus': sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
                'options': {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
                'binary': str(binary), 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'git_revision': capture(['git', '-C', str(ROOT), 'rev-parse', 'HEAD']),
                'git_status': capture(['git', '-C', str(ROOT), 'status', '--short']),
                'cmake_cache': str(cache_path) if cache_path else None, 'build_settings': settings,
                'compiler_version': capture([settings.get('CMAKE_CXX_COMPILER') or 'c++', '--version']),
                'versions': {tool: capture([tool, flag]) for tool, flag in [('node', '--version'), ('nginx', '-V'), ('wrk', '--version')]}}
    (out / 'environment.json').write_text(json.dumps(metadata, indent=2) + '\n')
    if not settings.get('CMAKE_BUILD_TYPE'):
        print('Build type unverified (external binary or multi-config build); confirm Release manually.', flush=True)
    # Keep exact copies of benchmark inputs with the results.
    for filename in ('node_server.js', 'nginx.conf', 'webcraft_server.cpp', 'run_benchmarks.py'):
        shutil.copyfile(HERE / filename, out / filename)
    config = (HERE / 'nginx.conf').read_text().replace('/tmp/nginx_bench.pid', str(out / 'nginx.pid'))
    config = config.replace('listen 8082 reuseport;', f'listen {args.base_port + 2};')
    pid_path = str(out / 'nginx.pid').replace('\\', '\\\\').replace('"', '\\"')
    config = config.replace(f'pid {out / "nginx.pid"};', f'pid "{pid_path}";')
    (out / 'nginx-run.conf').write_text(config)
    commands = {'webcraft': [str(binary), str(args.base_port)],
                'nodejs': ['node', str(out / 'node_server.js'), str(args.base_port + 1)],
                'nginx': ['nginx', '-p', str(out) + '/', '-e', str(out / 'nginx-startup.log'),
                          '-c', str(out / 'nginx-run.conf')]}
    rows = []
    try:
        for repeat in range(args.repeats):
            names = list(commands)
            rotation = repeat % len(names)
            names = names[rotation:] + names[:rotation]
            for name in names:
                port = args.base_port + list(commands).index(name)
                print(f'[{repeat + 1}/{args.repeats}] {name}', flush=True)
                rows.append(run_sample(name, commands[name], port, args, out, repeat))
                (out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        summary = {}
        print('\nServer       Median RPS       RPS min–max        Mean us    p99 us    CPU %')
        for name in commands:
            samples = [row for row in rows if row['server'] == name]
            summary[name] = {key: statistics.median(row[key] for row in samples)
                             for key in ('rps', 'mean_latency_us', 'p50_us', 'p90_us', 'p99_us')}
            summary[name]['rps_min'] = min(row['rps'] for row in samples)
            summary[name]['rps_max'] = max(row['rps'] for row in samples)
            cpu = [row['server_cpu_percent'] for row in samples if row['server_cpu_percent'] is not None]
            summary[name]['server_cpu_percent'] = statistics.median(cpu) if cpu else None
            s = summary[name]
            cpu_label = f'{statistics.median(cpu):.1f}' if cpu else 'N/A'
            print(f'{name:10} {s["rps"]:12.1f} {s["rps_min"]:9.1f}–{s["rps_max"]:<9.1f}'
                  f' {s["mean_latency_us"]:9.1f} {s["p99_us"]:9.1f} {cpu_label:>8}')
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    except BaseException as error:
        (out / 'failure.txt').write_text(f'{type(error).__name__}: {error}\n')
        raise


def interrupt(_signum, _frame):
    raise KeyboardInterrupt()


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, interrupt)
    try:
        main()
    except KeyboardInterrupt:
        sys.exit('Benchmark interrupted; active server cleanup completed.')
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        sys.exit(str(error))
