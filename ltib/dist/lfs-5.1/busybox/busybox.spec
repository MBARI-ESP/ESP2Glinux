%define base %(echo %{_prefix} | sed -e s,/usr.*$,,)
%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary         : A small executable that replaces many UNIX utilities
Name            : busybox
Version         : 1.11.2
Release         : 1
License         : GPL
Vendor          : Freescale
Packager        : Steve Papacharalambous/Stuart Hughes
Group           : System Environment/Shells
Source          : %{name}-%{version}.tar.bz2
Patch1          : busybox-1.11.2-getty-nobaud-1.patch
Patch3          : busybox-1.11.2-loop-24-compat.patch
Patch4          : busybox-1.11.2-msh-signals.patch
Patch5          : busybox-1.11.2-msh-alias-1.patch
Patch6          : busybox-1.11.2-makefile-implicit-rules.patch
Patch7          : busybox-1.11.2-mbari-1.patch
Patch8          : busybox-1.11.2-dhcpc-maxlease.patch
Patch9          : busybox-1.11.2-mbari-2.patch
Patch10         : busybox-1.11.2-leasefail.patch
BuildRoot       : %{_tmppath}/%{name}
Prefix          : %{pfx}

%Description
%{summary}

%Prep
%setup
%patch1 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1

%Build
PKG_BUSYBOX_PRECONFIG=${PKG_BUSYBOX_PRECONFIG:-busybox.config}
if [ -f "$PLATFORM_PATH/${PKG_BUSYBOX_PRECONFIG}" ]
then
    cp $PLATFORM_PATH/$PKG_BUSYBOX_PRECONFIG .config
else
    if [ -f "$CONFIG_DIR/defaults/$PKG_BUSYBOX_PRECONFIG" ]
    then
        cp "$CONFIG_DIR/defaults/$PKG_BUSYBOX_PRECONFIG"  .config
    fi
fi
if [ -n "$PKG_BUSYBOX_WANT_CF" -o -n "$SCB_WANT_CF" ]
then
    make menuconfig HOSTCC="$BUILDCC" &&
    cp .config $PLATFORM_PATH/$PKG_BUSYBOX_PRECONFIG
else
    yes "" | make config HOSTCC="$BUILDCC"
fi
if [ "$GNUTARCH" = m68knommu ]
then
    MAKE_EXTRA="SKIP_STRIP=y"
fi
make HOSTCC="$BUILDCC" $MAKE_EXTRA

%Install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/%{pfx}/%{base}/bin
make HOSTCC="$BUILDCC" CONFIG_PREFIX=$RPM_BUILD_ROOT/%{pfx}/%{base} install
for i in pidof ip
do
    
    if [ -f $RPM_BUILD_ROOT/%{pfx}/%{base}/bin/$i ]
    then
        rm $RPM_BUILD_ROOT/%{pfx}/%{base}/bin/$i
        ln -sf ../bin/busybox $RPM_BUILD_ROOT/%{pfx}/%{base}/sbin/$i
    fi
done
mkdir -p $RPM_BUILD_ROOT/%{pfx}/%{base}/etc/rc.d/init.d
for i in run log
do
    install -d $RPM_BUILD_ROOT/%{pfx}/%{base}/var/$i
done
touch $RPM_BUILD_ROOT/%{pfx}/%{base}/var/run/utmp
touch $RPM_BUILD_ROOT/%{pfx}/%{base}/var/log/wtmp
cat <<EOF > $RPM_BUILD_ROOT/%{pfx}/%{base}/etc/busybox.conf
[SUID]
su = ssx root.root
passwd = ssx root.root
EOF
chmod 644 $RPM_BUILD_ROOT/%{pfx}/%{base}/etc/busybox.conf

%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%attr(4755,root,root) %{pfx}/%{base}/bin/busybox
%{pfx}/*

%changelog
* Thu Apr 23 2020 Brent Roman <brent@mbari.org>
- vi no longer crashes if one has no previous regular expression
* Thu Apr 28 2011 Brent Roman <brent@mbari.org>
- added --maxlease option to dhcpcd and made it handle aliased interfaces
* Thu Jul 21 2005 Stuart Hughes <stuarth@freescale.com>
- changed syslogd-p patch to be unconditional
