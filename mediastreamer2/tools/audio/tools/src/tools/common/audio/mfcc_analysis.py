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

"""Analyze an audio signal from its MFCC representation.

The Mel-frequency cepstral coefficients (MFCCs) are used to represent the speech features from audio data. A similarly
criterion based on this representation is computed in order to compare a signal with a reference. The MFCC can be
plotted with the reference and the similarity measurement.
"""

import librosa
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.figure import Figure
from numpy.typing import NDArray


class MFCCAnalysis:
    """Handle the MFCC representation of an audio signal and comparison with a reference.

    See https://librosa.org/doc/0.11.0/generated/librosa.feature.mfcc.html
    """

    def __init__(self, audio_data: NDArray[np.float64], sample_rate_hz: int) -> None:
        """Initialize the MFCCAnalysis instance with audio data.

        :param audio_data: numpy array of 1D audio data.
        :type audio_data: NDArray[np.float64]
        :param sample_rate_hz: Sample rate in Hz.
        :type sample_rate_hz: int
        """
        self.audio_data = audio_data
        self.sample_rate_hz = sample_rate_hz
        self.mfcc = None
        self.ref = None
        self.diff = None
        self.distance = 0.0
        self.similarity = 0.0

    def compute_mfcc(self) -> None:
        """Compute the MFCC of the audio signal with librosa.

        The default parameters of librosa are used.
        """
        self.mfcc = librosa.feature.mfcc(y=self.audio_data, sr=self.sample_rate_hz)

    def compute_similarity_with_reference(self, ref_mfcc: NDArray[np.float64]) -> tuple[float, float]:
        """Compute a similarity criterion to compare the current MFCC representation with the one of a reference.

        Both MFCC arrays must have the same size.
        The criterion is computed as follows:
        - for each time interval, compute the Frobenius norm of the vector of differences between the MFCC coefficients
        - compute the mean of the resulting vector.
        :param ref_mfcc: The MFCC representation of the reference.
        :type ref_mfcc: NDArray[np.float64]
        :return: The similarity measurement and the distance computed from MFCC arrays.
        :rtype: tuple[float, float]
        """
        self.ref = ref_mfcc
        print(f"reference: {self.ref.shape[0]} coefs, {self.ref.shape[1]} samples")
        print(f"signal:    {self.mfcc.shape[0]} coefs, {self.ref.shape[1]} samples")
        if self.mfcc.shape[0] != self.ref.shape[0] or self.mfcc.shape[1] != self.ref.shape[1]:
            print("Cannot compare MFCC coefficients because the dimensions do not fit")
            return None

        self.diff = np.zeros(self.mfcc.shape[1])
        for i in range(self.mfcc.shape[1]):
            self.diff[i] = np.linalg.norm(self.ref[:, i] - self.mfcc[:, i])

        self.distance = np.mean(self.diff)
        if self.distance == 0:
            self.similarity = 1.0
        else:
            self.similarity = 1.0 / self.distance

        return self.similarity, self.distance

    def write_difference_with_ref(self, filename: str) -> None:
        """Write the difference with reference in binary file.

        :param filename: Name of the file.
        :type filename: str
        """
        np.save(filename, self.diff)

    def read_difference_with_ref(self, filename: str) -> None:
        """Read the difference with reference from binary file.

        :param filename: Name of the file.
        :type filename: str
        """
        print(f"read file {filename}")
        self.diff = np.load(filename)

    def write_mfcc(self, filename: str) -> None:
        """Write the MFCC representation in binary file.

        :param filename: Name of the file.
        :type filename: str
        """
        np.save(filename, self.mfcc)

    def read_mfcc(self, filename: str) -> None:
        """Read the MFCC representation from binary file.

        :param filename: Name of the file.
        :type filename: str
        """
        self.mfcc = np.load(filename)

    def plot(self) -> Figure:
        """Plot the MFCC representation.

        If a reference has been given, plot it too, with the difference between them, coefficient by coefficient, and
        with the distance.
        :return: A Matplotlib figure object. Use `fig.savefig()` to save it to a file.
        :rtype: Figure
        """
        colors = plt.get_cmap("jet")
        x_col = np.linspace(0, 1, self.mfcc.shape[0])
        diff_lim = [-200, 200]

        if self.ref is None:
            fig, ax = plt.subplots(nrows=2, sharex=True)
            img = librosa.display.specshow(
                librosa.power_to_db(self.mfcc, ref=np.max),
                x_axis="time",
                y_axis="mel",
                fmax=8000,
                ax=ax[0],
            )
            fig.colorbar(img, ax=[ax[0]])
            ax[0].set(title="Mel spectrogram")
            ax[0].label_outer()
            img = librosa.display.specshow(self.mfcc, x_axis="time", ax=ax[1])
            fig.colorbar(img, ax=[ax[1]])
            ax[1].set(title="MFCC")

            plt.show()

        else:
            n_rows = 4
            fig, ax = plt.subplots(nrows=n_rows, ncols=1, sharex=True, figsize=(20, 15))

            # Set a common color scale for mfcc
            v_min = np.min([self.ref.min(), self.mfcc.min()])
            v_max = np.max([self.ref.max(), self.mfcc.max()])

            librosa.display.specshow(self.ref, ax=ax[0], vmin=v_min, vmax=v_max)
            ax[0].set_title("MFCC, reference")
            ax[0].set_xlabel("Time")
            ax[0].set_ylabel("Frequency range")

            librosa.display.specshow(self.mfcc, ax=ax[1], vmin=v_min, vmax=v_max)
            ax[1].set_title("MFCC, test")
            ax[1].set_xlabel("Time")
            ax[1].set_ylabel("Frequency range")

            librosa.display.specshow(self.ref - self.mfcc, ax=ax[2], vmin=diff_lim[0], vmax=diff_lim[1])
            ax[2].set_title("Diff")
            ax[2].set_xlabel("Time")
            ax[2].set_ylabel("Frequency range")

            plt.subplot(n_rows, 1, 4)
            for c in range(self.ref.shape[0]):
                plt.plot(
                    abs(self.ref[c, :] - self.mfcc[c, :]),
                    label=f"distance coef {c}",
                    color=colors(x_col[c]),
                )
            plt.plot(self.diff, label="instant distance", color="red", linewidth=3)
            plt.ylim(0, diff_lim[1])

        # Adjust layout and display the plot
        plt.tight_layout()
        plt.close()

        return fig
