Belcard is a C++ library to manipulate VCard standard format

Dependencies
------------
* bctoolbox (git://git.linhpone.org/bctoolbox.git)
* belr (git://git.linphone.org/belr.git)


Build instrucitons
------------------
cmake . -DCMAKE_INSTALL_PREFIX=<install_prefix> -DCMAKE_PREFIX_PATH=<search_prefix>

make
make install


Options
-------
CMAKE_INSTALL_PREFIX=<string>: installation prefix
CMAKE_PREFIX_PATH=<string>: prefix where depedencies are installed
ENABLE_UNIT_TESTS=<bool>: compile non-regression tests


Note for packagers
------------------
Our CMake scripts may automatically add some path into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use -DCMAKE_SKIP_INSTALL_RPATH=ON
while you invoke cmake.
