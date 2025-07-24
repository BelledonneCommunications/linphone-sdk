import numpy as np
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import matplotlib.pyplot as plt
import librosa

class mfcc_analysis:
    """
    This class handles the MFCC representation of an audio signal and its usage to compare the audio with a reference
    one.
    See https://librosa.org/doc/0.10.2/generated/librosa.feature.mfcc.html#librosa.feature.mfcc
    """

    def __init__(self, audio_data, sample_rate_hz):
        """
        Initialization of the class.
        :param audio_data: numpy array of 1D audio data
        """

        self.audio_data = audio_data
        self.sample_rate_hz = sample_rate_hz
        self.mfcc = None
        self.ref = None
        self.diff = None
        self.distance = 0.
        self.similarity = 0.

    def compute_mfcc(self):
        """
        Compute the MFCC of the audio signal.
        """

        self.mfcc = librosa.feature.mfcc(y=self.audio_data, sr=self.sample_rate_hz)

    def compute_similarity_with_reference(self, ref_mfcc):
        """
        Compute a similarity criterion to compare teh current MFCC representation with the one of a reference.
        The criterion is computed as follow:
        - compute the difference between the reference and the current coefficients
        - for each time interval, compute the Frobenius norm of the vector of differences between the MFCC coeffcients
        - compute the mean of ths resulting vector

        The MFCC of the reference must have the same size than the current MFCC.
        TODO: test with DTW instead of mean.
        """

        self.ref = ref_mfcc
        print(f"reference: {self.ref.shape[0]} coefs, {self.ref.shape[1]} samples")
        print(f"signal:    {self.mfcc.shape[0]} coefs, {self.ref.shape[1]} samples")
        if self.mfcc.shape[0] != self.ref.shape[0] or self.mfcc.shape[1] != self.ref.shape[1]:
            print("Cannot compare MFCC coefficients because the dimensions do not fit")
            return

        self.diff = np.zeros((self.mfcc.shape[1]))
        for i in range(self.mfcc.shape[1]):
            self.diff[i] = np.linalg.norm(self.ref[:, i] - self.mfcc[:, i])

        self.distance = np.mean(self.diff)
        if self.distance == 0:
            self.similarity = 1.
        else:
            self.similarity = 1. / self.distance

        return self.similarity, self.distance


    def write_difference_with_ref(self, filename):
        """
        Write the difference with reference in binary file.
        """

        np.save(filename, self.diff)

    def read_difference_with_ref(self, filename):
        """
        Read the difference with reference from binary file.
        """

        print(f"read file {filename}")
        self.diff = np.load(filename)

    def write_mfcc(self, filename):
        """
        Write the MFCC representation in binary file.
        """

        np.save(filename, self.mfcc)


    def read_mfcc(self, filename):
        """
        Read the MFCC representation from binary file.
        """

        self.mfcc = np.load(filename)


    def plot(self):
        """
        Plot the MFCC. If a reference has been given, plot it also, with the similarity results.
        :return: matplotlib figure
        """

        colors = plt.get_cmap("jet")
        x_col = np.linspace(0, 1, self.mfcc.shape[0])
        diff_lim = [-200, 200]

        if self.ref is None:
            fig, ax = plt.subplots(nrows=2, sharex=True)
            img = librosa.display.specshow(librosa.power_to_db(self.mfcc, ref=np.max),
                                           x_axis='time', y_axis='mel', fmax=8000,
                                           ax=ax[0])
            fig.colorbar(img, ax=[ax[0]])
            ax[0].set(title='Mel spectrogram')
            ax[0].label_outer()
            img = librosa.display.specshow(self.mfcc, x_axis='time', ax=ax[1])
            fig.colorbar(img, ax=[ax[1]])
            ax[1].set(title='MFCC')

            plt.show()

        else:
            n_rows = 4
            fig, ax = plt.subplots(nrows=n_rows, ncols=1, sharex=True, figsize=(20, 15))

            # Set a common color scale for mfcc
            v_min = np.min([self.ref.min(), self.mfcc.min()])
            v_max = np.max([self.ref.max(), self.mfcc.max()])

            librosa.display.specshow(self.ref, ax=ax[0], vmin=v_min, vmax=v_max)
            ax[0].set_title('MFCC, reference')
            ax[0].set_xlabel('Time')
            ax[0].set_ylabel('Frequency range')

            librosa.display.specshow(self.mfcc, ax=ax[1], vmin=v_min, vmax=v_max)
            ax[1].set_title('MFCC, test')
            ax[1].set_xlabel('Time')
            ax[1].set_ylabel('Frequency range')

            librosa.display.specshow(self.ref - self.mfcc, ax=ax[2], vmin=diff_lim[0], vmax=diff_lim[1])
            ax[2].set_title('Diff')
            ax[2].set_xlabel('Time')
            ax[2].set_ylabel('Frequency range')

            plt.subplot(n_rows, 1, 4)
            for c in range(self.ref.shape[0]):
                plt.plot(abs(self.ref[c, :] - self.mfcc[c, :]), label=f"distance coef {c}", color=colors(x_col[c]))
            plt.plot(self.diff, label=f"instant distance", color="red", linewidth=3)
            plt.ylim(0, diff_lim[1])

        # Adjust layout and display the plot
        plt.tight_layout()
        plt.close()

        return fig