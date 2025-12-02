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

"""Compare audio with reference.

Provide several tools to compare audio data and get quality metrics, like energy, or similarity measurements based on
correlation or MFCC representation.
The silence and talk parts can be processed separately.
"""

import numpy as np
import plotly.graph_objects as go
from numpy.typing import NDArray
from plotly.subplots import make_subplots

from tools.common.audio.audio_signal import AudioSignal
from tools.common.audio.mfcc_analysis import MFCCAnalysis


class AudioComparison:
    """Compare a tested audio with a reference audio using multiple acoustic criteria.

    This class provides methods to compare two audio signals (tested and reference) based on various metrics, including:
        - Similarity between MFCC (Mel-Frequency Cepstral Coefficients) representations,
        - Correlation between the signals,
        - Energy levels.

    The comparison are performed on specific segments containing speech or silence.
    """

    def __init__(self) -> None:
        """Initialize the AudioComparison instance with default values."""
        self.tested_signal = None
        self.reference_signal = None
        self.sampling_rate_hz = 0
        self.energy_in_silence = 0.0
        self.energy_in_silence_with_noise = 0.0
        self.similarity_in_speech = 0.0
        self.silence_mask = None
        self.start_ms = 0
        self.distance_mfcc = 0.0
        self.similarity_mfcc = 0.0

    def set_audio(self, tested_signal: AudioSignal, reference_signal: AudioSignal) -> None:
        """Set audio data.

        Both signals must have the same sample rate. The reference signal must not be shorter than the tested one.
        :param tested_signal: tested audio signal.
        :type tested_signal: AudioSignal
        :param reference_signal: audio signal taken as reference.
        :type reference_signal: AudioSignal
        :raises ValueError: Exception raised if the sample rates are not the same or if the reference is shorter than
            the tested signal.
        """
        self.tested_signal = tested_signal
        self.reference_signal = reference_signal
        self.sampling_rate_hz = tested_signal.sample_rate_hz

        if self.reference_signal.data is not None:
            if self.tested_signal.sample_rate_hz != self.reference_signal.sample_rate_hz:
                raise ValueError(
                    f"Tested and reference signals don't have the same sampling rate : \
{self.tested_signal.sample_rate_hz} - {self.reference_signal.sample_rate_hz} Hz."
                )
            if self.tested_signal.data.size > self.reference_signal.data.size:
                self.tested_signal.data = self.tested_signal.data[: self.reference_signal.data.size]
                print("Reference signal is shorter than tested one -> truncate the end of tested signal.")

    def set_start_analysis(self, start_ms: int) -> int:
        """Set the time stamp when to start the comparison, in ms.

        :param start_ms: start time in ms.
        :type start_ms: int
        :return: the index of the sample related to start_ms.
        :rtype: int
        """
        if start_ms > 0:
            self.start_ms = start_ms
        self.start_sample = int(float(self.start_ms * self.sampling_rate_hz) / 1000.0)
        print(f"start audio comparison at sample {self.start_sample} ({self.start_ms} ms)")

        return self.start_sample

    @staticmethod
    def truncate_aligned_end_to_same_size(audio_list: [AudioSignal]) -> None:
        """Truncate all aligned_data vectors in audio_list to the size of the shortest one.

        :param audio_list: List of AudioSignal whose aligned_data must be truncated.
        :type audio_list: [AudioSignal]
        """
        if not audio_list:
            return

        new_size = min(audio.aligned_data.size for audio in audio_list)
        print(f"set size to {new_size} to all audio signals")
        for i in range(len(audio_list)):
            audio_list[i].aligned_data = audio_list[i].aligned_data[:new_size]

    @staticmethod
    def compute_correlation(tested_data: NDArray[np.float64], reference_data: NDArray[np.float64]) -> tuple[int, float]:
        """Return the maximal cross-correlation and its position (in samples) between two audio signals.

        :param tested_data: tested audio signal.
        :type tested_data: NDArray[np.float64]
        :param reference_data: audio signal taken as reference.
        :type reference_data: NDArray[np.float64]
        :return position of the maximal cross-correlation and maximal cross-correlation
        """
        max_length = max(len(reference_data), len(tested_data))
        padded_ref = np.pad(np.abs(reference_data), (0, max_length - len(reference_data)))
        padded_test = np.pad(np.abs(tested_data), (0, max_length - len(tested_data)))
        correlation = np.correlate(padded_ref, padded_test, mode="full")
        max_pos_samples = np.argmax(correlation) - max_length + 1
        corr = max(correlation)
        if len(reference_data) > len(tested_data):
            max_pos_samples = -1 * max_pos_samples

        return max_pos_samples, corr

    @staticmethod
    def align_signal_on_reference(
        tested_signal: AudioSignal,
        reference_signal: AudioSignal,
        start_time_ms: int = 0,
        alignment_interval_ms: [int, int] = None,
    ) -> None:
        """Align a tested signal on reference signal on the maximal correlation.

        Several shifts between tested audio and reference audio data are computed. The shift to apply on the tested
        signal is the one that gives the highest correlation.
        If the argument alignment_interval_ms is provided, the correlation is searched on this interval only. The
        purpose is to reduce the execution time by computing the correlation on few samples. To be relevant, the
        interval must be chosen on audio parts containing talk, not silence only.
        Once the shift is found, the aligned signals are created in reference_data.aligned_data and
        tested_signal.aligned_data, from sample start_time_ms. The signal self.tested_signal.aligned_data is created
        by shifting the data on left or right. Some 0 are added at start if needed.
        :param tested_signal: tested audio signal.
        :type tested_signal: AudioSignal
        :param reference_signal: audio signal taken as reference.
        :type reference_signal: AudioSignal
        :param start_time_ms: optional, start of the audio to keep in aligned_data, in ms. Default is 0.
        :type start_time_ms: int
        :param alignment_interval_ms: optional, begin and end of time interval used to find the maximal correlation, in
            ms. If empty, the whole tested_signal is taken. Default is None.
        :type alignment_interval_ms: [int, int]
        """
        start_align_sample = 0
        stop_align_sample = tested_signal.data.size
        if alignment_interval_ms is not None:
            start_align_sample = int(alignment_interval_ms[0] * 0.001 / tested_signal.sample_duration_s)
            stop_align_sample = min(
                stop_align_sample, int(alignment_interval_ms[1] * 0.001 / tested_signal.sample_duration_s)
            )
            print(
                f"try to align between samples [{start_align_sample}-{stop_align_sample}] \
                ({alignment_interval_ms[0]}-{alignment_interval_ms[1]} ms)"
            )
        else:
            print("try to align whole signal")

        if reference_signal.data is not None and tested_signal.data is not None:
            max_pos_samples, corr = AudioComparison.compute_correlation(
                tested_signal.data[start_align_sample:stop_align_sample],
                reference_signal.data[start_align_sample:stop_align_sample],
            )
            print(
                f"maximum correlation found at {max_pos_samples} samples \
{tested_signal.sample_duration_s * max_pos_samples * 1000:1.0f} ms with value {corr:1.1f}"
            )

            start_sample = int(start_time_ms * 0.001 / tested_signal.sample_duration_s)
            reference_signal.aligned_data = reference_signal.data[start_sample:]

            if max_pos_samples is not None:
                if max_pos_samples < 0:
                    tested_signal.aligned_data = tested_signal.data[start_sample - max_pos_samples :]
                elif max_pos_samples > 0:
                    tested_signal.aligned_data = np.concatenate((np.zeros((max_pos_samples)), tested_signal.data))
                    tested_signal.aligned_data = tested_signal.data[start_sample:]
                else:
                    tested_signal.aligned_data = tested_signal.data[start_sample:]

    def truncate_reference(self) -> bool:
        """Truncate reference data if it is too long.

        If the audio sizes are too different, the reference audio is shortened in order to reduce computation time
        for correlation. The beginning of the signal is kept, assuming that not all audio has been played during the
        mediastreamer2 test.
        :return: True if the reference signal is shortened, False otherwise.
        :rtype: bool
        """
        is_truncated = False
        tested_signal_size = self.tested_signal.data.size
        k = 1.1
        if self.reference_signal.data.size > k * self.tested_signal.data.size:
            print(f"speech size is {self.reference_signal.data.size}, too much compared with {tested_signal_size}")
            new_size = int(k * tested_signal_size)
            self.reference_signal.data = self.reference_signal.data[:new_size]
            is_truncated = True
        else:
            print(f"audio reference size is {self.reference_signal.data.size}, OK compared with {tested_signal_size}")

        return is_truncated

    def get_silence_mask_from_reference(self) -> None:
        """Compute the mask for silence in reference signal.

        Compute a mask of the silent samples in normalized aligned reference signal as a numpy array of boolean. The
        mask is set in self.silence_mask. The thresholding parameters depend on the sample rate.
        """
        # threshold
        silence_mask = np.zeros_like(self.reference_signal.normalized_aligned_data, dtype=bool)
        if self.reference_signal.sample_rate_hz == 48000:  # noqa: PLR2004, do not use a constant here
            hw = 400
            th = 0.001
        else:
            hw = 200
            th = 0.001
        for i in range(self.reference_signal.normalized_aligned_data.size):
            w0 = max(0, i - hw)
            wn = min(self.reference_signal.normalized_aligned_data.size, i + hw + 1)
            if np.mean(np.abs(self.reference_signal.normalized_aligned_data[w0:wn])) < th:
                silence_mask[i] = True

        # median filter on mask
        talk_hw = 1400
        silence_tmp = silence_mask.copy()
        for i in range(self.reference_signal.normalized_aligned_data.size):
            w0 = max(0, i - talk_hw)
            wn = min(self.reference_signal.normalized_aligned_data.size, i + talk_hw + 1)
            silence_tmp[i] = np.median(silence_mask[w0:wn])

        self.silence_mask = silence_tmp

    def detect_silence(self, audio: AudioSignal = None) -> None:
        """Detect silence in the reference signal to generate a mask.

        The silence is detected on the aligned and normalized reference data. The silence mask that is generated is
        applied to split the reference signal, the tested signal, and the optional audio (if provided) into distinct
        silence and speech arrays.
        :param audio: optional, other audio signal that will be masked. Default is None.
        :type audio: AudioSignal
        """
        if self.reference_signal.data is None:
            self.silence_mask = np.ones_like(self.tested_signal.data[self.start_sample :], dtype=bool)
            self.tested_signal.silence = self.tested_signal.data[self.start_sample :]
            if audio is not None:
                self.audio.silence = self.audio.data[self.start_sample :]
            return

        # detect silence
        self.reference_signal.normalize_aligned()
        self.get_silence_mask_from_reference()

        # apply mask
        masked_audio, not_masked_audio = self.apply_mask(
            self.silence_mask,
            [
                self.reference_signal.aligned_data,
                self.tested_signal.aligned_data,
                audio.aligned_data if audio is not None else None,
            ],
        )
        self.reference_signal.silence = masked_audio[0]
        self.reference_signal.talk = not_masked_audio[0]
        self.tested_signal.silence = masked_audio[1]
        self.tested_signal.talk = not_masked_audio[1]
        if audio is not None:
            audio.silence = masked_audio[2]
            audio.talk = not_masked_audio[2]

    @staticmethod
    def apply_mask(
        mask: NDArray[np.bool_], audio_list: [NDArray[np.float64]]
    ) -> tuple[[NDArray[np.float64]], [NDArray[np.float64]]]:
        """Apply a mask on a list of audio signals.

        Returns the list of audio generated from the mask, and the list of audio generated from the logical not of the
        mask. The mask and the audio arrays must have the same size.
        :param mask: mask to apply on arrays.
        :type mask: NDArray[np.bool_]
        :param audio_list: list of arrays to mask.
        :type audio_list: [NDArray[np.float64]]
        :return: a tuple of lists of masked arrays and of arrays masked with the logical not of the mask.
        :rtype: tuple[[NDArray[np.float64]], [NDArray[np.float64]]]
        """
        masked_audio = []
        not_masked_audio = []

        for audio in audio_list:
            if audio is not None:
                masked_audio.append(audio[mask])
                not_masked_audio.append(audio[np.logical_not(mask)])
            else:
                masked_audio.append(None)
                not_masked_audio.append(None)

        return masked_audio, not_masked_audio

    def plot_silence_and_talk(
        self, fig_title: str, legend: list[str], additional_audio: "AudioSignal" = None
    ) -> go.Figure:
        """Plot the audios split on silence and talk parts, after applying the silence mask.

        Plots and returns the reference signal, the tested signal and audio (if provided) with the silence and the
        talk parts, defined by the silence mask. Creates an interactive Plotly figure.
        :param fig_title: Title of the figure.
        :type fig_title: str
        :param legend: List of the plot names, sorted this way: 1- reference signa, 2- tested signal, 3- additional
            audio (if provided).
        :type legend: list of str
        :param additional_audio: Optional, another audio signal to plot.
        :type additional_audio: AudioSignal
        :return: Plotly figure with the audio signals split into silence and talk parts, after the silence mask has
            been applied.
        :rtype: go.Figure
        """
        colors = ["#636EFA", "#00CC96", "#b00b69", "#EF553B"]
        sig_max = max(np.max(self.reference_signal.aligned_data), np.max(self.tested_signal.aligned_data))
        signal_type = ["aligned", "silence", "talk"]
        audio_to_plot = {
            legend[0]: self.reference_signal,
            legend[1]: self.tested_signal,
        }
        if additional_audio is not None:
            sig_max = max(sig_max, np.max(additional_audio.aligned_data))
            audio_to_plot[legend[2]] = additional_audio
        n_type = len(signal_type)
        n_audio = len(audio_to_plot.keys())
        fig = make_subplots(rows=n_audio * n_type, cols=1, shared_xaxes=True)

        for i, st in enumerate(signal_type):
            for j, sn in enumerate(audio_to_plot.keys()):
                audio_all = audio_to_plot[sn]
                audio = None
                if st == "aligned":
                    audio = audio_all.aligned_data
                elif st == "silence":
                    audio = audio_all.silence
                elif st == "talk":
                    audio = audio_all.talk

                show_legend = True
                if i > 0:
                    show_legend = False

                if audio is not None:
                    signal = audio / sig_max

                    timestamp = self.tested_signal.sample_duration_s * np.arange(signal.size)
                    fig.add_trace(
                        go.Scatter(
                            x=timestamp,
                            y=signal,
                            mode="lines",
                            line={"width": 1, "color": colors[j]},
                            name=f"{sn}",
                            showlegend=show_legend,
                        ),
                        row=i * n_audio + j + 1,
                        col=1,
                    )
                    if st == "aligned":
                        show_mask_legend = True
                        if j > 0:
                            show_mask_legend = False
                        fig.add_trace(
                            go.Scatter(
                                x=timestamp,
                                y=0.7 * self.silence_mask,
                                mode="lines",
                                line={"width": 2, "color": colors[3]},
                                name="mask",
                                showlegend=show_mask_legend,
                            ),
                            row=i * n_audio + j + 1,
                            col=1,
                        )
                fig.update_yaxes(
                    title_text=f"{st}",
                    range=[-1.0, 1.0],
                    row=i * n_audio + j + 1,
                    col=1,
                )
        fig.update_layout(title=fig_title)
        fig.update_xaxes(title_text="Time")
        fig.show()

        return fig

    def compute_energy_difference(self, additional_audio: AudioSignal = None) -> tuple[float, float]:
        """Measure the energy in silence and return the difference with reference.

         Measure the energies of the silence parts of reference audio, tested audio and of additional audio (if
        provided). Compute and return the difference with reference.
        :param additional_audio: Optional, silence parts of another audio to compare with reference. Default is None.
        :type additional_audio: AudioSignal
        :return: The differences of energies in silence. The first one is between tested signal and reference, the
            second one is between additional audio if provided and reference, otherwise is 0.0.
        :rtype: tuple[float, float]
        """
        print("energy measured on silence:")
        energy_in_silence = np.sum(np.abs(self.tested_signal.silence))
        print(f"\tE_tested              :\t\t{energy_in_silence:1.1f}")
        if self.reference_signal.silence is not None:
            energy_in_silence = np.sum(np.abs(self.tested_signal.silence)) - np.sum(
                np.abs(self.reference_signal.silence)
            )
            print(f"\tE_tested - E_reference:\t\t{energy_in_silence:1.1f}")

        energy_in_silence_additional_audio = 0.0
        if additional_audio is not None:
            if self.reference_signal.silence is None:
                energy_in_silence_additional_audio = np.sum(np.abs(additional_audio.silence))
            else:
                energy_in_silence_additional_audio = np.sum(np.abs(additional_audio.silence)) - np.sum(
                    np.abs(self.reference_signal.silence)
                )
            print(f"\tE_audio  - E_reference:\t\t{energy_in_silence_additional_audio:1.1f}")

        return energy_in_silence, energy_in_silence_additional_audio

    def compute_acoustic_similarity(self, file_name_base: str) -> tuple[float, float]:
        """Compute acoustic similarity on talk based on MFCC coefficients.

        A similarity metric is computed from the MFCC to measure the similarity between talk parts in tested and
        reference audios. The inverse of the metric is also returned to measure the distance between both MFCC
        representations.
        The MFCC representations are written in `.npy` binary files. They are also plotted using matplotlib and saved as
         a `.png` image.
        :param file_name_base: Base name for the output files (e.g., "output").
            The function will generate:
            - Two `.npy` files for MFCC representations.
            - One `.png` file for the MFCC plot.
        :type file_name_base: str
        :return: A tuple containing:
            - The similarity metric (float).
            - The distance metric (float, inverse of similarity).
        :rtype: tuple[float, float]
        :note: The MFCC features are computed using `librosa.feature.mfcc`.
            See: https://librosa.org/doc/0.10.2/generated/librosa.feature.mfcc.html#librosa.feature.mfcc
        """
        ref_mfcc = MFCCAnalysis(self.reference_signal.talk, self.reference_signal.sample_rate_hz)
        test_mfcc = MFCCAnalysis(self.tested_signal.talk, self.tested_signal.sample_rate_hz)
        ref_mfcc.compute_mfcc()
        test_mfcc.compute_mfcc()
        test_mfcc.write_mfcc(file_name_base + "_test_MFCC_on_talk.npy")
        ref_mfcc.write_mfcc(file_name_base + "_ref_MFCC_on_talk.npy")
        self.similarity_mfcc, self.distance_mfcc = test_mfcc.compute_similarity_with_reference(ref_mfcc.mfcc)
        fig = test_mfcc.plot()
        fig.savefig(file_name_base + "_MFCC_comparison_on_talk.png")
        test_mfcc.write_difference_with_ref(file_name_base + "_MFCC_difference_with_ref_on_talk.npy")

        return self.similarity_mfcc, self.distance_mfcc
