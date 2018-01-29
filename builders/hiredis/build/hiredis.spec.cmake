# -*- rpm-spec -*-

%define _prefix    @CMAKE_INSTALL_PREFIX@

# re-define some directories for older RPMBuild versions which don't. This messes up the doc/ dir
# taken from https://fedoraproject.org/wiki/Packaging:RPMMacros?rd=Packaging/RPMMacros
%define _datarootdir       %{_prefix}/share
%define _datadir           %{_datarootdir}
%define _docdir            %{_datadir}/doc

%define build_number @PROJECT_VERSION_BUILD@
%if %{build_number}
%define build_number_ext -%{build_number}
%endif

Name:           @CPACK_PACKAGE_NAME@
Version:        @PROJECT_VERSION@
Release:        %{build_number}%{?dist}
Summary:        Minimalistic C client library for Redis
License:        BSD
URL:            https://github.com/redis/hiredis
Source0:        %{name}-%{version}%{?build_number_ext}.tar.gz

%description 
Hiredis is a minimalistic C client library for the Redis database.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
This package contains libraries and header files for
developing applications that use %{name}.

%if 0%{?rhel} && 0%{?rhel} <= 7
%global cmake_name cmake3
%define ctest_name ctest3
%else
%global cmake_name cmake
%define ctest_name ctest
%endif

# This is for debian builds where debug_package has to be manually specified, whereas in centos it does not
%define custom_debug_package %{!?_enable_debug_packages:%debug_package}%{?_enable_debug_package:%{nil}}
%custom_debug_package

%prep
%setup -q -n %{name}-%{version}%{?build_number_ext}

%build
%{expand:%%%cmake_name} . -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} -DCMAKE_PREFIX_PATH:PATH=%{_prefix} @RPM_ALL_CMAKE_OPTIONS@
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%doc COPYING
%{_libdir}/libhiredis.so.*

%files devel
%doc README.md
%{_includedir}/hiredis/
%if @ENABLE_STATIC@
%{_libdir}/libhiredis.a
%endif
%if @ENABLE_SHARED@
%{_libdir}/libhiredis.so
%endif

%changelog
* Wed Sep 16 2015 Sylvain Berfini <sylvain.berfini@belledonne-communications.com> - 0.13.3-1
- Version bump from 0.11.0 to 0.13.3

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.11.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.11.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Sat Sep 29 2012 Shakthi Kannan <shakthimaan [AT] fedoraproject dot org> 0.11.0-1
- Updated to 0.11.0

* Thu Jul 19 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.10.1-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Fri Jan 20 2012 Shakthi Kannan <shakthimaan [AT] fedoraproject dot org> 0.10.1-3
- Removed Requires redis.

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.10.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Fri Dec 30 2011 Shakthi Kannan <shakthimaan [AT] fedoraproject dot org> 0.10.1-1
- Updated to upstream 0.10.1-28-gd5d8843.

* Mon May 16 2011 Shakthi Kannan <shakthimaan [AT] fedoraproject dot org> 0.10.0-3
- Removed INSTALL_LIB from install target as we use INSTALL_LIBRARY_PATH.
- Use 'client library' in Summary.

* Wed May 11 2011 Shakthi Kannan <shakthimaan [AT] fedoraproject dot org> 0.10.0-2
- Updated devel sub-package description.
- Added optimization flags.
- Remove manual installation of shared objects.
- Use upstream .tar.gz sources.

* Tue May 10 2011 Shakthi Kannan <shakthimaan [AT] fedoraproject dot org> 0.10.0-1.gitdf203bc328
- Updated to upstream gitdf203bc328.
- Added TODO to the files.
- Updated to use libhiredis.so.0, libhiredis.so.0.10.
