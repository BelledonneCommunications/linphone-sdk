Name:           odb
Version:        2.3.0
Release:        3%{?dist}
Summary:        Object-relational mapping (ORM) system for C++

Group:          Development/Tools
License:        GPLv3
URL:            http://www.codesynthesis.com/products/odb/
Source0:        http://www.codesynthesis.com/download/odb/2.3/%{name}-%{version}.tar.bz2

# Set BuildRoot for compatibility with EPEL <= 5
# See: http://fedoraproject.org/wiki/EPEL:Packaging#BuildRoot_tag
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# If building on Fedora or RHEL 7
%if 0%{?rhel}%{?fedora} >= 7
# Then it just needs to be at least gcc 4.5.0 for plugin support
BuildRequires: gcc-c++ >= 4.5.0
BuildRequires: gcc-plugin-devel
%else
# Otherwise, use devtoolset on RHEL 5/6 because it supports plugins
BuildRequires: devtoolset-1.1-binutils
BuildRequires: devtoolset-1.1-gcc-c++
BuildRequires: devtoolset-1.1-gcc-plugin-devel
# Also list gmp-devel since devtoolset-gcc-plugin-devel should list it as a requirement but doesn't
# See: https://bugzilla.redhat.com/show_bug.cgi?id=908577
BuildRequires: gmp-devel
%endif
# Uses libcutl from Code Synthesis
BuildRequires: libcutl-devel >= 1.8.0
# Uses pkgconfig
BuildRequires: pkgconfig
# Uses expat
BuildRequires: expat-devel


# If building on Fedora or RHEL 7
%if 0%{?rhel}%{?fedora} >= 7
  #Then odb uses the gcc plugin directory
  %define ODB_PLUGIN_DIR %(g++ -print-file-name=plugin)
%else
  # Otherwise, it just uses the system directory
  %define ODB_PLUGIN_DIR %{_libexecdir}/odb/
%endif


%description
ODB is an object-relational mapping (ORM) system for C++. It provides
tools, APIs, and library support that allow you to persist C++ objects
to a relational database (RDBMS) without having to deal with tables,
columns, or SQL and without manually writing any of the mapping code.


%prep
%setup -q
# Set the path to the default.options file
#define
%define odb_default_options_dir %{_sysconfdir}/%{name}
%define odb_default_options_file %{odb_default_options_dir}/default.options


%build
# If building on Fedora or RHEL 7
%if 0%{?rhel}%{?fedora} >= 7
# Then do standard build
%configure --disable-static --with-options-file=%{odb_default_options_file}
%else
# Otherwise, use devtoolset on RHEL 5/6 to get a version of gcc that supports plugins
source /opt/rh/devtoolset-1.1/enable
# Add the explicit linking of pthread library because of a toolchain issue
# See: http://www.codesynthesis.com/pipermail/odb-users/2013-February/001103.html
%configure --disable-static LIBS="-lpthread" --with-options-file=%{odb_default_options_file}
%endif
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

# Add the .conf file
mkdir -p $RPM_BUILD_ROOT/%{odb_default_options_dir}
echo "# Default ODB options file. This file is automatically loaded by the ODB
# compiler and can be used for installation-wide customizations, such as
# adding an include search path for a commonly used library. For example:
# -I %{_includedir}/boost141
#" > $RPM_BUILD_ROOT/%{odb_default_options_file}


%clean
rm -rf $RPM_BUILD_ROOT


%files
%config(noreplace) %{odb_default_options_file}
%doc GPLv3
%doc LICENSE
%doc NEWS
%doc doc/default.css
%doc doc/manual.xhtml
%doc doc/odb-arch.png
%doc doc/odb-flow.png
%doc doc/odb-manual.pdf
%doc doc/odb-manual.ps
%doc doc/odb.xhtml
%{_bindir}/odb
%{ODB_PLUGIN_DIR}
%{_mandir}/man1/odb.1
# Exclude the documentation that doesn't need to be packaged
%exclude %{_datadir}/doc/odb/GPLv3
%exclude %{_datadir}/doc/odb/LICENSE
%exclude %{_datadir}/doc/odb/NEWS
%exclude %{_datadir}/doc/odb/README
%exclude %{_datadir}/doc/odb/default.css
%exclude %{_datadir}/doc/odb/manual.xhtml
%exclude %{_datadir}/doc/odb/odb-arch.png
%exclude %{_datadir}/doc/odb/odb-flow.png
%exclude %{_datadir}/doc/odb/odb-manual.pdf
%exclude %{_datadir}/doc/odb/odb-manual.ps
%exclude %{_datadir}/doc/odb/odb.xhtml
%exclude %{_datadir}/doc/odb/version


%changelog
* Wed Jul 2 2014 Dave Johansen <davejohansen@gmail.com> 2.3.0-3
- Rebuild for gcc 4.8.3
* Wed May 28 2014 Dave Johansen <davejohansen@gmail.com> 2.3.0-2
- Adding expat-devel as BuildRequires

* Mon Nov 4 2013 Dave Johansen <davejohansen@gmail.com> 2.3.0-1
- Updated to 2.3.0

* Tue Jul 23 2013 Dave Johansen <davejohansen@gmail.com> 2.2.2-1
- Initial build
