%define pfx /opt/freescale/rootfs/%{_target_cpu}

Summary		: Userspace device files
Name		: udev
Version		: 117
Release		: 2
License		: GPL
Vendor		: Freescale
Packager	: Duck
Group		: System Environment/Base
Source		: %{name}-%{version}.tar.bz2
Patch0		: udev-117-init-script.patch
Patch1		: udev-imx-input-rules-3.patch
Patch2		: udev-automount-v3.patch
Patch3		: udev-117-init_script.patch
Patch4		: udev-117-mount.patch
BuildRoot	: %{_tmppath}/%{name}
Prefix		: %{pfx}

# List of extra things to build, which must be
# passed to make at build time and install time.
# Use an rpm variable, so we only need to define
# it once.
%define extras "extras/volume_id extras/path_id extras/usb_id extras/firmware"

%Description
%{summary}

%Prep
%setup
%patch0 -p1
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1

%Build
make EXTRAS=%{extras}

%Install
rm -rf $RPM_BUILD_ROOT
make install EXTRAS=%{extras} DESTDIR=$RPM_BUILD_ROOT/%{pfx}
install -m 644 etc/udev/packages/40-alsa.rules $RPM_BUILD_ROOT/%{pfx}/etc/udev/rules.d

# To avoid duplication removed as skell installs this and all the
# other init scripts.
#install -d $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d
#install -m 744 etc/udev/ltib/init_script $RPM_BUILD_ROOT/%{pfx}/etc/rc.d/init.d/udev

if [ "$PKG_UDEV_WANT_IMX" = "y" ]; then
	install -m 644 etc/udev/ltib/??-imx*.rules $RPM_BUILD_ROOT/%{pfx}/etc/udev/rules.d
fi

if [ "$PKG_UDEV_WANT_AUTOMOUNT" = "y" ]; then
	install -m 644 etc/udev/ltib/65-automount.rules $RPM_BUILD_ROOT/%{pfx}/etc/udev/rules.d
fi

if [ "$PKG_UDEV_WANT_NON_RAID" = "y" ]; then
	# For non-raid systems, add the '--skip-raid' arg to vol_id cmds
	# in all udev rules.
	# This prevents vol_id from trying to read the last sector of a device
	# for raid info, which is problematic for devices which report their
	# capacity incorrectly.

	perl -pi -e 's/vol_id /vol_id --skip-raid /' \
		$RPM_BUILD_ROOT/%{pfx}/etc/udev/rules.d/*
fi


%Clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(-,root,root)
%{pfx}/*
