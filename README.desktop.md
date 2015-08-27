# LINPHONE DESKTOP (GTK2) #

## BUILD PREREQUISITES

The common prerequisites listed in the README.md file are requested to build
linphone for desktop.
You will also need to install some additional tools needed during the build 
process. On linux, you can generally get them from your distribution. On Mac OS
X, you can get them from MacPorts or Brew. On Windows you will need to download
and install them manually. Here are these tools:
- A C and C++ compiler. On Windows you will need any version of Visual Studio
beginning from the 2008 version.
- Python 2.7.x. On windows, add c:\Python27\Scripts in the PATH.
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
- MinGw (only on Windows): Follow the instructions at
  http://mingw.org/wiki/Getting_Started

On windows, append the following to your PATH if not the case:
    C:\Program Files (x86)\GnuWin32\bin
    C:\MinGW\bin
    C:\Python27\Scripts
    C:\Program Files (x86)\Graphviz2.38


## BUILDING ON LINUX AND MAC OS X

	$ ./prepare.py desktop
	$ make -C WORK/desktop/cmake

## BUILDING ON WINDOWS

	> python prepare.py desktop

Then open the WORK\desktop\cmake\Project.sln Visual Studio solution and build
using the Release configuration.

You can specify the Visual Studio version to be used to build, eg.:

	> python prepare.py desktop -G "Visual Studio 12 2013"
