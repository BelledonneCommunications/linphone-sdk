Summary:  A unit testing framework for 'C'
Name:     CUnit
Version:  1.1
Release:  2
Source:   http://www.sourceforge.net/projects/cunit/CUnit-1.1-2.tar.gz
Group:    Development/Tools
License:  GPL
URL:      http://cunit.sourceforge.net
Packager: Jerry St. Clair <jds2@users.sourceforge.net>

%description
CUnit is a unit testing framework for C.
This package installs the CUnit static library,
headers, and documentation files.

%prep
echo "Preparing for Installation."
%setup -q -n CUnit-1.1-2

%build
echo "Preparing for Building."
./configure --prefix=%{_prefix} && \
make

%install
echo "Preparing for Make install."
make DESTDIR=$RPM_BUILD_ROOT install

%clean

%files
%defattr(-,root,root)

########### Include Files
%{_prefix}/include/CUnit/Automated.h
%{_prefix}/include/CUnit/Console.h
%{_prefix}/include/CUnit/Curses.h
%{_prefix}/include/CUnit/CUnit.h
%{_prefix}/include/CUnit/Errno.h
%{_prefix}/include/CUnit/TestDB.h
%{_prefix}/include/CUnit/TestRun.h

########## Library File
%{_prefix}/lib/libcunit.a

########## Manpage Files
%{_prefix}/man/man3/add_test_case.3.*
%{_prefix}/man/man3/add_test_group.3.*
%{_prefix}/man/man3/ASSERT.3.*
%{_prefix}/man/man3/automated_run_tests.3.*
%{_prefix}/man/man3/cleanup_registry.3.*
%{_prefix}/man/man3/console_run_tests.3.*
%{_prefix}/man/man3/curses_run_tests.3.*
%{_prefix}/man/man3/get_error.3.*
%{_prefix}/man/man3/get_registry.3.*
%{_prefix}/man/man3/initialize_registry.3.*
%{_prefix}/man/man3/set_output_filename.3.*
%{_prefix}/man/man3/set_registry.3.*
%{_prefix}/man/man8/CUnit.8.*

########## Share information and Example Files
%{_prefix}/share/CUnit-1.1-2/Example/Automated/README
%{_prefix}/share/CUnit-1.1-2/Example/Automated/AutomatedTest
%{_prefix}/share/CUnit-1.1-2/Example/Console/README
%{_prefix}/share/CUnit-1.1-2/Example/Console/ConsoleTest
%{_prefix}/share/CUnit-1.1-2/Example/Curses/README
%{_prefix}/share/CUnit-1.1-2/Example/Curses/CursesTest
%{_prefix}/share/CUnit-1.1-2/Example/Register/README
%{_prefix}/share/CUnit-1.1-2/Example/Register/RegisterTest
%{_prefix}/share/CUnit/CUnit-List.dtd
%{_prefix}/share/CUnit/CUnit-List.xsl
%{_prefix}/share/CUnit/CUnit-Run.dtd
%{_prefix}/share/CUnit/CUnit-Run.xsl
%{_prefix}/share/CUnit/Memory-Dump.dtd
%{_prefix}/share/CUnit/Memory-Dump.xsl

# Add the change log in ChangeLog file located under source home directory.
# The same file is used internally to populate the change log for the RPM creation.
