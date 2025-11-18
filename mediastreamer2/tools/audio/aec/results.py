#!/usr/bin/python
# 
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
 
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

from audio_signal import AudioSignal

# -- tracer signaux clean pour différents tests

ref_path = "/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/mediastreamer2/tester/sounds/"
data_path = "/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/out/build/default-ninja-no-sanitizer/run_2/"
ref_file = ref_path + "nearend_simple_talk_48000.wav"
# mic_file = ref_path + "nearend_echo_550ms_noise_12dB_simple_talk_48000.wav"
# clean_aec_noise_file = data_path + "clean_audio_stream_with_echo_550ms_and_noise.wav"
# clean_aec_ns_noise_file = data_path + "clean_noise_suppression_in_audio_stream_with_echo_550ms_and_noise.wav"
mic_file = ref_path + "nearend_echo_600ms_noise_12dB_simple_talk_48000.wav"
clean_aec_noise_file = data_path + "clean_audio_stream_with_echo_600ms_and_noise.wav"
clean_aec_ns_noise_file = data_path + "clean_noise_suppression_in_audio_stream_with_echo_600ms_and_noise.wav"

ref = AudioSignal()
mic = AudioSignal()
clean_aec = AudioSignal()
clean_aec_ns = AudioSignal()
ref.read_audio_from_file(ref_file)
mic.read_audio_from_file(mic_file)
clean_aec.read_audio_from_file(clean_aec_noise_file)
clean_aec_ns.read_audio_from_file(clean_aec_ns_noise_file)

signal_name = ["reference", "input from mic", "output after AEC", "output after AEC and NS"]
n_rows = len(signal_name)
fig = make_subplots(rows=n_rows, cols=1, shared_xaxes=True)
for i, audio in enumerate([ref, mic, clean_aec, clean_aec_ns]):
    audio_signal = audio.data
    if audio_signal is not None:
        print(f"signal {signal_name[i]}, rate = {audio.sample_rate_hz} Hz")
        # signal = audio_signal / np.max(np.abs(audio_signal)) # normalize audio
        signal = audio_signal  # not normalized audio
        fig.add_trace(go.Scatter(x=audio.timestamps, y=signal, mode='lines', line={'width': 1},
                                 name=f"{signal_name[i]}"), row=i + 1, col=1)
    # fig.update_yaxes(title_text=f'{signal_name[i]}', range=[-1., 1.], row=i + 1, col=1)
    fig.update_yaxes(title_text=f'{signal_name[i]}', range=[-0.3, 0.3], row=i + 1, col=1)

fig.update_layout(title="Audio before and after filtering in audio stream")
fig.update_xaxes(title_text='Time')
fig.show()


# --  tracer courbes sim = f(dB)

# table_file = "/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/out/build/default-ninja/run_0/metrics.ods"
# df = pd.read_excel(table_file)
# print(df)
#
#
# df_real_noise = df[df["noise"] == "real (echo 4)"]
#
# df_rnnoise_real_noise = df_real_noise[df_real_noise["model"] == "rnnoise"]
# df_webrtcns_real_noise = df_real_noise[df_real_noise["model"] == "mswebrtcns"]
# df_rnnoise_white_noise = df[df["noise"] == "white noise"]
#
# print(f"RNNoise with white noise:\n{df_rnnoise_white_noise}")
# print(f"RNNoise with real noise:\n{df_rnnoise_real_noise}")
# print(f"WebRTCNS with real noise:\n{df_webrtcns_real_noise}")
#
# fig = make_subplots(rows=1, cols=2, shared_xaxes=True)
# RNNoise_col = "#1f77b4"
# WebRTCNS_col = "#ff7f0e"
# line_width = 2
# marker_size = 10
# # plot sim_talk = f(SNR)
# fig.add_trace(go.Scatter(
#     x=df_webrtcns_real_noise["SNR (dB)"],
#     y=df_webrtcns_real_noise["similarity"],
#     mode='lines+markers',
#     line={'width': line_width, "color": WebRTCNS_col},
#     marker={"symbol": "circle-open", "size": marker_size},
#     name=f"WebRTC NS, real noise"),
#     row=1, col=1)
# fig.add_trace(go.Scatter(
#     x=df_rnnoise_real_noise["SNR (dB)"],
#     y=df_rnnoise_real_noise["similarity"],
#     mode='lines+markers',
#     line={'width': line_width, "color": RNNoise_col},
#     marker={"symbol": "circle-open", "size": marker_size},
#     name=f"RNNoise, real noise"),
#     row=1, col=1)
# fig.add_trace(go.Scatter(
#     x=df_rnnoise_white_noise["SNR (dB)"],
#     y=df_rnnoise_white_noise["similarity"],
#     mode='lines+markers',
#     line={'width': line_width, "color": RNNoise_col},
#     marker={"symbol": "star-open", "size": marker_size},
#     name=f"RNNoise, white noise"),
#     row=1, col=1)
# fig.update_yaxes(title_text='Similarity in speech parts', range=[0.8, 1.], row=1, col=1)
#
# # plot energy_silence = f(SNR)
# fig.add_trace(go.Scatter(
#     x=df_webrtcns_real_noise["SNR (dB)"],
#     y=df_webrtcns_real_noise["energy in silence"],
#     mode='lines+markers',
#     line={'width': line_width, "color": WebRTCNS_col},
#     marker={"symbol": "circle-open", "size": 10},
#     name=f"WebRTC NS, real noise",
#     showlegend=False,),
#     row=1, col=2)
# fig.add_trace(go.Scatter(
#     x=df_rnnoise_real_noise["SNR (dB)"],
#     y=df_rnnoise_real_noise["energy in silence"],
#     mode='lines+markers',
#     line={'width': line_width, "color": RNNoise_col},
#     marker={"symbol": "circle-open", "size": 10},
#     name=f"RNNoise, real noise",
#     showlegend=False,),
#     row=1, col=2)
# fig.add_trace(go.Scatter(
#     x=df_rnnoise_white_noise["SNR (dB)"],
#     y=df_rnnoise_white_noise["energy in silence"],
#     mode='lines+markers',
#     line={'width': line_width, "color": RNNoise_col},
#     marker={"symbol": "star-open", "size": 10},
#     name=f"RNNoise, white noise",
#     showlegend=False,),
#     row=1, col=2)
# fig.update_yaxes(title_text='Remaining energy in silence parts', range=[0, 2], row=1, col=2)
#
#
# fig.update_layout(title="Noise suppressor performances")
# fig.update_xaxes(title_text='SNR in dB')
# fig.show()


