from datetime import datetime
import pandas as pd
import os
import glob
from pathlib import Path
from aec_tests import AECTest
from noise_suppression_tests import NoiseSuppressionTest

if __name__ == "__main__":

    start_time = datetime.now()

    input_data_path = "../../../tester/sounds/"
    build_path = "../../../../out/build/default-ninja-no-sanitizer/"

    farend = input_data_path + "farend_simple_talk_48000_2.wav"
    nearend = input_data_path + "nearend_simple_talk_48000.wav"
    nearend_echo_400ms = input_data_path + "nearend_echo_400ms_simple_talk_48000.wav"
    nearend_echo_400ms_noise = input_data_path + "nearend_echo_400ms_noise_12dB_simple_talk_48000.wav"
    noisy_nearend_12dB = input_data_path + "noisy_nearend_12dB_simple_talk_48000.wav"
    noisy_nearend_6dB = input_data_path + "noisy_nearend_6dB_simple_talk_48000.wav"
    noisy_nearend_0dB = input_data_path + "noisy_nearend_0dB_simple_talk_48000.wav"
    all_res = []

    # run all tests
    aec_test_list = ["simple_talk",
                 "simple_talk_600ms",
                 "double_talk",
                 "simple_talk_white_noise",
                 "double_talk_white_noise",
                 "near_end_single_talk",
                 "far_end_single_talk",
                 "simple_talk_48000Hz",
                 "simple_talk_with_delay_change",
                 "simple_talk_without_initial_delay"]

    noise_suppression_test_list = [
        "talk_with_noise_snr_12dB",
        "talk_with_noise_snr_6dB",
        "talk_with_noise_snr_0dB",
        "noise_suppression_in_audio_stream",
        "noise_suppression_in_audio_stream_with_echo_400ms",
    ]

    no_run = False
    no_fig = False
    only_run = False
    if only_run:
        no_run = False
        no_fig = True

    for test_name in noise_suppression_test_list:

        for i in range(6, 7):

            # for model in ["mswebrtcns", "rnnoise"]:
            for model in ["rnnoise"]:
            # for model in ["mswebrtcns"]:
                output_path = build_path +  "run_" + str(i) + "/"
                Path(output_path).mkdir(parents=True, exist_ok=True)
                # executable_path = os.path.join("..", "..", "..", "..", "out", "build", "test-ninja", "bin", "mediastreamer2-tester")
                executable_path = os.path.join("/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/out/build/default-ninja-no-sanitizer", "bin",
                                               "mediastreamer2-tester")

                kwargs = {
                    "build path": build_path,
                    "output path": output_path,
                    "start analysis": 11000,
                    "tester cmd": executable_path,
                    "model": model
                }

                # kwargs = {
                #     "build path": build_path,
                #     "output path": output_path,
                #     "start analysis": 15500,
                #     "tester cmd": executable_path
                # }
                # farend = ""
                # echo = ""
                # nearend = ""

                # if "simple_talk" in test_name:
                #     farend = farend_simple
                #     nearend = nearend_simple
                #     echo = echo_simple_100ms
                #     if test_name == "simple_talk_600ms":
                #         echo = echo_simple_600ms
                #     elif test_name == "simple_talk_with_delay_change":
                #         echo = echo_delay_change
                #         kwargs["start analysis"] = 18500
                # elif "double_talk" in test_name:
                #     kwargs["start analysis"] = 10000
                #     farend = farend_double
                #     nearend = nearend_double
                #     echo = echo_double
                # elif "near_end_single_talk" in test_name:
                #     nearend = nearend_double
                #     kwargs["start analysis"] = 0
                # elif "far_end_single_talk" in test_name:
                #     farend = farend_double
                #     echo = echo_double
                #     kwargs["start analysis"] = 0

                # aec_test.files.set_file(farend, echo, nearend)
                # aec_test.files.read_audio_from_files()
                # fig = aec_test.files.plot(f"All audio for test {aec_test.test_suite_name}")
                # all_audio_fig_name = output_path + aec_test.test_name + "_all_audio.png"
                # fig.write_image(all_audio_fig_name)

                # res = aec_test.get_results()
                # all_res.append(res)
                # metrics_fig_name = output_path + aec_test.test_name + "_metrics.png"
                # aec_test.plot_results(metrics_fig_name)

                print(test_name)

                kwargs["start analysis"] = 0
                speech = nearend
                noise = ""
                noisy_speech = noisy_nearend_12dB
                clean = output_path + "clean_" + test_name + ".wav"
                if "12dB" in test_name:
                    noisy_speech = noisy_nearend_12dB
                elif "6dB" in test_name:
                    noisy_speech = noisy_nearend_6dB
                elif "0dB" in test_name:
                    noisy_speech = noisy_nearend_0dB
                elif test_name == "noise_suppression_in_audio_stream":
                    noisy_speech = noisy_nearend_12dB
                elif test_name == "noise_suppression_in_audio_stream_with_echo_400ms":
                    noisy_speech = noisy_nearend_12dB
                    kwargs["start analysis"] = 15400

                ns_test = NoiseSuppressionTest(test_name, **kwargs)
                if not no_run or only_run:
                    ns_test.run()
                    ns_test.files.set_file(speech, noise, noisy_speech, clean)
                    ns_test.move_files()

            # # move also input_mic file
            # src_input_mic_file_name = "aec_input_mic.wav"
            # dst_input_mic_file_name = output_path + "aec_input_mic_" + aec_test.test_name + ".wav"
            # try:
            #     print(f"move {src_input_mic_file_name}")
            #     print(f"into {dst_input_mic_file_name}")
            #     os.rename(src_input_mic_file_name, dst_input_mic_file_name)
            # except FileNotFoundError:
            #     print("no input mic file")

                if not only_run:

                    ns_test.files.set_file(speech, noise, noisy_speech, clean)
                    ns_test.files.speech.sample_rate_hz = 48000
                    ns_test.files.noise.sample_rate_hz = 48000
                    ns_test.files.noisy_speech.sample_rate_hz = 48000
                    ns_test.files.clean.sample_rate_hz = 48000
                    ns_test.files.read_audio_from_files()
                    if not no_fig:
                        ns_test.files.plot(f"All audio for test {ns_test.test_suite_name}")

                    res = ns_test.get_results(no_fig)
                    all_res.append(res)

            # aec_test = AECTest(test_name, **kwargs)
            # aec_test.run()
            # aec_test.move_files()
            #
            # # move also input_mic file
            # src_input_mic_file_name = "aec_input_mic.wav"
            # dst_input_mic_file_name = output_path + "aec_input_mic_" + aec_test.test_name + ".wav"
            # try:
            #     print(f"move {src_input_mic_file_name}")
            #     print(f"into {dst_input_mic_file_name}")
            #     os.rename(src_input_mic_file_name, dst_input_mic_file_name)
            # except FileNotFoundError:
            #     print("no input mic file")
            #
            # # move other output for test several delays
            # if "simple_talks_with_several_delays" in test_name:
            #     aec_output_files = glob.glob("aec_output_delay_*ms_*.wav")
            #     for src_file_name in aec_output_files:
            #         delay_str = src_file_name.split("aec_output_delay_")[1].split("_")[0]
            #         dst_file_name = output_path + "aec_output_delay_" + delay_str + ".wav"
            #         try:
            #             print(f"move {src_file_name}")
            #             print(f"into {dst_file_name}")
            #             os.rename(src_file_name, dst_file_name)
            #         except FileNotFoundError:
            #             print("can't move delay file", src_file_name, "into", dst_file_name)
            #
            # aec_test.files.set_file(farend, echo, nearend)
            # aec_test.files.read_audio_from_files()
            # aec_test.files.plot(f"All audio for test {aec_test.test_suite_name}")
            #
            # res = aec_test.get_results()
            # all_res.append(res)
            # aec_test.plot_results()

            if not only_run:
                res_tests = pd.DataFrame(all_res)
                print(res_tests.loc[:, ["test", "test passed", "asserts"]])
                res_tests.to_csv(build_path + "metrics.csv", index=False, decimal=",")

    end_time = datetime.now()
    print(f"\n === Duration: {end_time - start_time} ===")
