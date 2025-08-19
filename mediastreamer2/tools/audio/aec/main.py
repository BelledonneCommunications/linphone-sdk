from datetime import datetime
import pandas as pd
import os
import glob
from pathlib import Path
from aec_tests import AECTest

if __name__ == "__main__":

    start_time = datetime.now()

    input_data_path = "../../../tester/sounds/"
    build_path = "../../../../out/build/default-ninja/"

    farend_simple = input_data_path + "farend_simple_talk.wav"
    nearend_simple = input_data_path + "nearend_simple_talk.wav"
    echo_simple_100ms = input_data_path + "echo_simple_talk_100ms.wav"
    echo_simple_600ms = input_data_path + "echo_simple_talk_600ms.wav"
    farend_double = input_data_path + "farend_double_talk.wav"
    nearend_double = input_data_path + "nearend_double_talk.wav"
    echo_double = input_data_path + "echo_double_talk_100ms.wav"
    echo_delay_change = input_data_path + "echo_delay_change.wav"

    all_res = []

    # run all tests
    test_list = ["simple_talk",
                 "simple_talk_600ms",
                 "double_talk",
                 "simple_talk_white_noise",
                 "double_talk_white_noise",
                 "near_end_single_talk",
                 "far_end_single_talk",
                 "simple_talk_48000Hz",
                 "simple_talk_with_delay_change",
                 "simple_talk_without_initial_delay"]

    for test_name in test_list:

        for i in range(0, 10):
            output_path = build_path +  "run_" + str(i) + "/"
            Path(output_path).mkdir(parents=True, exist_ok=True)
            executable_path = os.path.join("..", "..", "..", "..", "out", "build", "test-ninja", "bin", "mediastreamer2-tester")

            kwargs = {
                "build path": build_path,
                "output path": output_path,
                "start analysis": 15500,
                "tester cmd": executable_path
            }
            farend = ""
            echo = ""
            nearend = ""

            if "simple_talk" in test_name:
                farend = farend_simple
                nearend = nearend_simple
                echo = echo_simple_100ms
                if test_name == "simple_talk_600ms":
                    echo = echo_simple_600ms
                elif test_name == "simple_talk_with_delay_change":
                    echo = echo_delay_change
                    kwargs["start analysis"] = 18500
            elif "double_talk" in test_name:
                kwargs["start analysis"] = 10000
                farend = farend_double
                nearend = nearend_double
                echo = echo_double
            elif "near_end_single_talk" in test_name:
                nearend = nearend_double
                kwargs["start analysis"] = 0
            elif "far_end_single_talk" in test_name:
                farend = farend_double
                echo = echo_double
                kwargs["start analysis"] = 0

            aec_test = AECTest(test_name, **kwargs)
            aec_test.run()
            aec_test.move_files()

            # move also input_mic file
            src_input_mic_file_name = "aec_input_mic.wav"
            dst_input_mic_file_name = output_path + "aec_input_mic_" + aec_test.test_name + ".wav"
            try:
                print(f"move {src_input_mic_file_name}")
                print(f"into {dst_input_mic_file_name}")
                os.rename(src_input_mic_file_name, dst_input_mic_file_name)
            except FileNotFoundError:
                print("no input mic file")

            aec_test.files.set_file(farend, echo, nearend)
            aec_test.files.read_audio_from_files()
            fig = aec_test.files.plot(f"All audio for test {aec_test.test_suite_name}")
            all_audio_fig_name = output_path + aec_test.test_name + "_all_audio.png"
            fig.write_image(all_audio_fig_name)

            res = aec_test.get_results()
            all_res.append(res)
            metrics_fig_name = output_path + aec_test.test_name + "_metrics.png"
            aec_test.plot_results(metrics_fig_name)

        res_tests = pd.DataFrame(all_res)
        print(res_tests.loc[:, ["test", "test passed", "asserts"]])
        res_tests.to_csv(build_path + "metrics.csv", index=False, decimal=",")

    end_time = datetime.now()
    print(f"\n === Duration: {end_time - start_time} ===")
