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

"""Handle the set of audio files for the Noise Suppression tests.

The files contain the following audio signals:
- speech, reference audio with talk
- noise_speech, audio with noisy talk, set as input of the noise suppression filter
- clean, denoised audio, output of the noise suppression filter.
"""

import plotly.graph_objects as go
from plotly.subplots import make_subplots

from tools.common.audio.audio_signal import AudioSignal


class NoiseSuppressionFiles:
    """Handles the set of PCM 16 bits wav audio files used for the Noise Suppression tests.

    The audio must have only 1 channel and the same sample rate. The files give the audio data for reference speech,
    noisy speech and output of denoising filter.
    """

    def __init__(self) -> None:
        """Initialize the AudioComparison instance with default values.

        The audio are instances of the AudioSignal class.
        """
        self.speech = AudioSignal()
        self.noisy_speech = AudioSignal()
        self.clean = AudioSignal()
        self.sample_rate_hz = 48000

    def set_file(self, speech_file: str, noisy_speech_file: str = "", clean_file: str = "") -> None:
        """Set the name of the audio PCM 16 bits wav audio files.

        The audio must have only 1 channel and the same sample rate.
        :param speech_file: Name of the reference speech file.
        :type speech_file: str
        :param noisy_speech_file: Optional, name of the noisy speech file. Default is empty.
        :type noisy_speech_file: str
        :param clean_file: Optional, name of the near-end file. Default is empty.
        :type clean_file: str
        """
        if speech_file != "":
            self.speech.file_name = speech_file
        if noisy_speech_file != "":
            self.noisy_speech.file_name = noisy_speech_file
            print(f"set noisy speech file to {noisy_speech_file}")
        if clean_file != "":
            self.clean.file_name = clean_file

    def read_audio_from_files(self) -> None:
        """Read the audio data from files.

        The file names can be set with set_file() method or directly.
        """
        print("*** read_audio_from_files ***")
        self.speech.read_audio()
        if self.noisy_speech.file_name != "":
            self.noisy_speech.read_audio()
        self.clean.read_audio()

    def read_test_output_audio_from_file(self) -> None:
        """Read the audio data for filter output from file."""
        self.noisy_speech.read_audio()
        if self.noisy_speech.data is None:
            print(f"ERROR while reading noisy speech wav file: no data read in {self.noisy_speech.file_name}")

        self.clean.read_audio()
        if self.clean.data is None:
            print(f"ERROR while reading clean wav file: no data read in {self.clean.file_name}")

    def write_files(self) -> None:
        """Write all audio data in files."""
        if self.speech.data is not None:
            print(f"write speech file {self.speech.file_name}")
            self.speech.write_in_file(self.speech.file_name)
        if self.noisy_speech.data is not None:
            print(f"write noisy speech file {self.noisy_speech.file_name}")
            self.noisy_speech.write_in_file(self.noisy_speech.file_name)
        if self.clean.data is not None:
            print(f"write clean file {self.clean.file_name}")
            self.clean.write_in_file(self.clean.file_name)

    def plot(self, fig_title: str = "") -> go.Figure:
        """Plot all audios and return the plotly figure.

        :param fig_title: Optional, title of the figure. Default is empty.
        :type fig_title: str
        :return: Plotly figure with the audio signals set for the AEC study.
        :rtype: go.Figure
        """
        signal_name = ["speech", "noisy input", "clean output"]
        n_rows = len(signal_name)
        fig = make_subplots(rows=n_rows, cols=1, shared_xaxes=True)
        for i, audio in enumerate([self.speech, self.noisy_speech, self.clean]):
            audio_signal = audio.data
            if audio_signal is not None:
                print(f"signal {signal_name[i]}, rate = {audio.sample_rate_hz} Hz")
                # signal = audio_signal / np.max(np.abs(audio_signal)) # normalize audio
                signal = audio_signal  # not normalized audio
                fig.add_trace(
                    go.Scatter(
                        x=audio.timestamps,
                        y=signal,
                        mode="lines",
                        line={"width": 1},
                        name=f"{signal_name[i]}",
                    ),
                    row=i + 1,
                    col=1,
                )
            # fig.update_yaxes(title_text=f'{signal_name[i]}', range=[-1., 1.], row=i + 1, col=1)
            fig.update_yaxes(title_text=f"{signal_name[i]}", range=[-0.3, 0.3], row=i + 1, col=1)

        fig.update_layout(title=fig_title)
        fig.update_xaxes(title_text="Time", range=[0, self.clean.total_duration_s])
        fig.show()

        return fig

    def clear_audio(self) -> None:
        """Clear the audio data."""
        if self.speech.data is not None:
            self.speech = AudioSignal()
        if self.noisy_speech.data is not None:
            self.noisy_speech = AudioSignal()
        if self.clean.data is not None:
            self.clean = AudioSignal()
