import re
from datetime import datetime
import pandas as pd
import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import librosa


class AecMetrics:
    """ Handle the metrics used in AEC:
     - estimated delay
     - ERL
     - ERLE
     The metrics are read in the log file written during AEC test from mediastreamer2-tester. The dump of AEC metrics
     must be enabled.
     """

    def __init__(self):

        self.metrics = None
        self.delay_final = 0
        self.erl_final = 0
        self.erle_final = 0
        self.pattern_for_floats = r'[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?'
        self.real_delay = None
        self.changes = []


    def get_metrics_from_logs(self, logs):
        """ Extract the metrics from a list of lines that have been parsed from a log file."""

        data = []
        for line in logs:
            try:
                audio_str = line.split('\n')[0].split("metrics :")[1]
                matches = re.findall(self.pattern_for_floats, audio_str)
                ts = datetime.strptime(line.split(f' bctbx-message')[0], '%Y-%m-%d %H:%M:%S:%f')
                delay = float(matches[0])
                erl = 0
                erle = 0
                if len(matches) > 1:
                    erl = float(matches[1])
                    erle = float(matches[2])
                new_row = {"timestamp": ts,
                           "delay": delay,
                           "erl": erl,
                           "erle": erle}
                data.append(new_row)
            except IndexError:
                pass

        if len(data) > 0:
            self.metrics = pd.DataFrame(data)
            self.delay_final = self.metrics["delay"].iloc[-1]
            self.erl_final = self.metrics["erl"].iloc[-1]
            self.erle_final = self.metrics["erle"].iloc[-1]
            self.metrics['time_diff_seconds'] = (
                        self.metrics['timestamp'] - self.metrics['timestamp'].min()).dt.total_seconds()

    def print_final_values(self):
        print("final metrics:")
        # if similarity != 0:
        #     print(f"\tdelay = {delay[-1]}")
        print(f"\tdelay = {self.delay_final} ms")
        print(f"\terl = {self.erl_final}")
        print(f"\terle = {self.erle_final}")

    def plot_with_audio(self, signal_timestamps, signal, fig_title):
        """Plot the metrics given time, with the related audio signal."""

        fig = make_subplots(rows=4, cols=1, shared_xaxes=True)

        fig.add_trace(go.Scatter(x=signal_timestamps, y=signal, mode='lines', line={'width': 1},
                                 name=f"output"), row=1, col=1)

        fig.add_trace(
            go.Scatter(x=self.metrics["time_diff_seconds"], y=self.metrics["delay"], mode='lines', name="delay"),
            row=2, col=1)
        if self.real_delay is not None:
            fig.add_trace(
                go.Scatter(x=self.real_delay["time_diff_seconds"], y=self.real_delay["delay"], mode='lines',
                           line={'shape': 'hv'},
                           name="real delay"), row=2, col=1)
        fig.add_trace(go.Scatter(x=self.metrics["time_diff_seconds"], y=self.metrics["erl"], mode='lines', name="ERL"),
                      row=3,
                      col=1)
        fig.add_trace(
            go.Scatter(x=self.metrics["time_diff_seconds"], y=self.metrics["erle"], mode='lines', name="ERLE"), row=4,
            col=1)

        if len(self.changes) > 0:
            show = True
            for i in range(4):
                for ts in self.changes:
                    fig.add_vline(
                        x=ts, line_color='black', line_width=1, name="delay change",
                        showlegend=show, row=i + 1, col=1)
                    show = False

        fig.update_layout(title=fig_title)
        fig.update_xaxes(title_text='Time (s)')
        fig.update_yaxes(title_text='Waveform', range=[-1., 1.], row=1, col=1)
        fig.update_yaxes(title_text='Delay (ms)', rangemode="tozero", row=2, col=1)
        fig.update_yaxes(title_text='ERL', row=3, col=1)
        fig.update_yaxes(title_text='ERLE', row=4, col=1)

        fig.show()

        return fig
