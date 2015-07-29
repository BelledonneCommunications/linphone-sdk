# Using the afl-fuzz to look for bugs in the belle-sip parser

This guide expects that you have installed the afl-fuzz package for your distribution, or on Mac using Homebrew or port.
Windows is not supported right now.

Then follow these steps:

1. Configure belle-sip with the afl instrumentation tools as CC and OBJC, and with static linking:

        # Linux
        CC=`which afl-gcc` ./configure --disable-shared

        # Mac
        CC=`which afl-clang` OBJC=`which afl-clang` ./configure --disable-shared

2. Compile and make sure the testers are build. You should have an executable file named testers/belle_sip_parser

        make clean && make

3. You can now run the afl fuzzy tester in the tester/ directory to test the parser for SDP, HTTP or SIP.

        afl-fuzz -i afl/sip -o afl_sip_results -- ./belle_sip_parse --protocol sip @@

With this command:

- It will show you a screen with informations on the current state of the fuzzing steps.  

- The `afl/sip` directory contains valid SIP messages that the fuzzer will use as a base for its investigations. You can add 

- The results of the investigations will be placed in a directory named `afl_sip_results/`. You will have access to the SIP messages that provoked a crash in the `crashes/` directory.

# Notes

The afl directory contains test messages that will be the base for mutation with the afl fuzzer. They are saved using the CRLF line endings. This is important since the parser expects two "\r\n\r\n" at the end of a message.

The hangs usually occur when the message passed to belle_sip_parse is not correctly formed, and the underlying implementation fails at some point. These are not false positives, they are actual problems!

## TODO:

1. add HTTP and SDP fuzzy tests