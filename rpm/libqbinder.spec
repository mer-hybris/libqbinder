Name:       libqbinder

Summary:    Qt integration for libgbinder
Version:    1.0.2
Release:    1
License:    BSD
URL:        https://github.com/mer-hybris/libqbinder
Source0:    %{name}-%{version}.tar.bz2

%define libgbinder_version 1.0.40

Requires:       libgbinder >= %{libgbinder_version}
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(libgbinder) >= %{libgbinder_version}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}

%description
Qt integration for libgbinder

%package devel
Summary:    Development files for %{name}
Requires:   %{name} = %{version}-%{release}
Requires:   %{name} = %{version}

%description devel
This package contains the development header files for %{name}

%prep
%setup -q -n %{name}-%{version}

%build
%qtc_qmake5
%qtc_make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%license LICENSE
%{_libdir}/%{name}.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/%{name}.so
%{_libdir}/pkgconfig/qbinder.pc
%{_includedir}/qbinder/*.h
