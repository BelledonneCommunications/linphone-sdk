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

"""App to analyze the Acoustic Echo Cancellation feature.

Runs and analyzes the tests of AEC3 suite of linphone-sdk. It can be used to run several times the tests,
to compute the MFCC similarity, to plot the results or to get the table with all metrics.
"""

from datetime import datetime
from pathlib import Path
from typing import Annotated

import pandas as pd
import typer

from tools.aec.aec_tests import AECTest

app = typer.Typer()


@app.command()
def main(
    build_path: Annotated[Path, typer.Option(help="Path to the build directory.")],
    linphone_path: Annotated[Path, typer.Option(help="Path to the linphone-sdk directory.")],
    test: Annotated[str, typer.Option(help="Name of the test to run. Use `all` to run all tests.")] = "",
    repeat: Annotated[int, typer.Option(help="Number of times the test is run.")] = 1,
    run_test: Annotated[bool, typer.Option(help="Run linphone-sdk tests.")] = True,  # noqa: FBT002, not positional argument
    fig: Annotated[bool, typer.Option(help="Display plotly figures.")] = True,  # noqa: FBT002, not positional argument
    analysis: Annotated[bool, typer.Option(help="Do the analysis for audio quality.")] = False,  # noqa: FBT002, not positional argument
) -> None:
    """Run the aec app.

    Runs the tests of AEC3 suite of linphone-sdk, then analyses the results and display them on figure. An additional
    metric based on MFCC representation is computed from the reference and filtered audio. All metrics are written in a
    global csv file.
    """
    start_time = datetime.now()  # noqa: DTZ005
    if not linphone_path.exists():
        raise ValueError(f"linphone path doesn't exist {linphone_path}")
    if not build_path.exists():
        raise ValueError(f"build path doesn't exist {build_path}")
    linphone_path_str = str(linphone_path)
    input_data_path = linphone_path_str + "/mediastreamer2/tester/sounds/"
    aec_dir = Path(__file__).resolve().parent
    audio_file_path = aec_dir / "../../../audio_files/aec/"
    if not audio_file_path.exists():
        audio_file_path.mkdir(parents=True, exist_ok=True)

    farend_simple = input_data_path + "farend_simple_talk.wav"
    nearend_simple = input_data_path + "nearend_simple_talk.wav"
    echo_simple_100ms = input_data_path + "echo_simple_talk_100ms.wav"
    echo_simple_600ms = input_data_path + "echo_simple_talk_600ms.wav"
    farend_double = input_data_path + "farend_double_talk.wav"
    nearend_double = input_data_path + "nearend_double_talk.wav"
    echo_double = input_data_path + "echo_double_talk_100ms.wav"
    echo_delay_change = input_data_path + "echo_delay_change.wav"
    all_res = []

    if test == "all":
        aec_test_list = [
            "simple_talk",
            "simple_talk_600ms_delay",
            "double_talk",
            "simple_talk_with_white_noise",
            "double_talk_with_white_noise",
            "near_end_single_talk",
            "far_end_single_talk",
            "simple_talk_48000_Hz",
            "simple_talk_with_delay_change",
            "simple_talk_without_initial_delay",
            "simple_talk_with_missing_packets",
        ]
    elif test != "":
        aec_test_list = [test]
    else:
        aec_test_list = [
            # "simple_talk",
            # "simple_talk_600ms_delay",
            # "double_talk",
            # "simple_talk_with_white_noise",
            # "double_talk_with_white_noise",
            # "near_end_single_talk",
            "far_end_single_talk",
            # "simple_talk_48000_Hz",
            # "simple_talk_with_delay_change",
            # "simple_talk_without_initial_delay",
            # "simple_talk_with_missing_packets",
        ]

    for input_test_name in aec_test_list:
        test_name = input_test_name.replace(" ", "_")
        for i in range(repeat):
            print(f"\n===== iteration {i} =====\n")
            output_path = audio_file_path.resolve() / f"run_{i}"
            Path(output_path).mkdir(parents=True, exist_ok=True)
            output_path_str = str(output_path) + "/"

            kwargs = {"output path": output_path, "start analysis": 15500}
            farend = ""
            echo = ""
            nearend = ""
            kwargs["alignment interval"] = [18500, 20500]

            if "simple_talk" in test_name:
                farend = farend_simple
                nearend = nearend_simple
                echo = echo_simple_100ms
                if test_name == "simple_talk_600ms":
                    echo = echo_simple_600ms
                    kwargs["alignment interval"] = [19000, 20000]
                elif test_name == "simple_talk_with_delay_change":
                    echo = echo_delay_change
                    kwargs["start analysis"] = 18500
            elif "double_talk" in test_name:
                kwargs["start analysis"] = 10000
                kwargs["alignment interval"] = [12500, 14500]
                farend = farend_double
                nearend = nearend_double
                echo = echo_double
            elif "near_end_single_talk" in test_name:
                nearend = nearend_double
                kwargs["start analysis"] = 0
                kwargs["alignment interval"] = [2000, 4000]
            elif "far_end_single_talk" in test_name:
                farend = farend_double
                echo = echo_double
                kwargs["start analysis"] = 0

            aec_test = AECTest(test_name, build_path.resolve(), **kwargs)
            if run_test:
                aec_test.run()
                aec_test.move_files()

            if analysis:
                aec_test.files.set_file(farend, echo, nearend)
                aec_test.files.read_audio_from_files()
                if fig:
                    fig_title = f"All audio for test {aec_test.test_suite_name}"
                    all_audio_fig_name = output_path_str + aec_test.test_name + "_all_audio.png"
                    aec_test.files.plot(fig_title=fig_title, fig_name=all_audio_fig_name)

                res = aec_test.get_results(fig)
                all_res.append(res)
                if fig:
                    metrics_fig_name = output_path_str + aec_test.test_name + "_metrics.png"
                    aec_test.plot_results(metrics_fig_name)

                res_tests = pd.DataFrame(all_res)
                print(res_tests.loc[:, ["test", "test passed", "asserts"]])
                res_tests.to_csv(str(audio_file_path) + "/metrics.csv", index=False, decimal=",")

    end_time = datetime.now()  # noqa: DTZ005
    print(f"\n === Duration: {end_time - start_time} ===")


if __name__ == "__main__":
    app()
