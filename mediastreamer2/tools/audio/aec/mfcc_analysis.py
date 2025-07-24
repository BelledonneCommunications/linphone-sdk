from dtw import dtw
import matplotlib.pyplot as plt
import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots

from mfcc_comparison import mfcc_analysis

def compute_show_dtw(x, y, fig_list=[], figname=""):
    """
    Compute and show the distance measured by Dynamic Time Warping. For figure name, use threeway or density.
    """

    alignment = dtw(x, y, keep_internals=True)
    print(f"DTW distance with ref = {alignment.distance:0.0f}")

    # print(alignment.index1)
    # print(alignment.index2)

    for fig_name in fig_list:
        alignment.plot(type=fig_name)
        if figname != "":
            plt.savefig(figname)
        else:
            plt.show()

    return alignment


path = "/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/out/build/default-ninja-no-sanitizer/run_0/"

# test_list = [
    # "crystal_clear_talk",
    # "talk_with_noise_snr_15dB",
    # "crystal_clear_audio_in_audio_stream",
    # "noise_suppression_in_audio_stream"
# ]

mfcc_unfiltered_talk = mfcc_analysis(None, 48000)
mfcc_unfiltered_talk.read_difference_with_ref(path + "crystal_clear_talk_only_MFCC_difference_with_ref_on_talk.npy")

mfcc_unfiltered_talk_with_noise = mfcc_analysis(None, 48000)
mfcc_unfiltered_talk_with_noise.read_difference_with_ref(path + "talk_with_noise_snr_15dB_on_bypass_mode_MFCC_difference_with_ref_on_talk.npy")

mfcc_talk = mfcc_analysis(None, 48000)
mfcc_talk.read_difference_with_ref(path + "crystal_clear_talk_MFCC_difference_with_ref_on_talk.npy")

mfcc_talk_with_noise = mfcc_analysis(None, 48000)
mfcc_talk_with_noise.read_difference_with_ref(path + "talk_with_noise_snr_15dB_MFCC_difference_with_ref_on_talk.npy")

mfcc_audiostream = mfcc_analysis(None, 48000)
mfcc_audiostream.read_difference_with_ref(path + "crystal_clear_audio_in_audio_stream_MFCC_difference_with_ref_on_talk.npy")

mfcc_audiostream_with_noise = mfcc_analysis(None, 48000)
mfcc_audiostream_with_noise.read_difference_with_ref(path + "noise_suppression_in_audio_stream_MFCC_difference_with_ref_on_talk.npy")

fig_list = ["threeway"]
# fig_list = []

line_width = 2
n_coef = len(mfcc_talk.diff)
x = np.arange(0, n_coef)

# # impact of filter, talk without noise
# dtw_talk_ns_filter_impact= compute_show_dtw(mfcc_unfiltered_talk.diff, mfcc_talk.diff, fig_list, path + "dtw_ns_filter_impact_on_talk_without_noise_in_filter.png")
# dtw_talk_noise_impact_without_filter= compute_show_dtw(mfcc_unfiltered_talk_with_noise.diff, mfcc_unfiltered_talk.diff, fig_list, path + "dtw_noise_impact_without_filter.png")
# dtw_talk_noise_impact= compute_show_dtw(mfcc_talk_with_noise.diff, mfcc_talk.diff, fig_list, path + "dtw_noise_impact_in_filter.png")
# dtw_audiostream_noise_impact = compute_show_dtw(mfcc_audiostream_with_noise.diff, mfcc_audiostream.diff, fig_list, path + "dtw_noise_impact_in_audio_stream.png")
# dtw_audiostream_impact = compute_show_dtw(mfcc_audiostream.diff, mfcc_talk.diff, fig_list, path + "dtw_full_graph_impact_without_noise.png")
# dtw_audiostream_with_noise_impact = compute_show_dtw(mfcc_audiostream_with_noise.diff, mfcc_talk_with_noise.diff, fig_list, path + "dtw_full_graph_impact_with_noise.png")
# dtw_talk_filter_impact_on_noise= compute_show_dtw(mfcc_talk_with_noise.diff, mfcc_unfiltered_talk_with_noise.diff, fig_list, path + "dtw_noise_impact_with_and_without_filter.png")
#
# # print(f"DTW in filter graph, NS impact, without noise\t\t  = {dtw_talk_ns_filter_impact.distance:0.0f}")
# # print(f"DTW in filter graph, NS impact, with noise\t\t\t  = {dtw_talk_noise_impact_without_filter.distance:0.0f}")
# # print(f"DTW in filter graph, noise impact\t\t\t\t\t  = {dtw_talk_noise_impact.distance:0.0f}")
# # print(f"DTW in audio stream, noise impact\t\t\t\t\t  = {dtw_audiostream_noise_impact.distance:0.0f}")
# # print(f"DTW in filter graph vs in audio stream, without noise = {dtw_audiostream_impact.distance:0.0f}")
# # print(f"DTW in filter graph vs in audio stream, with noise\t  = {dtw_audiostream_with_noise_impact.distance:0.0f}")
# # print(f"DTW in filter graph, noise impact with and without NS = {dtw_talk_filter_impact_on_noise.distance:0.0f}")
#
# # # --- noise impact
# dtw_noise_impact_without_filter= compute_show_dtw(mfcc_unfiltered_talk.diff, mfcc_unfiltered_talk_with_noise.diff, fig_list, path + "dtw_noise_impact_without_filter.png")
# dtw_noise_impact_in_ns_filter = compute_show_dtw(mfcc_talk_with_noise.diff, mfcc_talk.diff, fig_list, path + "dtw_noise_impact_in_ns_filter.png")
# dtw_noise_impact_in_audiostream = compute_show_dtw(mfcc_audiostream_with_noise.diff, mfcc_audiostream.diff, fig_list, path + "dtw_noise_impact_in_audio_stream.png")
#
# print(f"DTW between without and with noise, no NS filter                \t\t  = {dtw_noise_impact_without_filter.distance:0.0f}")
# print(f"DTW between without and with noise, in NS filter                \t\t  = {dtw_noise_impact_in_ns_filter.distance:0.0f}")
# print(f"DTW between without and with noise, in audio stream             \t\t  = {dtw_noise_impact_in_audiostream.distance:0.0f}")

# fig = make_subplots(rows=1, cols=1, shared_xaxes=True)
# fig.add_trace(go.Scatter(x=dtw_noise_impact_without_filter.index1, y=dtw_noise_impact_without_filter.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between without and with noise, no NS filter"), row=1, col=1)
# fig.add_trace(go.Scatter(x=dtw_noise_impact_in_ns_filter.index1, y=dtw_noise_impact_in_ns_filter.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between without and with noise, in NS filter"), row=1, col=1)
# fig.add_trace(go.Scatter(x=dtw_noise_impact_in_audiostream.index1, y=dtw_noise_impact_in_audiostream.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between without and with noise, in audio stream"), row=1, col=1)
# fig.update_xaxes(range=[0, n_coef], row=1, col=1)
# fig.update_yaxes(range=[0, n_coef], row=1, col=1)
#
# fig.update_layout(title="DTW on MFCC comparisons")
# fig.show()

# --- audio stream impact
# dtw_audiostream_impact_without_noise = compute_show_dtw(mfcc_audiostream.diff, mfcc_talk.diff, fig_list, path + "dtw_audiostream_impact_without_noise.png")
# dtw_audiostream_impact_with_noise = compute_show_dtw(mfcc_audiostream_with_noise.diff, mfcc_talk_with_noise.diff, fig_list, path + "dtw_audiostream_impact_with_noise.png")
#
# print(f"DTW between NS filter alone and in audio stream, no noise   \t\t = {dtw_audiostream_impact_without_noise.distance:0.0f}")
# print(f"DTW between NS filter alone and in audio stream, with noise \t\t = {dtw_audiostream_impact_with_noise.distance:0.0f}")
#
# fig = make_subplots(rows=1, cols=1, shared_xaxes=True)
# fig.add_trace(go.Scatter(x=dtw_audiostream_impact_without_noise.index1, y=dtw_audiostream_impact_without_noise.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between NS filter alone and in audio stream, no noise"), row=1, col=1)
# fig.add_trace(go.Scatter(x=dtw_audiostream_impact_with_noise.index1, y=dtw_audiostream_impact_with_noise.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between NS filter alone and in audio stream, with noise"), row=1, col=1)
# fig.update_xaxes(range=[0, n_coef], row=1, col=1)
# fig.update_yaxes(range=[0, n_coef], row=1, col=1)
#
# fig.update_layout(title="DTW on MFCC comparisons")
# fig.show()

# # --- NS filter impact
# dtw_talk_ns_filter_impact_with_noise= compute_show_dtw(mfcc_talk_with_noise.diff, mfcc_unfiltered_talk_with_noise.diff, fig_list, path + "dtw_noise_impact_without_filter.png")
# dtw_talk_ns_filter_impact_in_audiostream= compute_show_dtw(mfcc_audiostream.diff, mfcc_unfiltered_talk.diff, fig_list, path + "dtw_noise_impact_in_filter.png")
# dtw_talk_ns_filter_in_audiostream_impact_with_noise = compute_show_dtw(mfcc_audiostream_with_noise.diff, mfcc_unfiltered_talk_with_noise.diff, fig_list, path + "dtw_noise_impact_in_audio_stream.png")
#
# print(f"DTW between without and with NS filter, no noise                  \t\t  = {dtw_talk_ns_filter_impact.distance:0.0f}")
# print(f"DTW between without and with NS filter, with noise                \t\t  = {dtw_talk_ns_filter_impact_with_noise.distance:0.0f}")
# print(f"DTW between without and with NS filter in audio stream, no noise  \t\t  = {dtw_talk_ns_filter_impact_in_audiostream.distance:0.0f}")
# print(f"DTW between without and with NS filter in audio stream, with noise\t\t  = {dtw_talk_ns_filter_in_audiostream_impact_with_noise.distance:0.0f}")
#
# fig = make_subplots(rows=1, cols=2, shared_xaxes=True)
# fig.add_trace(go.Scatter(x=dtw_talk_ns_filter_impact.index1, y=dtw_talk_ns_filter_impact.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between without and with NS filter, without noise"), row=1, col=1)
# fig.add_trace(go.Scatter(x=dtw_talk_ns_filter_impact_with_noise.index1, y=dtw_talk_ns_filter_impact_with_noise.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between without and with NS filter, with noise"), row=1, col=1)
# fig.update_xaxes(range=[0, n_coef], row=1, col=1)
# fig.update_yaxes(range=[0, n_coef], row=1, col=1)
#
# fig.add_trace(go.Scatter(x=dtw_talk_ns_filter_impact_in_audiostream.index1, y=dtw_talk_ns_filter_impact_in_audiostream.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between without and with NS filter in audio stream, without noise"), row=1, col=2)
# fig.add_trace(go.Scatter(x=dtw_talk_ns_filter_in_audiostream_impact_with_noise.index1, y=dtw_talk_ns_filter_in_audiostream_impact_with_noise.index2, mode='lines',
#                          line={'width': line_width}, name=f"DTW between without and with NS filter in audio stream, with noise"), row=1, col=2)
# fig.update_xaxes(range=[0, n_coef], row=1, col=2)
# fig.update_yaxes(range=[0, n_coef], row=1, col=2)
#
# fig.update_layout(title="DTW on MFCC comparisons")
# fig.show()
# # ---

# n_coef = len(mfcc_talk.diff)
# x = np.arange(0, n_coef)
# fig = go.Figure()
# fig.add_trace(go.Scatter(x=x, y=mfcc_unfiltered_talk.diff, mode='lines', line={'width': line_width}, name=f"no NS filter, no noise"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_unfiltered_talk_with_noise.diff, mode='lines', line={'width': line_width}, name=f"no NS filter, with noise"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_talk.diff, mode='lines', line={'width': line_width}, name=f"NS filter, no noise"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_talk_with_noise.diff, mode='lines', line={'width': line_width}, name=f"NS filter, with noise"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream.diff, mode='lines', line={'width': line_width}, name=f"NS filter in audio stream, no noise"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_with_noise.diff, mode='lines', line={'width': line_width}, name=f"NS filter in audio stream, with noise"))
# fig.update_layout(title="Difference between MFCC representation of the reference audio and the output audio, on speech parts")
# fig.update_xaxes(title_text='Time')
# fig.update_yaxes(title_text='Distance between MFCC of reference and output')
# fig.show()

# ---
# n_coef = len(mfcc_talk.diff)
# x = np.arange(0, n_coef)
# fig = go.Figure()
# fig.add_trace(go.Scatter(x=x, y=mfcc_unfiltered_talk.diff, mode='lines', line={'width': line_width}, name=f"no NS filter, no noise, in graph"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_unfiltered_talk_with_noise.diff, mode='lines', line={'width': line_width}, name=f"no NS filter, with noise, in graph"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_talk.diff, mode='lines', line={'width': line_width}, name=f"filter"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_talk_with_noise.diff, mode='lines', line={'width': line_width}, name=f"filter with noise"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream.diff, mode='lines', line={'width': line_width}, name=f"audio stream"))
# fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_with_noise.diff, mode='lines', line={'width': line_width}, name=f"audio stream with noise"))
# fig.update_layout(title="Difference between MFCC representation of the reference audio and the output audio, on speech parts")
# fig.update_xaxes(title_text='Time')
# fig.update_yaxes(title_text='Distance between MFCC of reference and output')
# fig.show()
#
# fig = make_subplots(rows=1, cols=3, shared_xaxes=True)
# fig.add_trace(go.Scatter(x=dtw_talk_ns_filter_impact.index1, y=dtw_talk_ns_filter_impact.index2, mode='lines',
#                          line={'width': line_width}, name=f"NS filter impact in graph, without noise"), row=1, col=1)
# fig.add_trace(go.Scatter(x=dtw_talk_filter_impact_on_noise.index1, y=dtw_talk_filter_impact_on_noise.index2, mode='lines',
#                          line={'width': line_width}, name=f"NS filter impact in graph, with noise"), row=1, col=1)
# fig.update_xaxes(range=[0, n_coef], row=1, col=1)
# fig.update_yaxes(range=[0, n_coef], row=1, col=1)
#
# # fig = make_subplots(rows=1, cols=3, shared_xaxes=True)
# # fig.add_trace(go.Scatter(x=dtw_talk_ns_filter_impact.index1, y=dtw_talk_ns_filter_impact.index2, mode='lines',
# #                          line={'width': line_width}, name=f"NS filter impact in filter, without noise"), row=1, col=1)
# # fig.update_xaxes(range=[0, n_coef], row=1, col=1)
# # fig.update_yaxes(range=[0, n_coef], row=1, col=1)
#
# # fig = make_subplots(rows=1, cols=3, shared_xaxes=True)
# # fig.add_trace(go.Scatter(x=dtw_talk_noise_impact_without_filter.index1, y=dtw_talk_noise_impact_without_filter.index2, mode='lines',
# #                          line={'width': line_width}, name=f"noise impact without filter"), row=1, col=1)
# # fig.update_xaxes(range=[0, n_coef], row=1, col=1)
# # fig.update_yaxes(range=[0, n_coef], row=1, col=1)
#
# fig.add_trace(go.Scatter(x=dtw_talk_noise_impact.index1, y=dtw_talk_noise_impact.index2, mode='lines',
#                          line={'width': line_width}, name=f"noise impact in filter"), row=1, col=2)
# fig.add_trace(go.Scatter(x=dtw_audiostream_noise_impact.index1, y=dtw_audiostream_noise_impact.index2, mode='lines',
#                          line={'width': line_width}, name=f"noise impact in audio stream"), row=1, col=2)
# fig.update_xaxes(range=[0, n_coef], row=1, col=2)
# fig.update_yaxes(range=[0, n_coef], row=1, col=2)
#
# fig.add_trace(go.Scatter(x=dtw_audiostream_impact.index1, y=dtw_audiostream_impact.index2, mode='lines',
#                          line={'width': line_width}, name=f"audio stream impact, no noise"), row=1, col=3)
# fig.add_trace(go.Scatter(x=dtw_audiostream_with_noise_impact.index1, y=dtw_audiostream_with_noise_impact.index2, mode='lines',
#                          line={'width': line_width}, name=f"audio stream impact, with noise"), row=1, col=3)
# fig.update_xaxes(range=[0, n_coef], row=1, col=3)
# fig.update_yaxes(range=[0, n_coef], row=1, col=3)
#
# fig.update_layout(title="DTW on MFCC comparisons")
# fig.show()

# # --- impact in audiostream
path = "/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/out/build/default-ninja-no-sanitizer/run_2/"

mfcc_audiostream = mfcc_analysis(None, 48000)
mfcc_audiostream.read_difference_with_ref(path + "noise_suppression_on_bypass_in_audio_stream_without_noise_MFCC_difference_with_ref_on_talk.npy")

mfcc_audiostream_with_noise = mfcc_analysis(None, 48000)
mfcc_audiostream_with_noise.read_difference_with_ref(path + "noise_suppression_on_bypass_in_audio_stream_with_noise_MFCC_difference_with_ref_on_talk.npy")

mfcc_audiostream_ns = mfcc_analysis(None, 48000)
mfcc_audiostream_ns.read_difference_with_ref(path + "noise_suppression_in_audio_stream_without_noise_MFCC_difference_with_ref_on_talk.npy")

mfcc_audiostream_ns_with_noise = mfcc_analysis(None, 48000)
mfcc_audiostream_ns_with_noise.read_difference_with_ref(path + "noise_suppression_in_audio_stream_MFCC_difference_with_ref_on_talk.npy")

line_width = 2
n_coef = len(mfcc_audiostream.diff)
x = np.arange(0, n_coef)

# impact of filter in audio stream
n_coef = len(mfcc_talk.diff)
x = np.arange(0, n_coef)
fig = go.Figure()
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream.diff, mode='lines', line={'width': line_width}, name=f"no NS filter, no noise"))
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_with_noise.diff, mode='lines', line={'width': line_width}, name=f"no NS filter, with noise"))
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_ns.diff, mode='lines', line={'width': line_width}, name=f"NS filter, no noise"))
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_ns_with_noise.diff, mode='lines', line={'width': line_width}, name=f"NS filter, with noise"))
fig.update_layout(title="Difference between MFCC representation of the reference audio and the output audio, on speech parts")
fig.update_xaxes(title_text='Time')
fig.update_yaxes(range = [0, 300], title_text='Distance between MFCC of reference and output')
fig.show()

# --- impact of echo in audiostream

path = "/home/flore/Dev/Linphone-sdk/submodules_removed/linphone-sdk/out/build/default-ninja-no-sanitizer/run_2/"

mfcc_audiostream_with_aec = mfcc_analysis(None, 48000)
mfcc_audiostream_with_aec_and_noise = mfcc_analysis(None, 48000)
mfcc_audiostream_with_aec_ns = mfcc_analysis(None, 48000)
mfcc_audiostream_with_aec_ns_with_noise = mfcc_analysis(None, 48000)

mfcc_audiostream_with_aec.read_difference_with_ref(path + "audio_stream_with_echo_400ms_MFCC_difference_with_ref_on_talk.npy")
mfcc_audiostream_with_aec_and_noise.read_difference_with_ref(path + "audio_stream_with_echo_400ms_and_noise_MFCC_difference_with_ref_on_talk.npy")
mfcc_audiostream_with_aec_ns.read_difference_with_ref(path + "noise_suppression_in_audio_stream_with_echo_400ms_MFCC_difference_with_ref_on_talk.npy")
mfcc_audiostream_with_aec_ns_with_noise.read_difference_with_ref(path + "noise_suppression_in_audio_stream_with_echo_400ms_and_noise_MFCC_difference_with_ref_on_talk.npy")

# mfcc_audiostream_with_aec.read_difference_with_ref(path + "audio_stream_with_echo_550ms_MFCC_difference_with_ref_on_talk.npy")
# mfcc_audiostream_with_aec_and_noise.read_difference_with_ref(path + "audio_stream_with_echo_550ms_and_noise_MFCC_difference_with_ref_on_talk.npy")
# mfcc_audiostream_with_aec_ns.read_difference_with_ref(path + "noise_suppression_in_audio_stream_with_echo_550ms_MFCC_difference_with_ref_on_talk.npy")
# mfcc_audiostream_with_aec_ns_with_noise.read_difference_with_ref(path + "noise_suppression_in_audio_stream_with_echo_550ms_and_noise_MFCC_difference_with_ref_on_talk.npy")


line_width = 2
n_coef = len(mfcc_audiostream_with_aec.diff)
x = np.arange(0, n_coef)

# impact of filter in audio stream
n_coef = len(mfcc_talk.diff)
x = np.arange(0, n_coef)
fig = go.Figure()
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_with_aec.diff, mode='lines', line={'width': line_width}, name=f"AEC, no NS filter, no noise"))
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_with_aec_and_noise.diff, mode='lines', line={'width': line_width}, name=f"AEC, no NS filter, with noise"))
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_with_aec_ns.diff, mode='lines', line={'width': line_width}, name=f"AEC, NS filter, no noise"))
fig.add_trace(go.Scatter(x=x, y=mfcc_audiostream_with_aec_ns_with_noise.diff, mode='lines', line={'width': line_width}, name=f"AEC, NS filter, with noise"))
fig.update_layout(title="Difference between MFCC representation of the reference audio and the output audio, on speech parts")
fig.update_xaxes(title_text='Time')
fig.update_yaxes(range = [0, 300], title_text='Distance between MFCC of reference and output')
fig.show()