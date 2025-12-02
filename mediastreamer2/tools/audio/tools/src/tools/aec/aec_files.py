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

"""Handle the set of audio files for the Acoustic Echo Cancellation tests (AEC).

The files contain the following audio signals:
- near-end, from local speaker, except for far-end single talk tests
- far-end, from remote speaker, except for near-end single talk tests
- echo, the reverberated far-end, except for near-end single talk tests
- aec output, the output signal that has been filtered out by the echo canceller, from output 1 of the filter
- aec reference, the reference signal read in output 0 of AEC filter (should be the far-end).
"""

from contextlib import suppress

import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots

from tools.common.audio.audio_signal import AudioSignal


class AECFiles:
    """Handles the set of PCM 16 bits wav audio files used for the Acoustic Echo Cancellation tests (AEC).

    All audios must have only 1 channel and the same sample rate. The files give the audio data for near-end, far-end,
    echo and output of AEC filter. The filtered audio is aec_output_clean.
    """

    def __init__(self) -> None:
        """Initialize the AudioComparison instance with default values.

        The audio are instances of the AudioSignal class.
        """
        self.nearend = AudioSignal()
        self.farend = AudioSignal()
        self.echo = AudioSignal()
        self.aec_output_clean = AudioSignal()
        self.aec_output_ref = AudioSignal()
        self.sample_rate_hz = 16000

    def set_file(
        self,
        farend_file: str,
        echo_file: str,
        nearend_file: str,
        aec_output_clean_file: str = "",
        aec_output_ref_file: str = "",
    ) -> None:
        """Set the name of the audio PCM 16 bits wav audio files.

        All audios must have only 1 channel and the same sample rate.
        :param farend_file: Name of the far-end file.
        :type farend_file: str
        :param echo_file: Name of the echo file.
        :type echo_file: str
        :param nearend_file: Name of the near-end file.
        :type nearend_file: str
        :param aec_output_clean_file: Optional, name of the output filtered file. Default is empty.
        :type aec_output_clean_file: str
        :param aec_output_ref_file: Optional, name of the output reference file. Default is empty.
        :type aec_output_ref_file: str
        """
        if farend_file != "":
            self.farend.file_name = farend_file
        if echo_file != "":
            self.echo.file_name = echo_file
        if nearend_file != "":
            self.nearend.file_name = nearend_file
        if aec_output_clean_file != "":
            self.aec_output_clean.file_name = aec_output_clean_file
        if aec_output_ref_file != "":
            self.aec_output_ref.file_name = aec_output_ref_file

    def read_audio_from_files(self) -> None:
        """Read the audio data from files.

        The file names can be set with set_file() method or directly.
        """
        self.nearend.read_audio()
        self.farend.read_audio()
        self.echo.read_audio()
        self.aec_output_clean.read_audio()
        with suppress(FileNotFoundError):
            self.aec_output_ref.read_audio()

    def read_aec_output_audio_from_file(self) -> None:
        """Read the audio data for both AEC output from files."""
        self.aec_output_clean.read_audio()
        if self.aec_output_clean.data is None:
            print(f"ERROR while reading aec output clean wav file: no data read in {self.aec_output_clean.file_name}")

        self.aec_output_ref.read_audio()
        if self.aec_output_ref.data is None:
            print(f"ERROR while reading aec output wav ref file: no data read in {self.aec_output_ref.file_name}")

    def write_files(self) -> None:
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
        if self.aec_output_clean.data is not None:
            print(f"write AEC output file {self.aec_output_clean.file_name}")
            self.aec_output_clean.write_in_file(self.aec_output_clean.file_name)
        if self.aec_output_ref.data is not None:
            print(f"write AEC output file {self.aec_output_ref.file_name}")
            self.aec_output_ref.write_in_file(self.aec_output_ref.file_name)

    def plot(self, fig_title: str = "", fig_name: str = "") -> go.Figure:
        """Plot all audios and return the plotly figure.

        :param fig_title: Optional, title of the figure. Default is empty.
        :type fig_title: str
        :param fig_name: Optional, name of the file to save the figure, if provided. Default is empty.
        :type fig_name: str
        :return: Plotly figure with the audio signals set for the AEC study.
        :rtype: go.Figure
        """
        sample_duration_s = 1.0 / self.sample_rate_hz
        signal_name = [
            "far-end",
            "echo",
            "near-end",
            "AEC output clean",
            "AEC output ref",
        ]
        n_rows = 5
        fig = make_subplots(rows=n_rows, cols=1, shared_xaxes=True)
        for i, audio_signal in enumerate(
            [
                self.farend.data,
                self.echo.data,
                self.nearend.data,
                self.aec_output_clean.data,
                self.aec_output_ref.data,
            ]
        ):
            if audio_signal is not None:
                timestamp = sample_duration_s * np.arange(audio_signal.size)
                signal = audio_signal / np.max(np.abs(audio_signal))
                fig.add_trace(
                    go.Scatter(
                        x=timestamp,
                        y=signal,
                        mode="lines",
                        line={"width": 1},
                        name=f"{signal_name[i]}",
                    ),
                    row=i + 1,
                    col=1,
                )
            fig.update_yaxes(title_text=f"{signal_name[i]}", range=[-1.0, 1.0], row=i + 1, col=1)
        fig.update_layout(title=fig_title)
        fig.update_xaxes(title_text="Time")
        fig.show()
        if fig_name != "":
            fig.write_image(fig_name)

        return fig

    def clear_audio(self) -> None:
        """Clear the audio data."""
        if self.farend.data is not None:
            self.farend = AudioSignal()
        if self.echo.data is not None:
            self.echo = AudioSignal()
        if self.nearend.data is not None:
            self.nearend = AudioSignal()
        if self.aec_output_clean.data is not None:
            self.aec_output_ref = AudioSignal()
        if self.aec_output_clean.data is not None:
            self.aec_output_ref = AudioSignal()
