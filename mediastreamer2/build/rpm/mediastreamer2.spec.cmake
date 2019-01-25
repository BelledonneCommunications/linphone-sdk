# -*- rpm-spec -*-

%define _prefix    @CMAKE_INSTALL_PREFIX@
%define pkg_prefix @BC_PACKAGE_NAME_PREFIX@

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
Release:        %build_number%{?dist}
Summary:         Audio/Video real-time streaming

Group:          Applications/Communications
License:        GPL
URL:            http://www.mediastreamer.org
Source0:        %{name}-%{version}%{?build_number_ext}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

Requires:	%{pkg_prefix}bctoolbox
Requires:	%{pkg_prefix}ortp

%description
Mediastreamer2 is a GPL licensed library to make audio and video
real-time streaming and processing. Written in pure C, it is based
upon the oRTP library.


BuildRequires:

%package devel
Summary:       Development libraries for mediastreamer
Group:         Development/Libraries
Requires:      %{name} = %{version}-%{release}

%description    devel
This package contains header files and development libraries needed to
develop programs using the mediastreamer2 library.

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
%setup -n %{name}-%{version}%{?build_number_ext}

%build
%{expand:%%%cmake_name} . -DCMAKE_BUILD_TYPE=@CMAKE_BUILD_TYPE@ -DCMAKE_PREFIX_PATH:PATH=%{_prefix} @RPM_ALL_CMAKE_OPTIONS@
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

# Dirty workaround to give exec rights for all shared libraries. Debian packaging needs this
# TODO : set CMAKE_INSTALL_SO_NO_EXE for a cleaner workaround
chmod +x `find %{buildroot} *.so.*`


%check
#%{ctest_name} -V %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%if @ENABLE_TOOLS@ || @ENABLE_UNIT_TESTS@
%{_bindir}/*
%endif
%{_libdir}/*.so.*
%if @ENABLE_VIDEO@
%{_datadir}/images/nowebcamCIF.jpg
%endif

%files devel
%defattr(-,root,root,-)
%if @ENABLE_STATIC@
%{_libdir}/*.a
%endif
%if @ENABLE_SHARED@
%{_libdir}/*.so
%endif
#%{_libdir}/pkgconfig/*.pc
%{_includedir}
%{_datadir}/Mediastreamer2/cmake/Mediastreamer2Config*.cmake
%{_datadir}/Mediastreamer2/cmake/Mediastreamer2Targets*.cmake
%if @ENABLE_DOC@
%doc %{_docdir}/*
%endif

%changelog

* Tue Nov 27 2018 ronan.abhamon <ronan.abhamon@belledonne-communications.com>
- Do not set CMAKE_INSTALL_LIBDIR.

* Thu Jul 13 2017 jehan.monnier <jehan.monnier@linphone.org>
- cmake port

* Mon Aug 19 2013 jehan.monnier <jehan.monnier@linphone.org>
- Initial RPM release.
