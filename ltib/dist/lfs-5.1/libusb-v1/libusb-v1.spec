%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : A newer library which allows userspace access to USB devices 
Name            : libusb
Version         : 1.0.8
Release         : 1
License         : LGPL
Vendor          : MBARI
Packager        : Brent Roman
Group           : System Environment/Libraries
Source          : %{name}-%{version}.tar.gz
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup 
libtoolize --copy --force

%Build
# configure can't figure this out in my environment (cross)
ENDIAN=${ENDIAN:-big}
case $ENDIAN in 
   big) conf_opts="ac_cv_c_bigendian=yes" ;;
   *)   conf_opts="ac_cv_c_bigendian=no"  ;;
esac

./configure --prefix=%{_prefix} --host=$CFGHOST --build=%{_build} --disable-build-docs  $conf_opts
make

%Install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT/%{pfx}
rm -f $RPM_BUILD_ROOT/%{pfx}/%{_prefix}/lib/*.la

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*
