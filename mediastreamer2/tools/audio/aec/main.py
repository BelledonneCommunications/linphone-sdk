from datetime import datetime
import pandas as pd
from aec_tests import AECTest

if __name__ == "__main__":

    start_time = datetime.now()

    input_data_path = "../../../tester/sounds/"
    build_path = "../../../../out/build/default-ninja/"
    output_path = build_path

    farend_simple = input_data_path + "farend_simple_talk.wav"
    nearend_simple = input_data_path + "nearend_simple_talk.wav"
    echo_simple = input_data_path + "echo_simple_talk.wav"
    farend_double = input_data_path + "farend_double_talk.wav"
    nearend_double = input_data_path + "nearend_double_talk.wav"
    echo_double = input_data_path + "echo_double_talk.wav"
    echo_delay_change = input_data_path + "echo_delay_change.wav"

    all_res = []

    # run all tests
    test_list = ["simple_talk",
                 "double_talk",
                 "simple_talk_white_noise",
                 "double_talk_white_noise",
                 "near_end_single_talk",
                 "far_end_single_talk",
                 "simple_talk_48000Hz",
                 "simple_talk_with_delay_change",
                 "simple_talks_with_several_delays"]

    for test_name in test_list:

        kwargs = {
            "build path": build_path,
            "output path": output_path,
            "start analysis": 11000
        }
        farend = ""
        echo = ""
        nearend = ""

        if "simple_talk" in test_name:
            kwargs["start analysis"] = 11000
            farend = farend_simple
            nearend = nearend_simple
            echo = echo_simple
            if test_name == "simple_talk_with_delay_change":
                echo = echo_delay_change
        elif "double_talk" in test_name:
            kwargs["start analysis"] = 9500
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

        aec_test.files.set_file(farend, echo, nearend)
        aec_test.files.read_audio_from_files()
        aec_test.files.plot(f"All audio for test {aec_test.test_suite_name}")

        res = aec_test.get_results()
        all_res.append(res)
        aec_test.plot_results()

    res_tests = pd.DataFrame(all_res)
    print(res_tests.loc[:, ["test", "energy in silence", "similarity"]])
    res_tests.to_csv(output_path + "metrics.csv", index=False, decimal=",")

    end_time = datetime.now()
    print(f"\n === Duration: {end_time - start_time} ===")
