# AEC tests sound files

The audio samples for AEC come from the ICASSP AEC Challenge, see https://github.com/microsoft/AEC-Challenge?tab=readme-ov-file.

For a more detailed description of the generation process, please refer to the [paper](https://arxiv.org/abs/2009.04972).

- `farend_speech_*.wav`: far end signals, some of these include background noise
- `echo_*.wav`: transformed version of far end speech, used as echo signals.
- `nearend_speech_*.wav`: clean near end signals that can be used as targets.

The `*simple_talk*` files have been modified in order to get a conversation between near-end and far-en speakers without double talk.

