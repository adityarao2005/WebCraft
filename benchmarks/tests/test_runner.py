import argparse
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location('runner', Path(__file__).parents[1] / 'run_benchmarks.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)

WRK = '''Running 1s test @ http://127.0.0.1:8080/
  1 threads and 1 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   250.50us    1.00ms   4.00ms   90.00%
    Req/Sec     1.00k     0.00     1.00k   100.00%
  Latency Distribution
     50%  100.00us
     75%  200.00us
     90%  1.50ms
     99%  0.01s
  1000 requests in 1.00s, 100.00KB read
Requests/sec:   1000.00
Transfer/sec:    100.00KB
'''


class RunnerTests(unittest.TestCase):
    def test_latency_units_and_percentiles(self):
        self.assertEqual(runner.parse_wrk(WRK), {
            'rps': 1000.0, 'mean_latency_us': 250.5,
            'p50_us': 100.0, 'p90_us': 1500.0, 'p99_us': 10000.0})

    def test_invalid_samples_cannot_enter_summary(self):
        for text in [WRK + '\nSocket errors: connect 0, read 1, write 0, timeout 0',
                     WRK + '\nNon-2xx or 3xx responses: 12',
                     WRK.replace('Requests/sec:   1000.00', 'Requests/sec:   0.00'),
                     WRK.replace('     99%  0.01s', ''), 'unexpected output']:
            with self.subTest(text=text), self.assertRaises(ValueError):
                runner.parse_wrk(text)

    def test_cpu_ranges(self):
        self.assertEqual(runner.cpus('0,2,4-6,2'), [0, 2, 4, 5, 6])
        for value in ['', '-1', '4-2', '1-2-3', 'a']:
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                runner.cpus(value)

    def test_duration_validation(self):
        self.assertEqual(runner.seconds('2m'), 120)
        for value in ['0s', '-1s', '10', '1.5s']:
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                runner.seconds(value)

    def test_shutdown_escalates_without_waiting_forever(self):
        process = Mock(pid=1234)
        process.poll.return_value = None
        process.wait.side_effect = [runner.subprocess.TimeoutExpired('server', 3), 0]
        with patch.object(runner.os, 'killpg') as kill:
            runner.stop_server(process)
        self.assertEqual(kill.call_args_list[0].args, (1234, runner.signal.SIGTERM))
        self.assertTrue(any(c.args == (1234, runner.signal.SIGKILL) for c in kill.call_args_list))
        self.assertEqual(process.wait.call_count, 2)

    def test_bad_response_still_stops_server(self):
        process = Mock(pid=1234)
        process.poll.return_value = None
        args = argparse.Namespace(server_cpus=None)
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(runner.socket, 'socket'), \
                 patch.object(runner.subprocess, 'Popen', return_value=process), \
                 patch.object(runner, 'check_response', side_effect=ValueError('wrong body')), \
                 patch.object(runner, 'stop_server') as stop:
                with self.assertRaisesRegex(ValueError, 'wrong body'):
                    runner.run_sample('test', ['server'], 8080, args, Path(directory), 0)
                stop.assert_called_once_with(process)

    def test_disappearing_process_group_is_already_clean(self):
        process = Mock(pid=1234)
        process.poll.return_value = None
        with patch.object(runner.os, 'killpg', side_effect=ProcessLookupError):
            runner.stop_server(process)
        process.wait.assert_called_once_with(timeout=3)


if __name__ == '__main__':
    unittest.main()
