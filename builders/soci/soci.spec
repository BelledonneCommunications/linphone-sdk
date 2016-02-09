#
##
# Default values are --with empty --with sqlite3 --with mysql --with postgresql
#                    --with odbc --without oracle 
# Note that, for Oracle, when enabled, the following options should
# also be given:
# --with-oracle-include=/opt/oracle/app/oracle/product/11.1.0/db_1/rdbms/public
# --with-oracle-lib=/opt/oracle/app/oracle/product/11.1.0/db_1/lib
# If the macros are defined, redefine them with the correct compilation flags.
%bcond_without empty
%bcond_without sqlite3
%bcond_without mysql
%bcond_without postgresql
%bcond_without odbc
%bcond_with oracle
%bcond_with firebird

%define _default_oracle_dir /opt/oracle/app/oracle/product/11.1.0/db_1
%{!?_with_oracle_incdir: %define _with_oracle_incdir --with-oracle-include=%{_default_oracle_dir}/rdbms/public}
%{!?_with_oracle_libdir: %define _with_oracle_libdir --with-oracle-lib=%{_default_oracle_dir}/lib}
#
##
#
Name:           soci
Version:        3.2.3
Release:        1%{?dist}

Summary:        The database access library for C++ programmers

Group:          System Environment/Libraries
License:        Boost
URL:            http://%{name}.sourceforge.net
Source0:        http://downloads.sourceforge.net/%{name}/%{name}-%{version}.tar.gz
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Patch:		%{soci_patch}

BuildRequires:  cmake
BuildRequires:  boost-devel

%description
%{name} is a C++ database access library that provides the
illusion of embedding SQL in regular C++ code, staying entirely within
the C++ standard.


%{?with_sqlite3:%package        sqlite3
Summary:        SQLite3 back-end for %{name}
Group:          System Environment/Libraries
Requires:       %{name}%{?_isa} = %{version}-%{release}
BuildRequires:  sqlite-devel

%description    sqlite3
This package contains the SQLite3 back-end for %{name}, i.e.,
dynamic library specific to the SQLite3 database. If you would like to
use %{name} in your programs with SQLite3, you will need to
install %{name}-sqlite3.}

%{?with_mysql:%package        mysql
Summary:        MySQL back-end for %{name}
Group:          System Environment/Libraries
Requires:       %{name}%{?_isa} = %{version}-%{release}
BuildRequires:  mysql-devel

%description    mysql
This package contains the MySQL back-end for %{name}, i.e.,
dynamic library specific to the MySQL database. If you would like to
use %{name} in your programs with MySQL, you will need to
install %{name}-mysql.}

%{?with_postgresql:%package        postgresql
Summary:        PostGreSQL back-end for %{name}
Group:          System Environment/Libraries
Requires:       %{name}%{?_isa} = %{version}-%{release}
BuildRequires:  postgresql-devel

%description    postgresql
This package contains the PostGreSQL back-end for %{name}, i.e.,
dynamic library specific to the PostGreSQL database. If you would like
to use %{name} in your programs with PostGreSQL, you will need to
install %{name}-postgresql.}

%{?with_odbc:%package        odbc
Summary:        ODBC back-end for %{name}
Group:          System Environment/Libraries
Requires:       %{name}%{?_isa} = %{version}-%{release}
BuildRequires:  unixODBC-devel

%description    odbc
This package contains the ODBC back-end for %{name}, i.e.,
dynamic library specific to the ODBC connectors. If you would like to
use %{name} in your programs with ODBC, you will need to
install %{name}-odbc.}

%{?with_oracle:%package        oracle
Summary:        Oracle back-end for %{name}
Group:          System Environment/Libraries
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    oracle
This package contains the Oracle back-end for %{name}, i.e.,
dynamic library specific to the Oracle database. If you would like to
use %{name} in your programs with Oracle, you will need to install
%{name}-oracle.}


%package        devel
Summary:        Header files, libraries and development documentation for %{name}
Group:          Development/Libraries
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       pkgconfig

%description    devel
This package contains the header files, dynamic libraries and
development documentation for %{name}. If you would like to develop
programs using %{name}, you will need to install %{name}-devel.

%{?with_sqlite3:%package        sqlite3-devel
Summary:        SQLite3 back-end for %{name}
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}
Requires:       %{name}-sqlite3 = %{version}-%{release}
Requires:       sqlite-devel

%description    sqlite3-devel
This package contains the SQLite3 back-end for %{name}, i.e., header
files and dynamic libraries specific to the SQLite3 database. If you
would like to develop programs using %{name} and SQLite3, you will need
to install %{name}-sqlite3.}

%{?with_mysql:%package        mysql-devel
Summary:        MySQL back-end for %{name}
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}
Requires:       %{name}-mysql = %{version}-%{release}
Requires:       mysql-devel

%description    mysql-devel
This package contains the MySQL back-end for %{name}, i.e., header
files and dynamic libraries specific to the MySQL database. If you
would like to develop programs using %{name} and MySQL, you will need
to install %{name}-mysql.}

%{?with_postgresql:%package        postgresql-devel
Summary:        PostGreSQL back-end for %{name}
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}
Requires:       %{name}-postgresql = %{version}-%{release}
Requires:       postgresql-devel

%description    postgresql-devel
This package contains the PostGreSQL back-end for %{name}, i.e., header
files and dynamic libraries specific to the PostGreSQL database. If
you would like to develop programs using %{name} and PostGreSQL, you
will need to install %{name}-postgresql.}

%{?with_odbc:%package        odbc-devel
Summary:        ODBC back-end for %{name}
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}
Requires:       %{name}-odbc = %{version}-%{release}
Requires:       unixODBC-devel

%description    odbc-devel
This package contains the Odbc back-end for %{name}, i.e., header
files and dynamic libraries specific to the Odbc database. If you
would like to develop programs using %{name} and Odbc, you will need
to install %{name}-odbc.}

%{?with_oracle:%package        oracle-devel
Summary:        Oracle back-end for %{name}
Group:          Development/Libraries
Requires:       %{name}-devel = %{version}-%{release}
Requires:       %{name}-oracle = %{version}-%{release}

%description    oracle-devel
This package contains the Oracle back-end for %{name}, i.e., header
files and dynamic libraries specific to the Oracle database. If you
would like to develop programs using %{name} and Oracle, you will need
to install %{name}-oracle.}


%package        doc
Summary:        HTML documentation for the %{name} library
Group:          Documentation
%if 0%{?fedora} || 0%{?rhel} > 5
BuildArch:      noarch
%endif
#BuildRequires:  tex(latex)
#BuildRequires:  doxygen, ghostscript

%description    doc
This package contains the documentation in the HTML format of the %{name}
library. The documentation is the same as at the %{name} web page.


%prep
%setup -q

# Rename change-log and license file, so that they comply with
# packaging standard
mv README.md README
mv CHANGES ChangeLog
mv LICENSE_1_0.txt COPYING
echo "2013-04-13:" > NEWS
echo "- Version 3.2.1" >> NEWS
echo "- See the ChangeLog file for more details." >> NEWS


%patch -p1

%build
# Support for building tests.
%define soci_testflags -DBUILD_TESTS="NONE"
%if %{with tests}
  %define soci_testflags -DSOCI_TEST=ON \
   -DSOCI_TEST_EMPTY_CONNSTR="dummy" \
   -DSOCI_TEST_SQLITE3_CONNSTR="test.db" \
   -DSOCI_TEST_POSTGRESQL_CONNSTR:STRING="dbname=soci_test" \
   -DSOCI_TEST_MYSQL_CONNSTR:STRING="db=soci_test user=mloskot password=pantera"
%endif

mkdir tmpbuild
pushd tmpbuild
# -DCMAKE_INSTALL_PREFIX:PATH=$RPM_BUILD_ROOT
%cmake \
 -DSOCI_EMPTY=%{?with_empty:ON}%{?without_empty:OFF} \
 -DSOCI_SQLITE3=%{?with_sqlite3:ON}%{?without_sqlite3:OFF} \
 -DSOCI_POSTGRESQL=%{?with_postgresql:ON}%{?without_postgresql:OFF} \
 -DSOCI_MYSQL=%{?with_mysql:ON}%{?without_mysql:OFF} \
 -DSOCI_ODBC=%{?with_odbc:ON}%{?without_odbc:OFF} \
 -DWITH_ORACLE=%{?with_oracle:ON %{?_with_oracle_incdir} %{?_with_oracle_libdir}}%{?without_oracle:OFF} \
 -DSOCI_LIBDIR=lib -DSOCI_FIREBIRD=NO \
 %{soci_testflags} ..
make VERBOSE=1 %{?_smp_mflags}
popd

%install
rm -rf $RPM_BUILD_ROOT
pushd tmpbuild
make install DESTDIR=$RPM_BUILD_ROOT
popd
##
#  Remove unpackaged files from the buildroot
rm -f $RPM_BUILD_ROOT%{_libdir}/*.a

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%{?with_sqlite3:%post sqlite3 -p /sbin/ldconfig

%postun sqlite3 -p /sbin/ldconfig}

%{?with_mysql:%post mysql -p /sbin/ldconfig

%postun mysql -p /sbin/ldconfig}

%{?with_postgresql:%post postgresql -p /sbin/ldconfig

%postun postgresql -p /sbin/ldconfig}

%{?with_odbc:%post odbc -p /sbin/ldconfig

%postun odbc -p /sbin/ldconfig}

%{?with_oracle:%post oracle -p /sbin/ldconfig

%postun oracle -p /sbin/ldconfig}



%files
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/lib%{name}_core.so.*
%{?with_empty:%{_libdir}/lib%{name}_empty.so.*}

%{?with_sqlite3:%files sqlite3
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/lib%{name}_sqlite3.so.*}

%{?with_mysql:%files mysql
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/lib%{name}_mysql.so.*}

%{?with_postgresql:%files postgresql
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/lib%{name}_postgresql.so.*}

%{?with_odbc:%files odbc
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/lib%{name}_odbc.so.*}

%{?with_oracle:%files oracle
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%{_libdir}/lib%{name}_oracle.so.*}


%files devel
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%dir %{_includedir}/%{name}/
%{_includedir}/%{name}/*.h
%{?with_empty:%{_includedir}/%{name}/empty/}
%{_libdir}/lib%{name}_core.so
%{?with_empty:%{_libdir}/lib%{name}_empty.so}

%{?with_sqlite3:%files sqlite3-devel
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/sqlite3/
%{_libdir}/lib%{name}_sqlite3.so}

%{?with_mysql:%files mysql-devel
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/mysql
%{_libdir}/lib%{name}_mysql.so}

%{?with_postgresql:%files postgresql-devel
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/postgresql
%{_libdir}/lib%{name}_postgresql.so}

%{?with_odbc:%files odbc-devel
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/odbc/
%{_libdir}/lib%{name}_odbc.so}

%{?with_oracle:%files oracle-devel
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/oracle
%{_libdir}/lib%{name}_oracle.so}


%files doc
%defattr(-,root,root,-)
%doc AUTHORS ChangeLog COPYING NEWS README doc


%changelog
* Thu Apr 09 2015 Fedora Release Monitoring <release-monitoring@fedoraproject.org> - 3.2.3-1
- Update to 3.2.3 (#1210126)

* Tue Jan 27 2015 Petr Machata <pmachata@redhat.com> - 3.2.2-5
- Rebuild for boost 1.57.0

* Mon Aug 18 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.2.2-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sun Jun 08 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.2.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Fri May 23 2014 Petr Machata <pmachata@redhat.com> - 3.2.2-2
- Rebuild for boost 1.55.0

* Sat Oct 19 2013 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.2.2-1
- Upstream integration

* Sun Aug 04 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.2.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Tue Jul 30 2013 Petr Machata <pmachata@redhat.com> - 3.2.1-2
- Rebuild for boost 1.54.0

* Mon May 20 2013 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.2.1-1
- Upstream integration

* Fri Feb 15 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.1.0-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Sat Jul 21 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.1.0-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Tue Feb 28 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.1.0-3
- Rebuilt for c++ ABI breakage

* Sat Jan 14 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.1.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Mon Oct 31 2011 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.1.0-1
- Upstream integration
- New CMake build system

* Sat Jul 23 2011 Denis Arnaud <denis.arnaud_fedora@m4x.org> - 3.0.0-23
- Rebuild for Boost-1.47.0-2

* Sun Jul 03 2011 Denis Arnaud <denis.arnaud_fedora@m4x.org> - 3.0.0-22
- Now links with the multi-threaded versions of the Boost libraries

* Mon Apr 25 2011 Denis Arnaud <denis.arnaud_fedora@m4x.org> - 3.0.0-21
- Rebuild for Boost-1.46.1-2

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.0.0-20
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Tue Feb 08 2011 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-19
- Fixed a compilation error with g++ 4.6 on default constructor definition
- Split the big patch into smaller pieces

* Tue Sep 07 2010 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-18
- Fixed bug #631175 (https://bugzilla.redhat.com/show_bug.cgi?id=631175)

* Sat Jan 23 2010 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-16
- Added a missing cstring header include for g++-4.4 compatibility

* Fri Jan 22 2010 Rahul Sundaram <sundaram@fedoraproject.org> - 3.0.0-15
- Rebuild for Boost soname bump

* Sun Jul 26 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.0.0-14
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Sat May 09 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-13
- Introduced distinct dependencies for different distributions

* Tue May 05 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-12
- Removed the dependency on the version of Boost, and on CPPUnit

* Tue May 05 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-11
- Removed the dependency on Latex for documentation delivery

* Tue May 05 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-10
- Burried the Boost Fusion header include for core/test/common-tests.h

* Tue May 05 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-9
- Added a missing cstdio header include for g++-4.4 compatibility

* Tue May 05 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-8
- Added missing cstdio header includes for g++-4.4 compatibility

* Tue May 05 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-7
- Added a missing cstdio header include for g++-4.4 compatibility

* Sat May 02 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-6
- Removed the unused build conditionals

* Tue Apr 28 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-5
- Simplified the conditional build rules within the RPM specification file

* Sat Apr 18 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-4
- Fixed an issue about OPTFLAGS compilation

* Tue Apr 14 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-3
- Restarted from pristine version 3.0.0 of upstream (SOCI) project

* Sat Apr  4 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-2
- Specific RPM for each back-end

* Fri Mar 27 2009 Denis Arnaud <denis.arnaud_fedora@m4x.org> 3.0.0-1
- First RPM release

