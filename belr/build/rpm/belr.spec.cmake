# -*- rpm-spec -*-

%define _prefix    @CMAKE_INSTALL_PREFIX@
%define pkg_prefix @BC_PACKAGE_NAME_PREFIX@
%define package_name @CPACK_PACKAGE_NAME@-${FULL_VERSION}

# re-define some directories for older RPMBuild versions which don't. This messes up the doc/ dir
# taken from https://fedoraproject.org/wiki/Packaging:RPMMacros?rd=Packaging/RPMMacros
%define _datarootdir       %{_prefix}/share
%define _datadir           %{_datarootdir}
%define _docdir            %{_datadir}/doc


Name:           @CPACK_PACKAGE_NAME@
Version:        ${RPM_VERSION}
Release:        ${RPM_RELEASE}%{?dist}
Summary:        Belr is language recognition library for ABNF based protocols.

Group:          Applications/Communications
License:        GPL
URL:            http://www.linphone.org
Source0:        %{package_name}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

Requires:	%{pkg_prefix}bctoolbox

%description

Belr is Belledonne Communications' language recognition library.
It aims at parsing any input formatted according to a language defined by an ABNF grammar,
such as the protocols standardized at IETF.

It is based on finite state machine theory and heavily relies on recursivity from an implementation standpoint.


%package devel
Summary:       Development libraries for belr
Group:         Development/Libraries
Requires:      %{name} = %{version}-%{release}

%description    devel
Libraries and headers required to develop software with belr

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
%setup -n %{package_name}

%build
%{expand:%%%cmake_name} . -DCMAKE_BUILD_TYPE=@CMAKE_BUILD_TYPE@ -DCMAKE_PREFIX_PATH:PATH=%{_prefix} @RPM_ALL_CMAKE_OPTIONS@
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

# Dirty workaround to give exec rights for all shared libraries. Debian packaging needs this
# TODO : set CMAKE_INSTALL_SO_NO_EXE for a cleaner workaround
chmod +x `find %{buildroot} *.so.*`


%check
%{ctest_name} -V %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root)
%doc CHANGELOG.md LICENSE.txt README.md
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root)
%{_includedir}/belr
%if @ENABLE_STATIC@
%{_libdir}/libbelr.a
%endif
%if @ENABLE_SHARED@
%{_libdir}/libbelr.so
%endif
%{_libdir}/cmake/belr/belrConfig*.cmake
%{_libdir}/cmake/belr/belrTargets*.cmake
%if @ENABLE_TESTS@ || @ENABLE_TOOLS@
%{_bindir}/*
%endif

%changelog

* Tue Nov 27 2018 ronan.abhamon <ronan.abhamon@belledonne-communications.com>
- Do not set CMAKE_INSTALL_LIBDIR and never with _libdir!

* Wed Jul 19 2017 jehan.monnier <jehan.monnier@linphone.org>
- Initial RPM release.
