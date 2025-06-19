import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from audio_signal import AudioSignal


class AECFiles:
    """This class handles the PCM 16 bits audio files used for AEC study.The audio must have only 1 channel. The files
    give the audio data for ner-end, far-end, echo and output of AEC filter."""

    def __init__(self):

        self.nearend = AudioSignal()
        self.farend = AudioSignal()
        self.echo = AudioSignal()
        self.aec_output = AudioSignal()
        self.sample_rate_hz = 16000

    def set_file(self, farend_file, echo_file, nearend_file, aec_output_file=""):
        """Set the name of the files."""

        if farend_file != "":
            self.farend.file_name = farend_file
        if echo_file != "":
            self.echo.file_name = echo_file
        if nearend_file != "":
            self.nearend.file_name = nearend_file
        if aec_output_file != "":
            self.aec_output.file_name = aec_output_file

    def read_audio_from_files(self):
        """Read the audio data from files."""

        self.nearend.read_audio(self.sample_rate_hz)
        self.farend.read_audio(self.sample_rate_hz)
        self.echo.read_audio(self.sample_rate_hz)
        self.aec_output.read_audio(self.sample_rate_hz)

    def read_aec_output_audio_from_file(self):
        """Read the audio data for AEC output from file."""

        self.aec_output.read_audio(self.sample_rate_hz)
        if self.aec_output.data is None:
            print(f"ERROR while reading aec output wav file: no data read in {self.aec_output.file_name}")

    def write_files(self):
        """Write all audio data in files."""

        if self.farend.data is not None:
            print(f"write farend file {self.farend.file_name}")
            self.farend.write_in_file(self.farend.file_name)
        if self.nearend.data is not None:
            print(f"write nearend file {self.nearend.file_name}")
            self.nearend.write_in_file(self.nearend.file_name)
        if self.echo.data is not None:
            print(f"write echo file {self.echo.file_name}")
            self.echo.write_in_file(self.echo.file_name)
        if self.aec_output.data is not None:
            print(f"write AEC output file {self.aec_output.file_name}")
            self.aec_output.write_in_file(self.aec_output.file_name)

    def plot(self, fig_title=""):
        """Plot the audio and return the plotly figure."""

        sample_duration_s = 1. / self.sample_rate_hz
        signal_name = ["far-end", "echo", "near-end", "AEC output"]
        n_rows = 4
        fig = make_subplots(rows=n_rows, cols=1, shared_xaxes=True)
        for i, audio_signal in enumerate([self.farend.data, self.echo.data, self.nearend.data, self.aec_output.data]):
            if audio_signal is not None:
                timestamp = sample_duration_s * np.arange(audio_signal.size)
                signal = audio_signal / np.max(np.abs(audio_signal))
                fig.add_trace(go.Scatter(x=timestamp, y=signal, mode='lines', line={'width': 1},
                                         name=f"{signal_name[i]}"), row=i + 1, col=1)
            fig.update_yaxes(title_text=f'{signal_name[i]}', range=[-1., 1.], row=i + 1, col=1)
        fig.update_layout(title=fig_title)
        fig.update_xaxes(title_text='Time')
        fig.show()

        return fig

    def clear_audio(self):
        """Clear the audio data."""

        if self.farend.data is not None:
            self.farend = AudioSignal()
        if self.echo.data is not None:
            self.echo = AudioSignal()
        if self.nearend.data is not None:
            self.nearend = AudioSignal()
        if self.aec_output.data is not None:
            self.aec_output = AudioSignal()
