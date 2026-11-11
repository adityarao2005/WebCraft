"""Rerun the file/adaptor test and sample a hang without leaving it running."""

import argparse
from pathlib import Path
import subprocess
import time


TEST_FILTER = "AsyncFSTestSuite.TestFileReadAllUsingAdaptors"


def diagnose(binary, output_dir, sample_after=5, timeout=30):
    binary = Path(binary).resolve()
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    with (output_dir / "diagnostic.log").open("w", buffering=1) as diagnostic:
        def log(message):
            print(message, flush=True)
            print(message, file=diagnostic, flush=True)

        command = [str(binary), f"--gtest_filter={TEST_FILTER}", "--gtest_color=no"]
        log(f"Rerunning {TEST_FILTER}; sample after {sample_after}s, limit {timeout}s")
        started = time.monotonic()
        with (output_dir / "test.log").open("w") as test_log:
            process = subprocess.Popen(
                command, cwd=binary.parent, stdout=test_log, stderr=subprocess.STDOUT
            )
            try:
                try:
                    result = process.wait(timeout=sample_after)
                    log(f"Test exited before sampling with status {result}; no hang captured")
                    return result
                except subprocess.TimeoutExpired:
                    log(f"Test still running (PID {process.pid}); capturing all thread stacks")

                with (output_dir / "sample.log").open("w") as sample_log:
                    try:
                        sample = subprocess.run(
                            ["/usr/bin/sample", str(process.pid), "2", "1", "-file",
                             str(output_dir / "sample.txt")],
                            stdout=sample_log, stderr=subprocess.STDOUT,
                            timeout=min(10, max(0.1, timeout - (time.monotonic() - started))),
                            check=False,
                        )
                        log(f"sample exited with status {sample.returncode}; see sample.log/sample.txt")
                    except (OSError, subprocess.TimeoutExpired) as error:
                        log(f"Could not capture thread stacks: {error}")

                try:
                    result = process.wait(timeout=max(0.1, timeout - (time.monotonic() - started)))
                    log(f"Test exited after sampling with status {result}")
                    return result
                except subprocess.TimeoutExpired:
                    log("Diagnostic time limit reached; stopping the test")
                    return 124
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    raise SystemExit(diagnose(args.binary, args.output_dir))
