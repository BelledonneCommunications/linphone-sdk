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

"""Compute the signal-to-noise ratio."""

import librosa
import numpy as np
from numpy.typing import NDArray


def compute_signal_to_noise_ratio_in_db_from_files(
    signal_file: str, noise_file: str, time_interval_ms: [int, int] = None, noise_gain: float = 1.0
) -> float:
    """Compute the signal-to-noise ratio from two audio files.

    Return the signal-to-noise ratio in dB from the audios read in signal_file and noise_file, between the time stamps
    in ms given in time_interval, if provided. Otherwise, the size of the shortest audio is taken. Both audio must have
    the same sampling rate. The gain noise_gain is applied to the noise. The audio are read with librosa.
    :param signal_file: wav file containing the signal.
    :type signal_file: str
    :param noise_file: wav file containing the noise.
    :type noise_file: str
    :param time_interval_ms: optional, list of two time stamps in ms. The SNR is computed between those values. Default
        is None.
    :type time_interval_ms: [int, int]
    :param noise_gain: optional, gain to apply on noise. Default is 1.0.
    :type noise_gain: float
    :return: the SNR in dB.
    :rtype: float
    """
    signal, sampling_rate_hz = librosa.load(signal_file, sr=None)
    noise, sampling_rate_noise_hz = librosa.load(noise_file, sr=None)
    if sampling_rate_hz != sampling_rate_noise_hz:
        print(f"ERROR sampling rate {sampling_rate_hz} != {sampling_rate_noise_hz} Hz")
        return None

    return compute_signal_to_noise_ratio_in_db(signal, noise, sampling_rate_hz, time_interval_ms, noise_gain)


def compute_signal_to_noise_ratio_in_db(
    signal: NDArray[np.float32],
    noise: NDArray[np.float32],
    sampling_rate_hz: int = 48000,
    time_interval_ms: [int, int] = None,
    noise_gain: float = 1.0,
) -> float:
    """Return the signal-to-noise ratio in dB between the audio vectors signal and noise.

    The ratio is computed between the time stamps given in time_interval_ms in ms, if provided. Otherwise, the size of
    the shortest audio is taken. The gain noise_gain is applied to the noise.

    :param signal: 1D numpy array containing the signal.
    :type signal: NDArray[np.float32]
    :param noise: 1D numpy array containing the noise.
    :type noise: NDArray[np.float32]
    :param time_interval_ms: optional, list of two time stamps in ms. The SNR is computed between those values. Default
        is None.
    :type time_interval_ms: [int, int]
    :param noise_gain: optional, gain to apply on noise. Default is 1.0.
    :type noise_gain: float
    :return: the SNR in dB.
    :rtype: float
    """
    start_sample = 0
    stop_sample = min(noise.size, signal.size)
    if time_interval_ms:
        start_sample = time_interval_ms[0] * sampling_rate_hz
        stop_sample = time_interval_ms[1] * sampling_rate_hz

    if noise_gain != 1.0:
        print(f"gain applied for noise = {noise_gain}")
    noise_with_gain = noise_gain * noise
    if noise_with_gain.size < signal.size:
        signal = signal[start_sample:stop_sample]
    else:
        noise_with_gain = noise_with_gain[start_sample:stop_sample]
    print(f"noise size = {noise_with_gain.size}")
    print(f"signal size = {signal.size}")

    speech_power = np.mean(signal**2)
    noise_power = np.mean(noise_with_gain**2)
    snr_db = 10.0 * np.log10(speech_power / noise_power)
    print(f"SNR = {snr_db:0.2f} dB")

    return snr_db
