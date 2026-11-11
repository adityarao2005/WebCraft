"""Exercise command construction and failure propagation without native tools."""
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("webcraft_runner", Path(__file__).parents[1] / "webcraft.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def invoke(self, system="Linux", extra=(), returncode=0, action="check"):
        self.commands = []

        class Process:
            stdout = io.StringIO("native tool output\n")
            def __enter__(self):
                return self
            def __exit__(self, *args):
                pass
            def wait(self):
                return self.returncode

        def start(command, **kwargs):
            self.commands.append(command)
            process = Process()
            process.returncode = returncode[len(self.commands) - 1] if isinstance(returncode, list) else returncode
            process.stdout = io.StringIO("native tool output\n")
            return process

        with tempfile.TemporaryDirectory(prefix="webcraft test ") as directory:
            with patch.object(runner.platform, "system", return_value=system), \
                 patch.object(runner.platform, "platform", return_value=system), \
                 patch.object(runner.subprocess, "Popen", side_effect=start), \
                 patch("sys.stdout", new_callable=io.StringIO):
                result = runner.main([action, "--build-dir", directory, *extra])
            self.log = (Path(directory) / "webcraft.log").read_text()
        return result

    def test_platform_presets_and_configuration(self):
        for system, preset in runner.PRESETS.items():
            with self.subTest(system=system):
                self.assertEqual(self.invoke(system, ["--config", "Release"]), 0)
                self.assertIn(preset, self.commands[0])
                self.assertIn("-DCMAKE_BUILD_TYPE=Release", self.commands[0])
                self.assertIn("--config", self.commands[1])
                self.assertIn("Release", self.commands[2])
                self.assertIn("--output-junit", self.commands[2])
                self.assertIn("--no-tests=error", self.commands[2])
                self.assertIn("native tool output", self.log)
                self.assertFalse(any("--repeat" in command for command in self.commands))

    def test_sanitizer_filter_and_extra_definitions(self):
        self.invoke(extra=["--sanitizer", "address-undefined", "--filter", "RuntimeTestSuite", "--define", "CUSTOM=value with spaces"])
        self.assertIn("-DWEBCRAFT_SANITIZER=address-undefined", self.commands[0])
        self.assertIn("-DCUSTOM=value with spaces", self.commands[0])
        self.assertEqual(self.commands[2][-2:], ["-R", "RuntimeTestSuite"])

    def test_failure_stops_pipeline_without_retry(self):
        with self.assertRaises(subprocess.CalledProcessError) as error:
            self.invoke(returncode=7)
        self.assertEqual(error.exception.returncode, 7)
        self.assertEqual(len(self.commands), 1)

    def test_build_and_test_failures_are_preserved(self):
        for codes, count in (([0, 8], 2), ([0, 0, 9], 3)):
            with self.subTest(codes=codes), self.assertRaises(subprocess.CalledProcessError) as error:
                self.invoke(returncode=codes)
            self.assertEqual(error.exception.returncode, codes[-1])
            self.assertEqual(len(self.commands), count)

    def test_individual_stages(self):
        for action, executable in (("configure", "cmake"), ("build", "cmake"), ("test", "ctest")):
            self.invoke(action=action)
            self.assertEqual(len(self.commands), 1)
            self.assertEqual(self.commands[0][0], executable)

    def test_invalid_options_fail_before_launch(self):
        for options in (["--jobs", "0"], ["--define", "invalid"], ["--sanitizer", "address"]):
            with self.subTest(options=options), self.assertRaises(SystemExit), \
                 patch("sys.stderr", new_callable=io.StringIO):
                self.invoke("Windows", options)
            self.assertEqual(self.commands, [])

    def test_cache_guard_refuses_toolchain_changes(self):
        for entry, options in (
            ("CMAKE_GENERATOR:INTERNAL=Unix Makefiles", []),
            ("CMAKE_GENERATOR:INTERNAL=Ninja\nCMAKE_CXX_COMPILER:FILEPATH=/different/compiler", ["--cxx-compiler", "c++"]),
            ("CMAKE_GENERATOR:INTERNAL=Ninja\nWEBCRAFT_SANITIZER:STRING=address", []),
        ):
            with self.subTest(entry=entry), tempfile.TemporaryDirectory() as directory:
                (Path(directory) / "CMakeCache.txt").write_text(entry)
                with patch.object(runner.platform, "system", return_value="Linux"), \
                     patch.object(runner.subprocess, "Popen") as launch, \
                     patch("sys.stderr", new_callable=io.StringIO), self.assertRaises(SystemExit):
                    runner.main(["configure", "--build-dir", directory, *options])
                launch.assert_not_called()


if __name__ == "__main__":
    unittest.main()
