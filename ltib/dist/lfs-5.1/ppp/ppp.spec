%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : Point-to-Point Protocol daemon
Name            : ppp
Version         : 2.4.4
Release         : 1
License         : BSD-like and GPL and Public Domain
Vendor          : Freescale
Packager        : Michael Barkowski
Group           : System Environment/Daemons
Source          : %{name}-%{version}.tar.gz
Patch0          : ppp-2.4.4-header-checks-filter-off-df546d4c.patch
Patch1          : ppp-2.4.3-hspeed.patch
Patch2          : ppp-2.4.4-dont-create-resolv.patch
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup 
%patch0 -p1
%patch1 -p1
%patch2 -p1

%Build
./configure --prefix=%{_prefix} --host=$CFGHOST --build=%{_build} --mandir=%{_mandir}
if [ "$PKG_PPP_WANT_FILTER" = "y" ]
then
    echo "Including support for PPP packet filtering.  Requires libpcap and kernel driver support."
    make -j1 FILTER=y
else
    make -j1
fi


%Install
rm -rf $RPM_BUILD_ROOT
make -j1 install INSTALL=install DESTDIR=$RPM_BUILD_ROOT/%{pfx}
mkdir -p $RPM_BUILD_ROOT/%{pfx}/var/lock

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%attr(4554,root,root)  %{pfx}//lib/pppd/2.4.4/pppoatm.so
%attr(4554,root,root)  %{pfx}//lib/pppd/2.4.4/rp-pppoe.so
%{pfx}/*
