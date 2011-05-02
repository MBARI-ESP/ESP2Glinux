%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : NFS daemon and tools
Name            : nfs-utils
Version         : 1.1.0
Release         : 1
License         : GPL
Vendor          : Freescale
Packager        : Stuart Hughes
Group           : System Environment/Daemons
URL             : http://nfs.sourceforge.net/
Source          : %{name}-%{version}.tar.bz2
Patch0          : 001-no-getgrouplist.patch
Patch1          : nfs-utils-1.1.0-dont-chmod-mtab.patch
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup 
%patch0 -p1
%patch1 -p1

%Build

./configure --prefix=%{_prefix} --host=$CFGHOST --build=%{_build} \
            --disable-nfsv4 --disable-uuid --disable-gss
make

%Install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT/%{pfx}

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%attr(744,root,root)  %{pfx}/var/lib/nfs/sm
%attr(744,root,root)  %{pfx}/var/lib/nfs/sm.bak
%attr(644,root,root)  %{pfx}/var/lib/nfs/state 
%attr(4555,root,root) %{pfx}/sbin/mount.nfs
%{pfx}/*
