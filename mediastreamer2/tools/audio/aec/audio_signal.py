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
        self.sample_duration_s = 1./16000.
        self.total_duration_s = 0.

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

    def read_audio(self):
        """
        Read the initial audio data from the wav file. Fill the data array, compute the normalized audio and fill the
        timestamps.
        """

        if self.file_name == "":
            return None

        self.data, sample_rate_read = librosa.load(self.file_name,
                                                   sr=self.sample_rate_hz)
        if self.sample_rate_hz != sample_rate_read:
            print(f"ERROR sampling rate {self.sample_rate_hz} != {sample_rate_read} Hz")
            return None
        self.sample_duration_s = 1./float(self.sample_rate_hz)
        self.total_duration_s = self.data.size * self.sample_duration_s
        print(f"read file {self.file_name}")
        print(
            f"  -> data size is {self.data.size}, max is {np.max(self.data):1.0f}, sampling rate is {sample_rate_read} -> {self.total_duration_s:.2f} s")

        self.normalize()

        self.sample_duration_s = 1. / self.sample_rate_hz
        self.timestamps = self.sample_duration_s * np.arange(self.data.size)

    def read_audio_from_file(self, file_path):
        """
        Read the audio data from a given wav file.
        :param file_path: name of the wav file.
        """

        if file_path == "":
            return None

        self.file_name = file_path
        self.read_audio()

    def write_in_file(self, file_name):
        """
        Write the audio data in wav file.
        :param file_name: name of the file.
        """

        if self.data is not None:
            sf.write(file_name, self.data, self.sample_rate_hz, subtype='PCM_16')

    def write_aligned_in_file(self, file_name):
        """
        Write the aligned audio data in wav file.
        :param file_name: name of the file.
        """

        if self.aligned_data is not None:
            sf.write(file_name, self.aligned_data, self.sample_rate_hz, subtype='PCM_16')

    def write_data_in_file(self, file_name, data):
        """
        Write the given audio data in wav file.
        :param file_name: name of the file.
        :param data: array of audio data
        """

        if data is not None:
            sf.write(file_name, data, self.sample_rate_hz, subtype='PCM_16')
