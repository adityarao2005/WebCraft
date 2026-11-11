#!/usr/bin/env python3
"""Configure, build and test WebCraft using the repository's platform presets."""
import argparse
import os
from pathlib import Path
import platform
import shlex
import shutil
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PRESETS = {"Linux": "linux-build", "Darwin": "macos-build", "Windows": "windows-build"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("configure", "build", "test", "check"))
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--config", default="Debug")
    parser.add_argument("--preset", choices=tuple(PRESETS.values()))
    parser.add_argument("--cxx-compiler")
    parser.add_argument("--sanitizer", choices=("none", "address", "undefined", "address-undefined"), default="none")
    parser.add_argument("--filter", help="CTest regular expression")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--define", action="append", default=[], metavar="NAME=VALUE")
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    system = platform.system()
    if system not in PRESETS:
        parser.error(f"unsupported platform: {system}")
    if system == "Windows" and args.sanitizer != "none":
        parser.error("sanitizer checks require GCC/Clang on Linux or macOS")
    if any("=" not in item for item in args.define):
        parser.error("--define requires NAME=VALUE")
    if any(item.split("=", 1)[0].split(":", 1)[0] in
           {"CMAKE_CXX_COMPILER", "CMAKE_BUILD_TYPE", "WEBCRAFT_SANITIZER"} for item in args.define):
        parser.error("use --cxx-compiler, --config, and --sanitizer for protected cache settings")
    build = (args.build_dir or Path("build" if args.sanitizer == "none" else f"build-{args.sanitizer}")).resolve()
    if args.action in ("configure", "check") and (build / "CMakeCache.txt").exists():
        cache = {}
        for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
            if "=" in line and not line.startswith(("//", "#")):
                key, value = line.split("=", 1)
                cache[key.split(":", 1)[0]] = value
        presets = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
        selected = args.preset or PRESETS[system]
        selected_preset = next(p for p in presets["configurePresets"] if p["name"] == selected)
        generator = selected_preset["generator"]
        if cache.get("CMAKE_GENERATOR", generator) != generator:
            parser.error("generator differs from existing cache; use a separate --build-dir")
        requested_compiler = args.cxx_compiler or selected_preset.get("cacheVariables", {}).get("CMAKE_CXX_COMPILER")
        if requested_compiler:
            found = shutil.which(requested_compiler)
            compiler = found or requested_compiler
            existing = cache.get("CMAKE_CXX_COMPILER", compiler)
            # Visual Studio resolves cl.exe even outside a developer prompt.
            same_msvc = (system == "Windows" and not found and requested_compiler.lower() in ("cl", "cl.exe")
                         and Path(existing).name.lower() == "cl.exe")
            if not same_msvc and Path(existing).resolve() != Path(compiler).resolve():
                parser.error("compiler differs from existing cache; use a separate --build-dir")
        if cache.get("WEBCRAFT_SANITIZER", "none") != args.sanitizer:
            parser.error("sanitizer differs from existing cache; use a separate --build-dir")
    build.mkdir(parents=True, exist_ok=True)
    # Record every command and its output, including configure/build failures.
    with (build / "webcraft.log").open("a", encoding="utf-8") as log:
        def run(command):
            heading = f"[{platform.platform()}] {shlex.join(map(str, command))}\n"
            print(heading, end="", flush=True)
            log.write(heading)
            log.flush()
            environment = os.environ.copy()
            environment.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
            with subprocess.Popen(list(map(str, command)), cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace") as process:
                for line in process.stdout:
                    print(line, end="", flush=True)
                    log.write(line)
                    log.flush()
                result = process.wait()
            log.write(f"exit code: {result}\n")
            log.flush()
            if result:
                raise subprocess.CalledProcessError(result, command)

        if args.action in ("configure", "check"):
            command = ["cmake", "--preset", args.preset or PRESETS[system], "-S", ROOT, "-B", build,
                       f"-DCMAKE_BUILD_TYPE={args.config}", "-DWEBCRAFT_BUILD_TESTS=ON",
                       f"-DWEBCRAFT_SANITIZER={args.sanitizer}"]
            if args.cxx_compiler:
                command.append(f"-DCMAKE_CXX_COMPILER={args.cxx_compiler}")
            command.extend(f"-D{item}" for item in args.define)
            run(command)
        if args.action in ("build", "check"):
            run(["cmake", "--build", build, "--config", args.config, "--parallel", args.jobs])
        if args.action in ("test", "check"):
            # CTest owns the existing per-test timeout. Never retry failed tests.
            command = ["ctest", "--test-dir", build, "-C", args.config, "--output-on-failure",
                       "--no-tests=error", "--output-junit", build / "test-results.xml"]
            if args.filter:
                command.extend(["-R", args.filter])
            run(command)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"WebCraft: {error}", file=sys.stderr)
        sys.exit(error.returncode if isinstance(error, subprocess.CalledProcessError) else 1)
