# Copyright (c) 2010-2025 Belledonne Communications SARL.
#
# This file is part of mediastreamer2
# (see https://gitlab.linphone.org/BC/public/mediastreamer2).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

"""Handle and run a test of a given linphone-sdk test suite.

The paths, suite and test names are set. The test is run with subprocess. the arguments of the command line are checked
before.
"""

import subprocess
from pathlib import Path


class SDKTest:
    """Handle and run a test of a linphone-sdk test suite."""

    def __init__(self, tester: str, suite: str, test_name: str, build_path: Path, log_path: Path) -> None:
        """Define the test and set variables to run it.

        The test is defined by the names of the suite and the test, the path to the executable and to the log file.
        :param tester: name of the executable, without path, like 'mediastreamer2-tester'.
        :type tester: str
        :param suite: suite name.
        :type suite: str
        :param test_name: Name of the test or of the related function in linphone-sdk test suite file.
        :type test_name: str
        :param build_path: Path to build directory that contains the executable at bin/[tester name].
        :type build_path: Path
        :param log_path: Path to the directory of the log file. The file name is the test name.
        :type log_path: Path
        :raises ValueError: if the executable is not found in build path or if the log path doesn't exist.
        """
        self.suite = suite
        self.test_name = test_name.replace(" ", "_")
        self.test_suite_name = test_name.replace("_", " ")
        self.build_path = build_path
        self.tester_cmd = self.build_path / "bin" / tester
        if not Path(self.tester_cmd).exists():
            raise ValueError(f"Unable to find executable {self.tester_cmd}")
        self.output_path = str(log_path) + "/"
        if not Path(self.output_path).exists():
            raise ValueError(f"Path for logs doesn't exist {self.output_path}")
        self.log_file = self.output_path + self.test_name + ".log"
        self.results = None

    @staticmethod
    def is_safe_path(arg: str) -> bool:
        """Check that the argument does not contain unauthorized or dangerous characters.

        :param arg: argument to test.
        :type: str
        :return: True if the argument is safe, False otherwise.
        :rtype: bool
        """
        for char in [";", "|", "&", "$", "../"]:
            if char in arg:
                print(f"found authorized characters {char} in {arg}")

        return isinstance(arg, str) and not any(char in arg for char in [";", "|", "&", "$", "../"])

    def run(self) -> None:
        """Check and run the test with command line.

        All arguments must be validated to ensure they do not contain unauthorized or dangerous characters that could
        lead to command injection.
        :raises ValueError: if an option of command is invalid.
        """
        print(f"\n=== run {self.test_name} ===")

        command = [
            self.tester_cmd,
            "--suite",
            self.suite,
            "--test",
            self.test_suite_name,
            "--verbose",
            "--log-file",
            self.log_file,
        ]
        print(command)

        if not all(
            self.is_safe_path(arg) for arg in [str(self.tester_cmd), self.suite, self.test_suite_name, self.log_file]
        ):
            raise ValueError(f"command {command} is unsafe.")

        try:
            result = subprocess.run(command, check=True, capture_output=True, text=True)  # noqa: S603, disable unsafe warning because the arguments are checked
            print("Output:", result.stdout)
        except subprocess.CalledProcessError as e:
            print(f"Error: command failed with code {e.returncode}\n{e.stderr}")
