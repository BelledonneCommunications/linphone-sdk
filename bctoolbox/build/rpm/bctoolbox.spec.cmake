# -*- rpm-spec -*-

## rpmbuild options
# These 2 lines are here because we can build the RPM for flexisip, in which
# case we prefix the entire installation so that we don't break compatibility
# with the user's libs.
# To compile with bc prefix, use rpmbuild -ba --with bc [SPEC]
%define                 pkg_name        %{?_with_bc:bc-bctoolbox}%{!?_with_bc:bctoolbox}
%{?_with_bc: %define    _prefix         /opt/belledonne-communications}
%define                 tests_component %{?_without_tests_component:0}%{?!_without_tests_component:1}

%define     pkg_prefix %{?_with_bc:bc-}%{!?_with_bc:}

# re-define some directories for older RPMBuild versions which don't. This messes up the doc/ dir
# taken from https://fedoraproject.org/wiki/Packaging:RPMMacros?rd=Packaging/RPMMacros
%define _datarootdir       %{_prefix}/share
%define _datadir           %{_datarootdir}
%define _docdir            %{_datadir}/doc

%define build_number @PROJECT_VERSION_BUILD@
%if %{build_number}
%define build_number_ext -%{build_number}
%endif



Name:           %{pkg_name}
Version:        @PROJECT_VERSION@
Release:        %{build_number}%{?dist}
Summary:        Belr is language recognition library for ABNF based protocols.

Group:          Applications/Communications
License:        GPL
URL:            http://www.linphone.org
Source0:        %{name}-%{version}%{?build_number_ext}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

Requires:	%{pkg_prefix}bctoolbox

%description

Toolbox package used by Belledonne Communications projects

%package devel
Summary:       Development libraries for bctoolbox
Group:         Development/Libraries
Requires:      %{name} = %{version}-%{release}

%description    devel
Libraries and headers required to develop software with bctoolbox

%if 0%{?rhel} && 0%{?rhel} <= 7
%global cmake_name cmake3
%define ctest_name ctest3
%else
%global cmake_name cmake
%define ctest_name ctest
%endif

%prep
%setup -n %{name}-%{version}%{?build_number_ext}

%build
%{expand:%%%cmake_name} . -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} -DCMAKE_PREFIX_PATH:PATH=%{_prefix} -DENABLE_TESTS=NO -DENABLE_TESTS_COMPONENT=${tests_component}
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%check
%{ctest_name} -V %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root)
%doc AUTHORS ChangeLog COPYING NEWS README.md
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root)
%{_includedir}/bctoolbox
%{_libdir}/libbctoolbox.a
%{_libdir}/libbctoolbox.so
%{_libdir}/pkgconfig/bctoolbox.pc
%{_datadir}/bctoolbox/cmake/BcGitVersion.cmake
%{_datadir}/bctoolbox/cmake/BcToolboxCMakeUtils.cmake
%{_datadir}/bctoolbox/cmake/BcToolboxConfig.cmake
%{_datadir}/bctoolbox/cmake/BcToolboxConfigVersion.cmake
%{_datadir}/bctoolbox/cmake/BcToolboxTargets-noconfig.cmake
%{_datadir}/bctoolbox/cmake/BcToolboxTargets.cmake
%{_datadir}/bctoolbox/cmake/gitversion.h.in
%if %{tests_component}
%{_libdir}/libbctoolbox-tester.a
%{_libdir}/libbctoolbox-tester.so
%{_libdir}/pkgconfig/bctoolbox-tester.pc
%endif

%changelog
* Tue Jan 16 2018 Ghislain MARY <ghislain.mary@belledonne-communications.com>
- Initial RPM release.
