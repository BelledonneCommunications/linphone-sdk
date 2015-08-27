# LIBLINPHONE PYTHON EXTENSION MODULE #

## BUILD PREREQUISITES

The common prerequisites listed in the README.md file are requested to build
the liblinphone Python extension module.
You will also need to install some additional tools needed during the build 
process. On linux, you can generally get them from your distribution. On Mac OS
X, you can get them from MacPorts or Brew. On Windows you will need to download
and install them manually. Here are these tools:
- A C and C++ compiler. On Windows you will need Visual Studio 2008 as it is
  the version used by Python.
- Python 2.7.x, with 'pip' command. On windows, add c:\Python27\Scripts in the
  PATH.
- Java
- awk (automatically installed when building on Windows)
- sed (automatically installed when building on Windows)
- patch (automatically installed when building on Windows)
- yasm (automatically installed when building on Windows)
- xxd (only for Linux and Mac OS X)
- pkg-config (automatically installed when building on Windows)
- intltoolize (automatically installed when building on Windows)
- doxygen
- dot (graphviz)
- pystache (install with pip install pystache in terminal)
- wheel (install with pip install wheel in terminal)
- MinGw (only on Windows): Follow the instructions at
  http://mingw.org/wiki/Getting_Started

On windows, append the following to your PATH if not the case:
    C:\Program Files (x86)\GnuWin32\bin
    C:\MinGW\bin
    C:\Python27\Scripts
    C:\Program Files (x86)\Graphviz2.38

Once done, within the system's shell (unix terminal, windows cmd), place into
the root of linphone-cmake-builder project and run the following commands:

## BUILDING THE SDK ON LINUX AND MAC OS X

	$ ./prepare.py python
	$ make -C WORK/python/cmake

If everything is successful (and after a few minutes) you will find the Python
wheel package of the liblinphone Python extension module in the OUTPUT
directory.

## BUILDING THE SDK ON WINDOWS

Run the following command in Windows command prompt after having setup the
build prerequisites:

	> python prepare.py python

Then open the WORK\python\cmake\Project.sln Visual Studio solution and build
using the Release configuration.

If everything is successful (and after a few minutes) you will find the Python
wheel package and installers of the liblinphone Python extension module in the
OUTPUT directory.
