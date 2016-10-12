

%define                 pkg_name        hiredis
%define                 finalpkg_name   %{?_with_bc:bc-%{pkg_name}}%{!?_with_bc:%{pkg_name}}
%{?_with_bc: %define    _prefix         /opt/belledonne-communications}
# re-define some directories for older RPMBuild versions which don't. This messes up the doc/ dir
# taken from https://fedoraproject.org/wiki/Packaging:RPMMacros?rd=Packaging/RPMMacros
%define _datarootdir       %{_prefix}/share
%define _datadir           %{_datarootdir}
%define _docdir            %{_datadir}/doc


Name:           %{finalpkg_name}
Version:        0.13.3
Release:        3%{?dist}
Summary:        Minimalistic C client library for Redis
License:        BSD
URL:            https://github.com/redis/hiredis
Source0:        v%{version}.tar.gz

%description 
Hiredis is a minimalistic C client library for the Redis database.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
This package contains libraries and header files for
developing applications that use %{name}.

%prep
%setup -q -n hiredis-%{version}

%build
make %{?_smp_mflags} OPTIMIZATION="%{optflags}"

%install
make install PREFIX=%{buildroot}%{_prefix} INSTALL_LIBRARY_PATH=%{buildroot}%{_libdir}

mkdir -p %{buildroot}%{_bindir}
cp hiredis-test    %{buildroot}%{_bindir}

find %{buildroot} -name '*.a' -delete -print

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%doc COPYING
%{_bindir}/hiredis-test
%{_libdir}/libhiredis.so.0.13
%{_libdir}/pkgconfig/hiredis.pc

%files devel
%doc README.md
%{_includedir}/%{pkg_name}/
%{_libdir}/libhiredis.so

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
