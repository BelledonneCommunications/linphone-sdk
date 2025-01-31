import numpy as np
import librosa
import soundfile as sf


class AudioSignal:
    """
    Class to handle audio signal read from wav file in format PCM_16.
    """

    def __init__(self):

        self.file_name = ""
        self.data = None
        self.normalized_data = None
        self.silence = None
        self.talk = None
        self.aligned_data = None
        self.normalized_aligned_data = None
        self.timestamps = None
        self.sample_rate_hz = 16000

    def normalize(self):
        """
        Compute the normalized audio from the initial data.
        """
        """Normalize the audio data."""

        self.normalized_data = self.data / np.max(np.abs(self.data))

    def normalize_aligned(self):
        """
        Compute the normalized audio from the aligned data.
        """

        if self.aligned_data is not None:
            self.normalized_aligned_data = self.aligned_data / np.max(np.abs(self.aligned_data))

    def read_audio(self, sample_rate_hz=16000):
        """
        Read the initial audio data from the wav file. Fill the data array, compute the normalized audio and fill the
        timestamps.
        :param sample_rate_hz: sample rate in Hz, default is 16000 Hz.
        """

        if self.file_name == "":
            return None

        self.data, sample_rate_read = librosa.load(self.file_name,
                                                   sr=sample_rate_hz)
        if sample_rate_hz != sample_rate_read:
            print(f"ERROR sampling rate {sample_rate_hz} != {sample_rate_read} Hz")
            return None
        self.sample_rate_hz = sample_rate_read
        print(f"read file {self.file_name}")
        print(
            f"  -> data size is {self.data.size}, max is {np.max(self.data):1.0f}, sampling rate is {sample_rate_read}")

        self.normalize()

        sample_duration_s = 1. / self.sample_rate_hz
        self.timestamps = sample_duration_s * np.arange(self.data.size)

    def read_audio_from_file(self, file_path, sample_rate_hz=16000):
        """
        Read the audio data from a given wav file.
        :param file_path: name of the wav file.
        :param sample_rate_hz: sample rate in Hz, default is 16000 Hz.
        """

        if file_path == "":
            return None

        self.file_name = file_path
        self.read_audio(sample_rate_hz)

    def write_in_file(self, file_name):
        """
        Write the audio data in wav file.
        :param file_name: name of the file.
        """
        """
        Write audio data in wav file.
        :param file_name: name of the file
        :return:
        """
        if self.data is not None:
            sf.write(file_name, self.data, self.sample_rate_hz, subtype='PCM_16')
