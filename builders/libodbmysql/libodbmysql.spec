Name:           libodb-mysql
Version:        2.3.0
Release:        1%{?dist}
Summary:        MySQL ODB runtime library from Code Synthesis

Group:          System Environment/Libraries
License:        GPLv2
URL:            http://www.codesynthesis.com/products/odb/
Source0:        http://www.codesynthesis.com/download/odb/2.3/%{name}-%{version}.tar.bz2

# Set BuildRoot for compatibility with EPEL <= 5
# See: http://fedoraproject.org/wiki/EPEL:Packaging#BuildRoot_tag
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# Uses libodb from Code Synthesis 
BuildRequires: libodb-devel >= 2.3.0
# Uses MySQL
BuildRequires: mysql-devel
# Uses pkgconfig
BuildRequires: pkgconfig


%description
ODB is an object-relational mapping (ORM) system for C++. It provides
tools, APIs, and library support that allow you to persist C++ objects
to a relational database (RDBMS) without having to deal with tables,
columns, or SQL and without manually writing any of the mapping code.

This package contains the MySQL ODB runtime library. Every application
that includes code generated for the MySQL database will need to link
to this library.


%package        devel
Summary:        Development files for %{name}
Group:          Development/Libraries
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%prep
%setup -q


%build
%configure --disable-static
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'


%clean
rm -rf $RPM_BUILD_ROOT


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%doc GPLv2
%doc LICENSE
%{_libdir}/libodb-mysql-2.3.so
# Exclude the documentation that doesn't need to be packaged
%exclude %{_datadir}/doc/libodb-mysql/GPLv2
%exclude %{_datadir}/doc/libodb-mysql/LICENSE
%exclude %{_datadir}/doc/libodb-mysql/NEWS
%exclude %{_datadir}/doc/libodb-mysql/README
%exclude %{_datadir}/doc/libodb-mysql/version

%files devel
%doc NEWS
# odb folder is created/owned by libodb package
%{_includedir}/odb/*
%{_libdir}/libodb-mysql.so
%{_libdir}/pkgconfig/libodb-mysql.pc


%changelog
* Mon Nov 4 2013 Dave Johansen <davejohansen@gmail.com> 2.3.0-1
- Updated to 2.3.0

* Tue Jul 23 2013 Dave Johansen <davejohansen@gmail.com> 2.2.0-1
- Initial build
