# -*- rpm-spec -*-

## rpmbuild options
# These 2 lines are here because we can build the RPM for flexisip, in which
# case we prefix the entire installation so that we don't break compatibility
# with the user's libs.
# To compile with bc prefix, use rpmbuild -ba --with bc [SPEC]
%define                 pkg_name        %{?_with_bc:bc-belle-sip}%{!?_with_bc:belle-sip}
%{?_with_bc: %define    _prefix         /opt/belledonne-communications}
%define                 srtp            %{?_without_srtp:0}%{?!_without_srtp:1}

# re-define some directories for older RPMBuild versions which don't. This messes up the doc/ dir
# taken from https://fedoraproject.org/wiki/Packaging:RPMMacros?rd=Packaging/RPMMacros
%define _datarootdir       %{_prefix}/share
%define _datadir           %{_datarootdir}
%define _docdir            %{_datadir}/doc

%define build_number @PROJECT_VERSION_BUILD@



Name:           %{pkg_name}
Version:        @PROJECT_VERSION@
Release:        %build_number%{?dist}
Summary:        Linphone's sip stack

Group:          Applications/Communications
License:        GPL
URL:            http://www.belle-sip.org
Source0:        %{name}-%{version}-%{build_number}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot
%description
Belle-sip is an object oriented SIP stack, written in C, used by Linphone.


BuildRequires: antlr3-tool antlr3-C-devel

%package devel
Summary:       Development libraries for belle-sip
Group:         Development/Libraries
Requires:      %{name} = %{version}-%{release}

%description    devel
Libraries and headers required to develop software with belle-sip

%if 0%{?rhel} && 0%{?rhel} <= 7
%global cmake_name cmake3
%define ctest_name ctest3
%else
%global cmake_name cmake
%define ctest_name ctest
%endif

%prep
%setup -n %{name}-%{version}-%build_number

%build
%{expand:%%%cmake_name} . -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} -DCMAKE_PREFIX_PATH:PATH=%{_prefix}
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%check
#%{ctest_name} -V %{?_smp_mflags}

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
%{_includedir}/belle-sip
%{_libdir}/libbellesip.a
%{_libdir}/libbellesip.so
%{_libdir}/pkgconfig/belle-sip.pc
%{_datadir}/BelleSIP/cmake/BelleSIPConfig.cmake
%{_datadir}/BelleSIP/cmake/BelleSIPConfigVersion.cmake
%{_datadir}/BelleSIP/cmake/BelleSIPTargets-noconfig.cmake
%{_datadir}/BelleSIP/cmake/BelleSIPTargets.cmake
%{_bindir}/*

%changelog
* Mon Aug 19 2013 jehan.monnier <jehan.monnier@linphone.org>
- Initial RPM release.
