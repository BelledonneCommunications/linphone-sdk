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

"""App to analyse the Noise Suppression feature.

Runs and analyses the tests of Noise Suppression suite of linphone-sdk. It can be used to run several times the tests,
to compute the MFCC similarity, to plot the results or to get the table with all metrics.
"""

from datetime import datetime
from pathlib import Path
from typing import Annotated

import pandas as pd
import typer

from tools.noise_suppression.noise_suppression_tests import NoiseSuppressionTest

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
    """Run the noise suppression app.

    Runs the tests of Noise Suppression suite of linphone-sdk, then analyses the results and display them on figure. An
    additional metrics based on MFCC representation is computed from the reference and filtered audio. All metrics are
    written in a global csv file.
    """
    start_time = datetime.now()  # noqa: DTZ005, do not need time zone in duration measurement
    if not linphone_path.exists():
        raise ValueError(f"linphone path doesn't exist {linphone_path}")
    if not build_path.exists():
        raise ValueError(f"build path doesn't exist {build_path}")
    linphone_path_str = str(linphone_path)
    input_data_path = linphone_path_str + "/mediastreamer2/tester/sounds/"
    noise_suppr_dir = Path(__file__).resolve().parent
    audio_file_path = noise_suppr_dir / "../../../audio_files/noise_suppression/"
    if not audio_file_path.exists():
        audio_file_path.mkdir(parents=True, exist_ok=True)

    nearend = input_data_path + "nearend_simple_talk_48000.wav"
    nearend_echo_400ms_noise = input_data_path + "nearend_echo_400ms_noise_12dB_simple_talk_48000.wav"
    noisy_nearend_12db = input_data_path + "noisy_nearend_12dB_simple_talk_48000.wav"
    noisy_nearend_6db = input_data_path + "noisy_nearend_6dB_simple_talk_48000.wav"
    noisy_nearend_0db = input_data_path + "noisy_nearend_0dB_simple_talk_48000.wav"
    all_res = []

    if test == "all":
        noise_suppression_test_list = [
            "talk_with_noise_snr_12dB",
            "talk_with_noise_snr_6dB",
            "talk_with_noise_snr_0dB",
            "noise_suppression_in_audio_stream",
            "noise_suppression_in_audio_stream_with_echo_400ms",  # do not run this one with sanitizer
        ]
    elif test != "":
        noise_suppression_test_list = [test]
    else:
        noise_suppression_test_list = [
            "talk_with_noise_snr_12dB",
            # "talk_with_noise_snr_6dB",
            # "talk_with_noise_snr_0dB",
            # "noise_suppression_in_audio_stream",
            # "noise_suppression_in_audio_stream_with_echo_400ms",
        ]

    for input_test_name in noise_suppression_test_list:
        test_name = input_test_name.replace(" ", "_")
        test_name = test_name.lower().replace("db", "dB")
        for i in range(repeat):
            print(f"\n===== iteration {i} =====\n")
            output_path = audio_file_path.resolve() / f"run_{i}"
            Path(output_path).mkdir(parents=True, exist_ok=True)
            output_path_str = str(output_path) + "/"
            kwargs = {
                "output path": output_path,
                "start analysis": 0,
                "alignment interval": [3500, 4500],
            }
            speech = nearend
            noisy_speech = noisy_nearend_12db
            clean = output_path_str + "clean_" + test_name + ".wav"
            if "12dB" in test_name:
                noisy_speech = noisy_nearend_12db
            elif "6dB" in test_name:
                noisy_speech = noisy_nearend_6db
            elif "0dB" in test_name:
                noisy_speech = noisy_nearend_0db
            elif test_name == "noise_suppression_in_audio_stream":
                noisy_speech = noisy_nearend_12db
            elif test_name == "noise_suppression_in_audio_stream_with_echo_400ms":
                print("Warning: compile without sanitizer for this test.")
                noisy_speech = nearend_echo_400ms_noise
                kwargs["start analysis"] = 15400
                kwargs["alignment interval"] = [18500, 19500]

            ns_test = NoiseSuppressionTest(test_name, build_path.resolve(), **kwargs)
            if run_test:
                ns_test.run()
                ns_test.files.set_file(speech, noisy_speech, clean)
                ns_test.move_files()

            if analysis:
                ns_test.files.set_file(speech, noisy_speech, clean)
                ns_test.files.speech.sample_rate_hz = 48000
                ns_test.files.noisy_speech.sample_rate_hz = 48000
                ns_test.files.clean.sample_rate_hz = 48000
                ns_test.files.read_audio_from_files()
                if fig:
                    ns_test.files.plot(f"All audio for test {ns_test.test_suite_name}")

                res = ns_test.get_results(fig)
                all_res.append(res)

                res_tests = pd.DataFrame(all_res)
                print(res_tests.loc[:, ["test", "test passed", "asserts"]])
                res_tests.to_csv(str(audio_file_path) + "/metrics.csv", index=False, decimal=",")

    end_time = datetime.now()  # noqa: DTZ005
    print(f"\n === Duration: {end_time - start_time} ===")


if __name__ == "__main__":
    app()
