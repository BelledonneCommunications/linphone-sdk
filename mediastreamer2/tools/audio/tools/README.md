[![Ruff](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/ruff/main/assets/badge/v2.json)](https://github.com/astral-sh/ruff)
[![uv](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/astral-sh/uv/main/assets/badge/v0.json)](https://github.com/astral-sh/uv)
![Python Version](https://img.shields.io/badge/python-3.12+-blue.svg)
[![License: AGPL v3 or later](https://img.shields.io/badge/License-AGPL_v3_or_later-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

# Tools

The Tools project is developed for several purposes: analysis of log file, complete study on a given topic, like AEC or the noise suppression, or simple useful scripts. It is managed with [uv](https://github.com/astral-sh/uv/tree/main).

Use [ruff](https://github.com/astral-sh/ruff) to format or check python code:
```commandline
uv run ruff format
uv run ruff check
```
Some issues detected by ruff checker have not been resolved yet. The ignored rules are defined in `pyproject.toml`.

Warning: there are no unit tests to validate the classes and methods.

## AEC

This app runs the tests of the AEC3 suite of mediastreamer2 and analyses the quality of the Acoustic Echo Cancellation feature in Linphone (MSWebRTCAEC3 filter in mediastreamer2). 
After a test has run, the log file is parsed to get the quality criteria computed by the test. The output audio file is read and other metrics are computed, like the MFCC.

Don't forget to enable the dump of files in mediastreamer2/tester/mediastreamer2_aec3_tester.c :
```
#define EC_DUMP 1
```

To get more instant measurements of AEC metrics during talk, change or comment the time occurrence in mswebrtc/mswebrtc_aec3.cc :
```
if ((filter->ticker->time % 5000) == 0) {
    ms_message("WebRTCAEC[%p] AEC3 current metrics: delay = %d ms, ERL = %f, ERLE = %f", this,
               aecMetrics.delay_ms, aecMetrics.echo_return_loss, aecMetrics.echo_return_loss_enhancement);
}
```

The log file and the filtered audio are saved in tools/audio_files/aec. The results of the analysis (data read in log and MFCC measurement) are written in metrics.csv.

To run the `main.py` script that execute the tests and analyse their results, do
```commandline
cd path/to/tools/
uv run aec --build-path path/to/build --linphone-path path/to/linphone
```

To choose the tests to run, the number of time or the analysis of the results, the `main` function can be edited, or you 
can use the CLI, build with [typer](https://typer.tiangolo.com/).
Get the list of options with:
```
uv run aec --help
```
that returns
```commandline
╭─ Options ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
│ *  --build-path                             PATH     Path to the build directory. [required]                                                                                               │
│ *  --linphone-path                          PATH     Path to the linphone-sdk directory. [required]                                                                                        │
│    --test                                   TEXT     Name of the test to run. Use `all` to run all tests.                                                                                  │
│    --repeat                                 INTEGER  Number of times the test is run. [default: 1]                                                                                         │
│    --run-test              --no-run-test             Run linphone-sdk tests. [default: run-test]                                                                                           │
│    --fig                   --no-fig                  Display plotly figures. [default: fig]                                                                                                │
│    --analysis              --no-analysis             Do the analysis for audio quality. [default: no-analysis]                                                                             │
│    --install-completion                              Install completion for the current shell.                                                                                             │
│    --show-completion                                 Show completion for the current shell, to copy it or customize the installation.                                                      │
│    --help                                            Show this message and exit.                                                                                                           │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
```
For example to run only the test "simple talk" twice, without analysis:
```commandline
uv run aec --build-path path/to/build --linphone-path path/to/linphone --test "simple talk" --repeat 2

```
To run all tests:
```commandline
uv run aec --build-path path/to/build --linphone-path path/to/linphone --test "all"
```
To run only the tests that are listed in `main` function:
```commandline
uv run aec --build-path path/to/build --linphone-path path/to/linphone
```
To analyse the results of these tests, without running the suite, and to display the figures:
```commandline
uv run aec --build-path path/to/build --linphone-path path/to/linphone --no-run-test --analysis
```
Note: the MFCC figures are not displayed but are saved directly in output directory.

## Noise Suppression

This app runs the tests of the Noise Suppression suite of mediastreamer2 and analyses the quality of the denoising. After a test has run, the log file is parsed to get the quality criteria computed by the test. The output audio file is read and other metrics are computed, like the MFCC.

Don't forget to enable the dump of files in mediastreamer2/tester/mediastreamer2_noise_suppression_tester.c :
```
#define NS_DUMP 1
```

The log file and the filtered audio are saved in tools/audio_files/noise_suppression. The results of the analysis (data read in log and MFCC measurement) are written in metrics.csv.

To run the `main.py` script that execute the tests and analyse their results, do
```commandline
cd path/to/tools/
uv run noise-suppression --build-path path/to/build --linphone-path path/to/linphone
```

To choose the tests to run, the number of time or the analysis of the results, the `main` function can be edited, or you 
can use the CLI, build with [typer](https://typer.tiangolo.com/).
Get the list of options with:
```
uv run noise-suppression --help
```
that returns
```commandline
╭─ Options ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
│ *  --build-path                             PATH     Path to the build directory. [required]                                                                                               │
│ *  --linphone-path                          PATH     Path to the linphone-sdk directory. [required]                                                                                        │
│    --test                                   TEXT     Name of the test to run. Use `all` to run all tests.                                                                                  │
│    --repeat                                 INTEGER  Number of times the test is run. [default: 1]                                                                                         │
│    --run-test              --no-run-test             Run linphone-sdk tests. [default: run-test]                                                                                           │
│    --fig                   --no-fig                  Display plotly figures. [default: fig]                                                                                                │
│    --analysis              --no-analysis             Do the analysis for audio quality. [default: no-analysis]                                                                             │
│    --install-completion                              Install completion for the current shell.                                                                                             │
│    --show-completion                                 Show completion for the current shell, to copy it or customize the installation.                                                      │
│    --help                                            Show this message and exit.                                                                                                           │
╰────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
```
Note: if `--build-path` or `--linphone-path` are not given, the default paths defined in main.py are used.

For example to run only the test "talk with noise snr 6dB" twice, without analysis:
```commandline
uv run noise-suppression --build-path path/to/build --linphone-path path/to/linphone --test "talk with noise snr 6dB" --repeat 2
```
To run all tests (where noise suppression is enabled):
```commandline
uv run noise-suppression --build-path path/to/build --linphone-path path/to/linphone --test "all"
```
To run only the tests that are listed in `main` function:
```commandline
uv run noise-suppression --build-path path/to/build --linphone-path path/to/linphone
```
To analyse the results of these tests, without running the suite, and to display the figures:
```commandline
uv run noise-suppression --build-path path/to/build --linphone-path path/to/linphone --no-run-test --analysis
```

## Common

Common functions, for example for audio processing.

For each project that needs `common`, the dependency is set in `pyproject.toml` file with a relative path
```
[tool.poetry.dependencies]
common = { path = "../common", develop = true }
```

## Licence

Copyright © Belledonne Communications

The software products developed in the context of the Linphone project are dual licensed, and are available either :

 - under a [GNU/AGPLv3 license](https://www.gnu.org/licenses/agpl-3.0.html), for free (open source). Please make sure that you understand and agree with the terms of this license before using it (see LICENSE.txt file for details). AGPLv3 is chosen over GPLv3 because linphone-sdk can be used to create server-side applications, not just client-side ones. Any product incorporating linphone-sdk to provide a remote service then has to be licensed under AGPLv3.
 For a client-side product, the "remote interaction" clause of AGPLv3 being irrelevant, the usual terms GPLv3 terms do apply (the two licences differ by this only clause).

 - under a proprietary license, for a fee, to be used in closed source applications. Contact [Belledonne Communications](https://www.linphone.org/contact) for any question about costs and services.
